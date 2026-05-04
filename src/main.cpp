// Traktor entry point.
//
// All platforms: argv[1] subcommand dispatch routes list/info/extract/
// cat/verify/mcp/install-cli/uninstall through QCoreApplication. --help
// and --version are intercepted before any Q*Application is constructed
// so the loader never has to resolve QtGui / libGL just to print help.
//
// Linux: this file links only Qt6::Core. The GUI lives in
// libtraktor-gui.so which we dlopen() at runtime when the user wants
// it (and the system can support it). On a minimal container (no
// $DISPLAY, no libGL) the binary stays in CLI mode.
//
// macOS / Windows: the GUI runs inline below under #ifndef __linux__.
// No dlopen, no plugin — the executable links Qt6::Widgets directly,
// exactly as it always has.

#include "clihandler.h"
#include <QString>
#include <cstdio>
#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Windows builds use the console subsystem so CLI subcommands (--help,
// list, extract, ...) print reliably to any shell. The cost is that a
// console window is allocated when launching from Explorer. Hide it (only
// if we own the console — never the inherited terminal of cmd / PowerShell
// / Windows Terminal) and detach before the GUI event loop starts so the
// GUI looks clean. No-op on macOS/Linux.
static void detachFromConsole()
{
#ifdef Q_OS_WIN
    // GetWindowThreadProcessId(GetConsoleWindow(), ...) returns the PID of
    // conhost.exe (the out-of-process console host), not our PID — so a
    // self-ownership check is always false. The documented idiom is
    // GetConsoleProcessList: a count of 1 means we are the sole attachee,
    // i.e. Windows allocated this console for us at launch (Explorer flow).
    // A count > 1 means we inherited the user's terminal — leave it alone.
    if (GetConsoleProcessList(nullptr, 0) == 1) {
        if (HWND console = GetConsoleWindow())
            ShowWindow(console, SW_HIDE);
        FreeConsole();
    }
#endif
}

#ifdef __linux__
#include <dlfcn.h>
#include <linux/limits.h>
#include <unistd.h>
#include <cstdlib>
#include <initializer_list>
#include <QDir>
#include <QFileInfo>
#endif

#ifndef __linux__
#include "appdelegate.h"
#include "autoextractor.h"
#include "backupfile.h"
#include "dockprogress.h"
#include "extractionworker.h"
#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QLocalSocket>
#include <QTimer>

static void printProgress(float percent)
{
    int filled = static_cast<int>(percent / 5.0f);
    fprintf(stdout, "\r[");
    for (int i = 0; i < 20; i++)
        fputc(i < filled ? '#' : ' ', stdout);
    fprintf(stdout, "] %3d%%", static_cast<int>(percent));
    fflush(stdout);
}

static int runGuiInline(int argc, char **argv);
#endif // !__linux__

#ifdef __linux__

static bool hasFlag(int argc, char **argv, const char *name)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], name) == 0)
            return true;
    }
    return false;
}

// Strip the named flags from argv in-place. Argument vector is shrunk,
// argc updated. We never touch argv[0]. Used so QCommandLineParser
// downstream (in run_gui or cmdLegacyExtract) doesn't reject --gui /
// --cli / --no-gui as unknown options.
static void stripFlags(int *argc, char **argv, std::initializer_list<const char *> flags)
{
    int w = 1;
    for (int r = 1; r < *argc; ++r) {
        bool drop = false;
        for (const char *f : flags) {
            if (std::strcmp(argv[r], f) == 0) {
                drop = true;
                break;
            }
        }
        if (!drop) {
            argv[w++] = argv[r];
        }
    }
    for (int i = w; i < *argc; ++i) {
        argv[i] = nullptr;
    }
    *argc = w;
}

