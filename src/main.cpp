#include "mainwindow.h"
#include "appdelegate.h"
#include "autoextractor.h"
#include "backupfile.h"
#include "dockprogress.h"
#include "extractionworker.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QDir>
#include <QFileOpenEvent>
#include <QLocalSocket>
#include <QTimer>
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

    // Register Traktor as the default handler for .wpress files
    claimFileType();

    QCommandLineParser parser;
    parser.setApplicationDescription("Qtraktor - All-in-One WP Migration and Backup extractor");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption sourceOption(QStringList() << "s" << "source", "Backup file to open (.wpress)", "source");
    parser.addOption(sourceOption);

    QCommandLineOption destinationOption(QStringList() << "d" << "destination", "Directory to extract the backup into",
                                         "destination");
    parser.addOption(destinationOption);

    QCommandLineOption passwordOption(QStringList() << "p" << "password", "Password for encrypted backup", "password");
    parser.addOption(passwordOption);

    parser.addPositionalArgument("file", "Backup file to open (.wpress)", "[file]");

    parser.process(a);

    QString source = parser.value(sourceOption);
    const QStringList positional = parser.positionalArguments();
    if (source.isEmpty() && !positional.isEmpty())
        source = positional.first();

    const QString destination = parser.value(destinationOption);
    const QString password = parser.value(passwordOption);

    // ── CLI mode ────────────────────────────────────────────────────────────
    if (!source.isEmpty() && !destination.isEmpty()) {
        attachConsole();

        QFileInfo fileInfo(source);
        if (!fileInfo.isReadable()) {
            fprintf(stderr, "Error: cannot read file: %s\n", source.toLocal8Bit().constData());
            return 1;
        }

        QDir extractTo(destination + "/" + fileInfo.baseName());
        if (!QDir().mkdir(extractTo.path())) {
            fprintf(stderr, "Error: cannot create directory: %s\n", extractTo.path().toLocal8Bit().constData());
            return 1;
        }

        // Read config header to detect encryption / compression
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
        if (needsPassword && filePassword.isEmpty()) {
            fprintf(stderr, "Error: backup is encrypted – provide a password with -p\n");
            extractTo.removeRecursively();
            return 1;
        }

        BackupFile backupFile(source, filePassword);
        if (!backupFile.open(QIODevice::ReadOnly)) {
            fprintf(stderr, "Error: cannot open file: %s\n", source.toLocal8Bit().constData());
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
        fprintf(stdout, "To   : %s\n", extractTo.path().toLocal8Bit().constData());
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

        fprintf(stdout, "Done. Extracted to: %s\n", extractTo.path().toLocal8Bit().constData());
        fflush(stdout);
        return 0;
    }

    // ── Auto-extract mode ──────────────────────────────────────────────────
    // Positional args without --source/--destination flags = double-click / file association
    const bool hasExplicitSource = parser.isSet(sourceOption);
    if (!positional.isEmpty() && !hasExplicitSource) {
        // Try single-instance: forward to existing instance if running
        QLocalSocket socket;
        socket.connectToServer("com.servmask.Traktor");
        if (socket.waitForConnected(500)) {
            // Another instance is running, forward all files
            for (const QString &file : positional) {
                socket.write((file + "\n").toUtf8());
            }
            socket.waitForBytesWritten(1000);
            socket.disconnectFromServer();
            return 0;
        }

        AutoExtractor extractor(positional);
        AppDelegate appDelegate(nullptr, &extractor);
        a.installEventFilter(&appDelegate);

        return QApplication::exec();
    }

    // ── GUI or macOS file-open mode ─────────────────────────────────────────
    // On macOS, double-clicking a .wpress file launches the app with NO args,
    // then sends QEvent::FileOpen shortly after. We defer the GUI/auto-extract
    // decision to handle this: install a startup delegate that collects FileOpen
    // events during a brief window, then decides which mode to use.

    // If --source was explicitly given, go straight to GUI mode
    if (!source.isEmpty()) {
        MainWindow w;
        AppDelegate appDelegate(&w);
        a.installEventFilter(&appDelegate);
        w.openBackupFile(source);
        if (!password.isEmpty())
            w.setPassword(password);
        w.show();
        return QApplication::exec();
    }

    // No args at all: wait briefly for macOS FileOpen events
    QStringList pendingFiles;
    bool decided = false;
    MainWindow *window = nullptr;
    AutoExtractor *extractor = nullptr;

    // Temporary event filter that collects FileOpen events during startup
    class StartupFilter : public QObject
    {
    public:
        QStringList *files;
        bool *decided;
        explicit StartupFilter(QStringList *f, bool *d, QObject *p = nullptr) : QObject(p), files(f), decided(d) {}
        bool eventFilter(QObject *, QEvent *event) override
        {
            if (event->type() == QEvent::FileOpen) {
                QFileOpenEvent *e = static_cast<QFileOpenEvent *>(event);

                if (!*decided) {
                    files->append(e->file());
                    return true;
                }
            }
            return false;
        }
    };

    StartupFilter startupFilter(&pendingFiles, &decided);
    a.installEventFilter(&startupFilter);

    // Give macOS 300ms to deliver FileOpen events
    QTimer::singleShot(300, [&]() {
        decided = true;
        a.removeEventFilter(&startupFilter);

        if (!pendingFiles.isEmpty()) {

            extractor = new AutoExtractor(pendingFiles);
            AppDelegate *appDel = new AppDelegate(nullptr, extractor);
            a.installEventFilter(appDel);
        } else {

            window = new MainWindow();
            AppDelegate *appDel = new AppDelegate(window);
            a.installEventFilter(appDel);
            window->show();
        }
    });

    return QApplication::exec();
}
