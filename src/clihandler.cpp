#include "clihandler.h"
#include "backupfile.h"
#include "cryptoutils.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
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
