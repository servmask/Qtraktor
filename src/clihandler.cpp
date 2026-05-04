#include "clihandler.h"
#include "backupfile.h"
#include "cryptoutils.h"
#include "installcli.h"
#include "mcpserver.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <cstdio>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#endif

// Shared setup: open archive, validate, detect encryption/compression.
// Returns exit code (0 = ok).
static int openAndSetup(const QString &archivePath, const QString &password, BackupFile *&outFile)
{
    QFileInfo fi(archivePath);
    if (!fi.isReadable()) {
        fprintf(stderr, "Error: cannot read file: %s\n", archivePath.toLocal8Bit().constData());
        return 1;
    }

    outFile = new BackupFile(archivePath, password);
    if (!outFile->open(QIODevice::ReadOnly)) {
        fprintf(stderr, "Error: cannot open file: %s\n", archivePath.toLocal8Bit().constData());
        delete outFile;
        outFile = nullptr;
        return 1;
    }

    if (!outFile->isValid()) {
        fprintf(stderr, "Error: backup file is corrupted or not a valid .wpress archive\n");
        outFile->close();
        delete outFile;
        outFile = nullptr;
        return 2;
    }

    outFile->ensureConfigLoaded();

    if (outFile->isEncryptedFile() && password.isEmpty()) {
        fprintf(stderr, "Error: backup is encrypted - provide a password with -p or TRAKTOR_PASSWORD\n");
        outFile->close();
        delete outFile;
        outFile = nullptr;
        return 3;
    }

    outFile->setConfig(outFile->isEncryptedFile(), outFile->getCompressionType());
    return 0;
}

// Parse common flags for a subcommand
static void addCommonOptions(QCommandLineParser &parser)
{
    parser.addHelpOption();

    QStringList pwNames;
    pwNames << "p" << "password";
    parser.addOption(QCommandLineOption(pwNames, "Password for encrypted backup", "password"));

    parser.addOption(QCommandLineOption("json", "Machine-readable JSON output"));

    QStringList quietNames;
    quietNames << "q" << "quiet";
    parser.addOption(QCommandLineOption(quietNames, "Suppress progress output"));
}

// Get password from flag or environment
static QString resolvePassword(const QCommandLineParser &parser)
{
    QString pw = parser.value("password");
    if (pw.isEmpty()) {
        pw = qEnvironmentVariable("TRAKTOR_PASSWORD");
    }
    return pw;
}

// Get positional arguments, skipping the subcommand name (argv[1]).
// QCommandLineParser sees the subcommand as the first positional arg
// because it doesn't know about subcommands. We strip it here.
static QStringList positionalArgs(const QCommandLineParser &parser)
{
    QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        args.removeFirst();
    }
    return args;
}

static void printProgressStderr(float percent)
{
    int filled = static_cast<int>(percent / 5.0f);
    fprintf(stderr, "\r[");
    for (int i = 0; i < 20; i++)
        fputc(i < filled ? '#' : ' ', stderr);
    fprintf(stderr, "] %3d%%", static_cast<int>(percent));
    fflush(stderr);
}

// ── list ────────────────────────────────────────────────────────────────────