// Resolve absolute path to libtraktor-gui.so. We compute it from
// /proc/self/exe so LD_LIBRARY_PATH / cwd don't matter. Try the
// installed layout (../lib) first, then the build-tree layout (next to
// the executable).
static QString resolveGuiPluginPath(QString *errorOut)
{
    char buf[PATH_MAX];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) {
        if (errorOut)
            *errorOut = QStringLiteral("cannot read /proc/self/exe");
        return QString();
    }
    buf[n] = '\0';
    QFileInfo exe(QString::fromLocal8Bit(buf));
    QDir d = exe.absoluteDir();
    const char *candidates[] = {"../lib/libtraktor-gui.so", "libtraktor-gui.so"};
    for (const char *rel : candidates) {
        QString p = QDir::cleanPath(d.absoluteFilePath(QString::fromLatin1(rel)));
        if (QFileInfo::exists(p))
            return p;
    }
    if (errorOut)
        *errorOut = QStringLiteral("libtraktor-gui.so not found next to executable");
    return QString();
}

static int runGuiPlugin(int argc, char **argv, bool loudOnFail)
{
    QString err;
    QString path = resolveGuiPluginPath(&err);
    if (path.isEmpty()) {
        if (loudOnFail)
            std::fprintf(stderr, "traktor: %s\n", qPrintable(err));
        return 1;
    }
    void *h = ::dlopen(path.toLocal8Bit().constData(), RTLD_NOW);
    if (!h) {
        if (loudOnFail)
            std::fprintf(stderr, "traktor: cannot load GUI plugin: %s\n", ::dlerror());
        return 1;
    }
    using RunGuiFn = int (*)(int, char **);
    auto fn = reinterpret_cast<RunGuiFn>(::dlsym(h, "run_gui"));
    if (!fn) {
        if (loudOnFail)
            std::fprintf(stderr, "traktor: GUI plugin missing run_gui symbol: %s\n", ::dlerror());
        ::dlclose(h);
        return 1;
    }
    return fn(argc, argv);
}

// Probe whether GUI mode is viable. Returns true and leaves whyOut
// empty on success; returns false and writes a short reason on
// failure. We dlclose what we open here — runGuiPlugin re-opens for
// the real run.
static bool guiProbeOk(QString *whyOut)
{
    if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY")) {
        if (whyOut)
            *whyOut = QStringLiteral("no display server (DISPLAY/WAYLAND_DISPLAY unset)");
        return false;
    }
    void *gl = ::dlopen("libGL.so.1", RTLD_LAZY);
    if (!gl) {
        if (whyOut)
            *whyOut = QStringLiteral("libGL.so.1 not loadable");
        return false;
    }
    QString perr;
    QString path = resolveGuiPluginPath(&perr);
    if (path.isEmpty()) {
        ::dlclose(gl);
        if (whyOut)
            *whyOut = perr;
        return false;
    }
    void *gui = ::dlopen(path.toLocal8Bit().constData(), RTLD_NOW);
    if (!gui) {
        if (whyOut)
            *whyOut = QString::fromLatin1("GUI plugin probe failed: %1").arg(QString::fromLocal8Bit(::dlerror()));
        ::dlclose(gl);
        return false;
    }
    ::dlclose(gui);
    ::dlclose(gl);
    return true;
}

// CLI-mode fallback: if --source/--destination were passed, run the
// legacy extract; otherwise print the global help. Exit codes match
// the legacy behavior.
static int runCliFallback(int argc, char **argv)
{
    bool hasSrc = false, hasDst = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-s") == 0 || std::strcmp(argv[i], "--source") == 0 ||
            std::strncmp(argv[i], "--source=", 9) == 0)
            hasSrc = true;
        if (std::strcmp(argv[i], "-d") == 0 || std::strcmp(argv[i], "--destination") == 0 ||
            std::strncmp(argv[i], "--destination=", 14) == 0)
            hasDst = true;
    }
    if (hasSrc && hasDst)
        return cmdLegacyExtract(argc, argv);
    printGlobalHelp();
    return 0;
}

#endif // __linux__

