#include "mainwindow.h"
#include "appdelegate.h"
#include "backupfile.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QDir>
#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static void attachConsole()
{
#ifdef Q_OS_WIN
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif
}

static void printProgress(float percent)
{
    int filled = static_cast<int>(percent / 5.0f);
    fprintf(stdout, "\r[");
    for (int i = 0; i < 20; i++)
        fputc(i < filled ? '#' : ' ', stdout);
    fprintf(stdout, "] %3d%%", static_cast<int>(percent));
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Qtraktor - All-in-One WP Migration and Backup extractor");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption sourceOption(QStringList() << "s" << "source",
        "Backup file to open (.wpress)", "source");
    parser.addOption(sourceOption);

    QCommandLineOption destinationOption(QStringList() << "d" << "destination",
        "Directory to extract the backup into", "destination");
    parser.addOption(destinationOption);

    QCommandLineOption passwordOption(QStringList() << "p" << "password",
        "Password for encrypted backup", "password");
    parser.addOption(passwordOption);

    parser.addPositionalArgument("file", "Backup file to open (.wpress)", "[file]");

    parser.process(a);

    QString source = parser.value(sourceOption);
    const QStringList positional = parser.positionalArguments();
    if (source.isEmpty() && !positional.isEmpty())
        source = positional.first();

    const QString destination = parser.value(destinationOption);
    const QString password    = parser.value(passwordOption);

    // ── CLI mode ────────────────────────────────────────────────────────────
    if (!source.isEmpty() && !destination.isEmpty()) {
        attachConsole();

        QFileInfo fileInfo(source);
        if (!fileInfo.isReadable()) {
            fprintf(stderr, "Error: cannot read file: %s\n",
                    source.toLocal8Bit().constData());
            return 1;
        }

        QDir extractTo(destination + "/" + fileInfo.baseName());
        if (!QDir().mkdir(extractTo.path())) {
            fprintf(stderr, "Error: cannot create directory: %s\n",
                    extractTo.path().toLocal8Bit().constData());
            return 1;
        }

        // Read config header to detect encryption / compression
        BackupFile configChecker(source);
        bool needsPassword = false;
        CompressionType compressionType = COMPRESSION_NONE;

        if (configChecker.open(QIODevice::ReadOnly)) {
            if (configChecker.isValid()) {
                needsPassword    = configChecker.isEncryptedFile();
                compressionType  = configChecker.getCompressionType();
            }
            configChecker.close();
        }

        QString filePassword = password;
        if (needsPassword && filePassword.isEmpty()) {
            fprintf(stderr, "Error: backup is encrypted – provide a password with -p\n");
            extractTo.removeRecursively();
            return 1;
        }

        BackupFile backupFile(source, filePassword);
        if (!backupFile.open(QIODevice::ReadOnly)) {
            fprintf(stderr, "Error: cannot open file: %s\n",
                    source.toLocal8Bit().constData());
            extractTo.removeRecursively();
            return 1;
        }
        backupFile.setConfig(needsPassword, compressionType);

        if (!backupFile.isValid()) {
            fprintf(stderr, "Error: backup file is corrupted (missing end-of-file marker)\n");
            backupFile.close();
            extractTo.removeRecursively();
            return 1;
        }

        fprintf(stdout, "File : %s\n", fileInfo.fileName().toLocal8Bit().constData());
        fprintf(stdout, "To   : %s\n",  extractTo.path().toLocal8Bit().constData());
        fflush(stdout);

        QObject::connect(&backupFile, &BackupFile::progress, printProgress);

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
            extractTo.removeRecursively();
            return 1;
        }

        fprintf(stdout, "Done. Extracted to: %s\n",
                extractTo.path().toLocal8Bit().constData());
        fflush(stdout);
        return 0;
    }

    // ── GUI mode ─────────────────────────────────────────────────────────────
    MainWindow w;
    AppDelegate appDelegate(&w);
    a.installEventFilter(&appDelegate);

    if (!source.isEmpty())
        w.openBackupFile(source);

    if (!password.isEmpty())
        w.setPassword(password);

    w.show();

    return QApplication::exec();
}