int cmdList(int /*argc*/, char * /*argv*/[])
{
    QCommandLineParser parser;
    parser.setApplicationDescription("List contents of a .wpress archive");
    addCommonOptions(parser);
    parser.addPositionalArgument("archive", "Archive file (.wpress)");
    parser.process(*QCoreApplication::instance());

    const QStringList args = positionalArgs(parser);
    if (args.isEmpty()) {
        fprintf(stderr, "Error: missing archive argument\nUsage: traktor list [options] <archive>\n");
        return 1;
    }

    const QString password = resolvePassword(parser);
    const bool jsonMode = parser.isSet("json");

    BackupFile *bf = nullptr;
    int rc = openAndSetup(args.first(), password, bf);
    if (rc != 0)
        return rc;

    if (!jsonMode) {
        fprintf(stdout, "PATH\tSIZE\tMTIME\n");
    }

    bool ok = bf->iterateHeaders([&](const BackupFile::HeaderInfo &info) {
        const QString path = BackupFile::normalizePath(info.filePath, info.fileName);

        if (jsonMode) {
            QJsonObject obj;
            obj["path"] = path;
            obj["size"] = info.fileSize;
            obj["mtime"] = info.mtime.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(info.mtime);
            if (!info.crc32.isEmpty()) {
                obj["crc32"] = info.crc32;
            }
            fprintf(stdout, "%s\n", QJsonDocument(obj).toJson(QJsonDocument::Compact).constData());
        } else {
            fprintf(stdout, "%s\t%lld\t%s\n", path.toLocal8Bit().constData(), info.fileSize,
                    info.mtime.isEmpty() ? "-" : info.mtime.toLocal8Bit().constData());
        }
        fflush(stdout);
        return true;
    });

    bf->close();
    delete bf;
    return ok ? 0 : 2;
}

// ── info ────────────────────────────────────────────────────────────────────

int cmdInfo(int /*argc*/, char * /*argv*/[])
{
    QCommandLineParser parser;
    parser.setApplicationDescription("Show metadata of a .wpress archive");
    addCommonOptions(parser);
    parser.addPositionalArgument("archive", "Archive file (.wpress)");
    parser.process(*QCoreApplication::instance());

    const QStringList args = positionalArgs(parser);
    if (args.isEmpty()) {
        fprintf(stderr, "Error: missing archive argument\nUsage: traktor info [options] <archive>\n");
        return 1;
    }

    const QString password = resolvePassword(parser);
    const bool jsonMode = parser.isSet("json");

    BackupFile *bf = nullptr;
    int rc = openAndSetup(args.first(), password, bf);
    if (rc != 0)
        return rc;

    QJsonObject info = bf->getArchiveInfo();
    bf->close();
    delete bf;

    if (jsonMode) {
        fprintf(stdout, "%s\n", QJsonDocument(info).toJson(QJsonDocument::Compact).constData());
    } else {
        fprintf(stdout, "Version     : %d\n", info["version"].toInt());
        fprintf(stdout, "Encrypted   : %s\n", info["encrypted"].toBool() ? "yes" : "no");
        fprintf(stdout, "Compression : %s\n", info["compression"].toString().toLocal8Bit().constData());
        fprintf(stdout, "Total files : %d\n", info["totalFiles"].toInt());
        fprintf(stdout, "Total size  : %lld bytes\n", static_cast<long long>(info["totalSize"].toDouble()));
        fprintf(stdout, "Archive size: %lld bytes\n", static_cast<long long>(info["archiveSize"].toDouble()));
    }
    fflush(stdout);
    return 0;
}

// ── extract ─────────────────────────────────────────────────────────────────