int main(int argc, char *argv[])
{
#ifdef __linux__
    // Detect and strip Linux GUI/CLI flags BEFORE the all-platforms subcommand
    // dispatch so:
    //   (a) `traktor list --gui backup.wpress` doesn't choke cmdList's
    //       QCommandLineParser with "Unknown option: gui",
    //   (b) `traktor --gui --help` resolves to printGlobalHelp() instead of
    //       Qt's auto-generated mini-help inside run_gui.
    bool forceGui = hasFlag(argc, argv, "--gui");
    bool forceCli = hasFlag(argc, argv, "--cli") || hasFlag(argc, argv, "--no-gui");

    if (forceGui && forceCli) {
        std::fprintf(stderr, "traktor: --gui and --cli are mutually exclusive\n");
        return 1;
    }

    stripFlags(&argc, argv, {"--gui", "--cli", "--no-gui"});
#endif

    // ── Subcommand dispatch (all platforms) ────────────────────────────────
    // Detect subcommand or --help/--version from raw argv BEFORE creating
    // any Q*Application. CLI subcommands use QCoreApplication only — they
    // never construct QApplication and never need a display server.
    if (argc >= 2) {
        const QString sub = QString::fromLocal8Bit(argv[1]);

        // Global help — intercept before QApplication to prevent
        // QCommandLineParser's auto-help from showing the old GUI-mode help
        if (sub == "--help" || sub == "-h") {
            printGlobalHelp();
            return 0;
        }
        if (sub == "--version" || sub == "-v") {
#ifdef PROJECT_VERSION_STR
            std::printf("Traktor %s\n", PROJECT_VERSION_STR);
#else
            std::printf("Traktor (unknown version)\n");
#endif
            return 0;
        }

        bool handled = false;
        int rc = dispatchCliSubcommand(argc, argv, &handled);
        if (handled)
            return rc;
    }

#ifdef __linux__
    // ── Linux: hybrid CLI/GUI dispatch ─────────────────────────────────────
    if (forceCli)
        return runCliFallback(argc, argv);

    if (forceGui)
        return runGuiPlugin(argc, argv, /*loudOnFail=*/true);

    QString why;
    if (!guiProbeOk(&why)) {
        std::fprintf(stderr,
                     "traktor: GUI unavailable (%s); running in CLI mode. "
                     "Re-run with --gui to see the specific error.\n",
                     qPrintable(why));
        return runCliFallback(argc, argv);
    }
    return runGuiPlugin(argc, argv, /*loudOnFail=*/true);
#else
    return runGuiInline(argc, argv);
#endif
}

#ifndef __linux__
// ── macOS / Windows GUI path (unchanged from pre-split main.cpp) ──────────
// QApplication, MainWindow, AutoExtractor, the macOS FileOpen startup
// window, and the legacy --source/--destination CLI extraction (which on
// macOS/Windows runs inside QApplication for parity with prior releases).
static int runGuiInline(int argc, char **argv)
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
        QFileInfo fileInfo(source);
        if (!fileInfo.isReadable()) {
            fprintf(stderr, "Error: cannot read file: %s\n", source.toLocal8Bit().constData());
            return 1;
        }

        QDir extractTo(destination + "/" + fileInfo.baseName());
        // mkpath() returns true when the directory already exists, so on
        // error paths we must only removeRecursively() the dir if WE created
        // it — otherwise we'd silently wipe a user's pre-existing directory
        // (e.g. a prior successful extraction at the same destination).
        const bool dirPreExisted = QFileInfo::exists(extractTo.path());
        if (!QDir().mkpath(extractTo.path())) {
            fprintf(stderr, "Error: cannot create directory: %s\n", extractTo.path().toLocal8Bit().constData());
            return 1;
        }
        auto cleanupIfCreated = [&] {
            if (!dirPreExisted)
                extractTo.removeRecursively();
        };

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
        if (filePassword.isEmpty())
            filePassword = qEnvironmentVariable("TRAKTOR_PASSWORD");
        if (needsPassword && filePassword.isEmpty()) {
            fprintf(stderr, "Error: backup is encrypted – provide a password with -p\n");
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
            cleanupIfCreated();
            return 1;
        }

        fprintf(stdout, "Done. Extracted to: %s\n", extractTo.path().toLocal8Bit().constData());
        fflush(stdout);
        return 0;
    }

    // Past the CLI-only paths — we're going to QApplication::exec() one way
    // or another (auto-extract, GUI with --source, or bare GUI). On Windows,
    // drop the console window allocated for the console-subsystem binary so
    // the GUI launch doesn't leave an empty cmd window behind.
    detachFromConsole();

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
#endif // !__linux__