int cmdExtract(int /*argc*/, char * /*argv*/[])
{
    QCommandLineParser parser;
    parser.setApplicationDescription("Extract a .wpress archive");
    addCommonOptions(parser);
    parser.addPositionalArgument("archive", "Archive file (.wpress)");
    parser.addPositionalArgument("destination", "Directory to extract into", "[destination]");
    parser.process(*QCoreApplication::instance());

    const QStringList args = positionalArgs(parser);
    if (args.isEmpty()) {
        fprintf(stderr, "Error: missing archive argument\nUsage: traktor extract [options] <archive> [destination]\n");
        return 1;
    }

    const QString password = resolvePassword(parser);
    const bool jsonMode = parser.isSet("json");
    const bool quiet = parser.isSet("quiet");

    BackupFile *bf = nullptr;
    int rc = openAndSetup(args.first(), password, bf);
    if (rc != 0)
        return rc;

    QFileInfo archiveInfo(args.first());
    QString destPath = args.size() > 1 ? args.at(1) : QDir::currentPath();
    QDir extractTo(destPath + "/" + archiveInfo.baseName());

    if (!QDir().mkpath(extractTo.path())) {
        fprintf(stderr, "Error: cannot create directory: %s\n", extractTo.path().toLocal8Bit().constData());
        bf->close();
        delete bf;
        return 1;
    }

    if (!quiet) {
        QObject::connect(bf, &BackupFile::progress, printProgressStderr);
    }

    QObject::connect(bf, &BackupFile::error, [](const QString &msg) {
        fprintf(stderr, "\nError: %s\n", msg.toLocal8Bit().constData());
        fflush(stderr);
    });

    QElapsedTimer timer;
    timer.start();

    const bool ok = bf->extract(extractTo);
    bf->close();

    const qint64 elapsed = timer.elapsed();

    if (!quiet) {
        fprintf(stderr, "\n");
    }

    if (jsonMode) {
        QJsonObject result;
        result["status"] = ok ? QString("success") : QString("error");
        result["destination"] = extractTo.path();
        result["duration_ms"] = elapsed;
        fprintf(stdout, "%s\n", QJsonDocument(result).toJson(QJsonDocument::Compact).constData());
    } else {
        if (ok) {
            fprintf(stdout, "Done. Extracted to: %s\n", extractTo.path().toLocal8Bit().constData());
        } else {
            fprintf(stderr, "Extraction failed.\n");
        }
    }
    fflush(stdout);

    delete bf;
    return ok ? 0 : 2;
}

// ── cat ─────────────────────────────────────────────────────────────────────

int cmdCat(int /*argc*/, char * /*argv*/[])
{
    QCommandLineParser parser;
    parser.setApplicationDescription("Stream a single file from a .wpress archive to stdout");
    addCommonOptions(parser);
    parser.addPositionalArgument("archive", "Archive file (.wpress)");
    parser.addPositionalArgument("path", "File path inside the archive");
    parser.process(*QCoreApplication::instance());

    if (parser.isSet("json")) {
        fprintf(stderr, "Error: --json is not supported with cat (output is always binary)\n");
        return 1;
    }

    const QStringList args = positionalArgs(parser);
    if (args.size() < 2) {
        fprintf(stderr, "Error: missing arguments\nUsage: traktor cat [options] <archive> <path>\n");
        return 1;
    }

    const QString password = resolvePassword(parser);

    BackupFile *bf = nullptr;
    int rc = openAndSetup(args.at(0), password, bf);
    if (rc != 0)
        return rc;

#ifdef Q_OS_WIN
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    QFile stdoutFile;
    if (!stdoutFile.open(stdout, QIODevice::WriteOnly)) {
        fprintf(stderr, "Error: cannot open stdout for writing\n");
        bf->close();
        delete bf;
        return 1;
    }

    const bool ok = bf->extractSingleFile(args.at(1), &stdoutFile);
    stdoutFile.close();
    bf->close();
    delete bf;

    if (!ok) {
        fprintf(stderr, "Error: file not found in archive: %s\n", args.at(1).toLocal8Bit().constData());
        return 1;
    }
    return 0;
}

// ── verify ──────────────────────────────────────────────────────────────────

int cmdVerify(int /*argc*/, char * /*argv*/[])
{
    QCommandLineParser parser;
    parser.setApplicationDescription("Verify integrity of a .wpress archive");
    addCommonOptions(parser);
    parser.addPositionalArgument("archive", "Archive file (.wpress)");
    parser.process(*QCoreApplication::instance());

    const QStringList args = positionalArgs(parser);
    if (args.isEmpty()) {
        fprintf(stderr, "Error: missing archive argument\nUsage: traktor verify [options] <archive>\n");
        return 1;
    }

    const QString password = resolvePassword(parser);
    const bool jsonMode = parser.isSet("json");
    const bool quiet = parser.isSet("quiet");

    BackupFile *bf = nullptr;
    int rc = openAndSetup(args.first(), password, bf);
    if (rc != 0)
        return rc;

    const bool isV2 = bf->isV2Format();
    bool allPassed = true;
    bool hasCrcData = false;

    if (!quiet) {
        QObject::connect(bf, &BackupFile::progress, printProgressStderr);
    }

    // Per-file CRC verification for v2 archives
    if (isV2) {
        bool iterOk = bf->iterateHeaders([&](const BackupFile::HeaderInfo &info) {
            const QString path = BackupFile::normalizePath(info.filePath, info.fileName);

            if (!info.crc32.isEmpty()) {
                hasCrcData = true;

                // Compute CRC by streaming through decrypt/decompress to CrcDevice
                CrcDevice crcSink;

                const bool isCompressed =
                    !CryptoUtils::isConfigFile(info.fileName) && bf->getCompressionType() != COMPRESSION_NONE;

                QString processError;
                bool streamOk;

                if (bf->isEncryptedFile() && !password.isEmpty()) {
                    streamOk = CryptoUtils::processFileContentWithPasswordStreaming(
                        bf, info.fileSize, &crcSink, isCompressed, info.fileName, password, bf->getCompressionType(),
                        &processError);
                } else {
                    streamOk = CryptoUtils::processFileContentStreaming(bf, info.fileSize, &crcSink, isCompressed,
                                                                        info.fileName, bf->getCompressionType(),
                                                                        &processError);
                }

                QString status;
                const QString actualCrc = crcSink.result();

                if (!streamOk) {
                    status = "error";
                    allPassed = false;
                    if (!processError.isEmpty()) {
                        fprintf(stderr, "Error verifying '%s': %s\n", path.toLocal8Bit().constData(),
                                processError.toLocal8Bit().constData());
                    }
                } else if (actualCrc == info.crc32) {
                    status = "pass";
                } else {
                    status = "fail";
                    allPassed = false;
                }

                if (jsonMode) {
                    QJsonObject obj;
                    obj["path"] = path;
                    obj["status"] = status;
                    obj["expectedCrc"] = info.crc32;
                    obj["actualCrc"] = actualCrc;
                    if (!processError.isEmpty()) {
                        obj["error"] = processError;
                    }
                    fprintf(stdout, "%s\n", QJsonDocument(obj).toJson(QJsonDocument::Compact).constData());
                } else {
                    fprintf(stdout, "%s\t%s\t%s\t%s\n", status.toLocal8Bit().constData(),
                            path.toLocal8Bit().constData(), info.crc32.toLocal8Bit().constData(),
                            actualCrc.toLocal8Bit().constData());
                }
                fflush(stdout);

                // Don't seek past content — streaming already consumed it
                return true;
            }

            // No CRC for this entry — iterateHeaders will skip content automatically

            if (jsonMode) {
                QJsonObject obj;
                obj["path"] = path;
                obj["status"] = QString("unchecked");
                fprintf(stdout, "%s\n", QJsonDocument(obj).toJson(QJsonDocument::Compact).constData());
            } else {
                fprintf(stdout, "unchecked\t%s\n", path.toLocal8Bit().constData());
            }
            fflush(stdout);
            return true;
        });

        if (!quiet) {
            fprintf(stderr, "\n");
        }

        if (!iterOk) {
            bf->close();
            delete bf;
            return 2;
        }
    } else {
        // v1 archive: structural validation only (no CRC data)
        bool iterOk = bf->iterateHeaders([&](const BackupFile::HeaderInfo &info) {
            const QString path = BackupFile::normalizePath(info.filePath, info.fileName);
            if (jsonMode) {
                QJsonObject obj;
                obj["path"] = path;
                obj["status"] = QString("unchecked");
                fprintf(stdout, "%s\n", QJsonDocument(obj).toJson(QJsonDocument::Compact).constData());
            } else {
                fprintf(stdout, "unchecked\t%s\n", path.toLocal8Bit().constData());
            }
            fflush(stdout);
            return true;
        });

        if (!quiet) {
            fprintf(stderr, "\n");
        }

        if (!iterOk) {
            bf->close();
            delete bf;
            return 2;
        }
    }

    bf->close();
    delete bf;

    if (!allPassed) {
        return 4;
    }
    return 0;
}

// ── global help ─────────────────────────────────────────────────────────────

void printGlobalHelp()
{
    fprintf(stdout, "Qtraktor - All-in-One WP Migration and Backup extractor\n"
                    "\n"
                    "Usage: traktor <command> [options] <archive>\n"
                    "\n"
                    "Commands:\n"
                    "  list        List contents of a .wpress archive\n"
                    "  info        Show archive metadata (format, encryption, file count)\n"
                    "  extract     Extract all files from a .wpress archive\n"
                    "  cat         Stream a single file from an archive to stdout\n"
                    "  verify      Verify archive integrity (CRC32)\n"
                    "  mcp         Start MCP server for AI agent integration\n"
                    "  install-cli Install command-line tool to system PATH (macOS)\n"
                    "  uninstall   Remove CLI, AI agent integrations, and settings\n"
                    "\n"
                    "Global options:\n"
                    "  -p, --password <pw>   Password for encrypted archives\n"
                    "                        (or set TRAKTOR_PASSWORD environment variable)\n"
                    "  --json                Machine-readable JSON output\n"
                    "  -q, --quiet           Suppress progress output\n"
                    "  -h, --help            Show this help\n"
                    "  -v, --version         Show version\n"
                    "\n"
#ifdef __linux__
                    "Linux GUI options:\n"
                    "  --gui                 Force GUI mode (fail loudly if GUI libs missing)\n"
                    "  --cli, --no-gui       Force CLI mode (skip GUI auto-detect)\n"
                    "\n"
#endif
                    "Legacy mode:\n"
                    "  traktor --source <file> --destination <dir> [-p password]\n"
                    "\n"
                    "Exit codes:\n"
                    "  0  Success\n"
                    "  1  General error (I/O, permissions, usage)\n"
                    "  2  Invalid or corrupted archive\n"
                    "  3  Wrong or missing password\n"
                    "  4  CRC32 verification failure\n"
                    "\n"
                    "Examples:\n"
                    "  traktor list backup.wpress\n"
                    "  traktor list --json backup.wpress | jq '.path'\n"
                    "  traktor cat backup.wpress wp-config.php | grep DB_PASSWORD\n"
                    "  traktor verify --json backup.wpress\n"
                    "  traktor extract backup.wpress ./output/\n");
    fflush(stdout);
}

// ── subcommand dispatch ─────────────────────────────────────────────────────

int dispatchCliSubcommand(int argc, char *argv[], bool *handled)
{
    *handled = false;
    if (argc < 2) {
        return 0;
    }

    const QString sub = QString::fromLocal8Bit(argv[1]);

    auto run = [&](auto fn) {
        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName("traktor");
        return fn();
    };

    if (sub == "list") {
        *handled = true;
        return run([&] { return cmdList(argc, argv); });
    }
    if (sub == "info") {
        *handled = true;
        return run([&] { return cmdInfo(argc, argv); });
    }
    if (sub == "extract") {
        *handled = true;
        return run([&] { return cmdExtract(argc, argv); });
    }
    if (sub == "cat") {
        *handled = true;
        return run([&] { return cmdCat(argc, argv); });
    }
    if (sub == "verify") {
        *handled = true;
        return run([&] { return cmdVerify(argc, argv); });
    }
    if (sub == "mcp") {
        *handled = true;
        return run([&] { return cmdMcp(); });
    }
    if (sub == "install-cli") {
        *handled = true;
        return run([&] { return cmdInstallCli(); });
    }
    if (sub == "uninstall") {
        *handled = true;
        return run([&] { return cmdUninstall(); });
    }

    return 0;
}

// ── legacy --source / --destination extract ─────────────────────────────────

static void printLegacyProgress(float percent)
{
    int filled = static_cast<int>(percent / 5.0f);
    fprintf(stdout, "\r[");
    for (int i = 0; i < 20; i++)
        fputc(i < filled ? '#' : ' ', stdout);
    fprintf(stdout, "] %3d%%", static_cast<int>(percent));
    fflush(stdout);
}

int cmdLegacyExtract(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("traktor");

    QCommandLineParser parser;
    parser.setApplicationDescription("Qtraktor - legacy CLI extract mode");
    parser.addHelpOption();

    QCommandLineOption sourceOption(QStringList() << "s" << "source", "Backup file to open (.wpress)", "source");
    parser.addOption(sourceOption);
    QCommandLineOption destinationOption(QStringList() << "d" << "destination", "Directory to extract the backup into",
                                         "destination");
    parser.addOption(destinationOption);
    QCommandLineOption passwordOption(QStringList() << "p" << "password", "Password for encrypted backup", "password");
    parser.addOption(passwordOption);

    parser.process(app);

    const QString source = parser.value(sourceOption);
    const QString destination = parser.value(destinationOption);
    const QString password = parser.value(passwordOption);

    if (source.isEmpty() || destination.isEmpty()) {
        fprintf(stderr, "Error: legacy mode requires both --source and --destination\n");
        return 1;
    }

    QFileInfo fileInfo(source);
    if (!fileInfo.isReadable()) {
        fprintf(stderr, "Error: cannot read file: %s\n", source.toLocal8Bit().constData());
        return 1;
    }

    QDir extractTo(destination + "/" + fileInfo.baseName());
    // mkpath() returns true when the directory already exists, so on error
    // paths we must only removeRecursively() the dir if WE created it —
    // otherwise we'd silently wipe a user's pre-existing directory (e.g. a
    // prior successful extraction at the same destination).
    const bool dirPreExisted = QFileInfo::exists(extractTo.path());
    if (!QDir().mkpath(extractTo.path())) {
        fprintf(stderr, "Error: cannot create directory: %s\n", extractTo.path().toLocal8Bit().constData());
        return 1;
    }
    auto cleanupIfCreated = [&] {
        if (!dirPreExisted)
            extractTo.removeRecursively();
    };

    BackupFile configChecker(source);
    bool needsPassword = false;
    CompressionType compressionType = COMPRESSION_NONE;

    if (configChecker.open(QIODevice::ReadOnly)) {
        if (configChecker.isValid()) {
            needsPassword = configChecker.isEncryptedFile();
            compressionType = configChecker.getCompressionType();
        }
        configChecker.close();
    }

    QString filePassword = password;
    if (filePassword.isEmpty())
        filePassword = qEnvironmentVariable("TRAKTOR_PASSWORD");
    if (needsPassword && filePassword.isEmpty()) {
        fprintf(stderr, "Error: backup is encrypted - provide a password with -p\n");
        cleanupIfCreated();
        return 1;
    }

    BackupFile backupFile(source, filePassword);
    if (!backupFile.open(QIODevice::ReadOnly)) {
        fprintf(stderr, "Error: cannot open file: %s\n", source.toLocal8Bit().constData());
        cleanupIfCreated();
        return 1;
    }
    backupFile.setConfig(needsPassword, compressionType);

    if (!backupFile.isValid()) {
        fprintf(stderr, "Error: backup file is corrupted (missing end-of-file marker)\n");
        backupFile.close();
        cleanupIfCreated();
        return 1;
    }

    fprintf(stdout, "File : %s\n", fileInfo.fileName().toLocal8Bit().constData());
    fprintf(stdout, "To   : %s\n", extractTo.path().toLocal8Bit().constData());
    fflush(stdout);

    QObject::connect(&backupFile, &BackupFile::progress, printLegacyProgress);

    QObject::connect(&backupFile, &BackupFile::error, [](const QString &msg) {
        fprintf(stderr, "\nError: %s\n", msg.toLocal8Bit().constData());
        fflush(stderr);
    });

    const bool ok = backupFile.extract(extractTo);
    backupFile.close();

    fprintf(stdout, "\n");
    fflush(stdout);

    if (!ok) {
        fprintf(stderr, "Extraction failed.\n");
        cleanupIfCreated();
        return 1;
    }

    fprintf(stdout, "Done. Extracted to: %s\n", extractTo.path().toLocal8Bit().constData());
    fflush(stdout);
    return 0;
}
