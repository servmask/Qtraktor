#include "installcli.h"
#include "agentconfig.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <cstdio>

static const QString CLI_TARGET = "/usr/local/bin/traktor";

// ── Core install logic ──────────────────────────────────────────────────────

InstallResult installCli()
{
    InstallResult result;

#ifdef Q_OS_MAC
    const QString binaryPath = QCoreApplication::applicationFilePath();

    // Pre-flight checks
    QFileInfo targetInfo(CLI_TARGET);
    if (targetInfo.exists()) {
        if (targetInfo.isSymLink()) {
            if (targetInfo.symLinkTarget() == binaryPath) {
                // Already installed correctly
                result.success = true;
                result.message = "Command-line tool is already installed at " + CLI_TARGET;

                // Still register MCP with detected agents in case that's missing
                AgentConfigManager mgr;
                QStringList messages;
                mgr.registerAllDetected(&messages);
                for (const QString &msg : messages)
                    result.message += "\n" + msg;
                return result;
            }
            // Symlink points elsewhere - will overwrite
        } else {
            // Regular file - refuse
            result.success = false;
            result.message = QString("A file already exists at %1 that is not a symlink. "
                                     "Remove it manually before installing.")
                                 .arg(CLI_TARGET);
            return result;
        }
    }

    // Create symlink via osascript admin privileges
    const QString shellCmd = QString("mkdir -p /usr/local/bin && ln -sf '%1' '%2'").arg(binaryPath, CLI_TARGET);

    QStringList osascriptArgs;
    osascriptArgs << "-e" << QString("do shell script \"%1\" with administrator privileges").arg(shellCmd);

    int exitCode = QProcess::execute("/usr/bin/osascript", osascriptArgs);
    if (exitCode != 0) {
        result.success = false;
        result.message = "Authorization denied or symlink creation failed.";
        return result;
    }

    result.success = true;
    result.message = QString("Installed command-line tool at %1").arg(CLI_TARGET);

    // Register MCP with all detected agents
    AgentConfigManager mgr;
    QStringList messages;
    int agentCount = mgr.registerAllDetected(&messages);
    for (const QString &msg : messages)
        result.message += "\n" + msg;
    if (agentCount > 0)
        result.message += "\nAI agents will discover Traktor automatically.";

#elif defined(Q_OS_LINUX)
    result.success = true;
    result.message = "To install the CLI on Linux, create a symlink manually:\n"
                     "  sudo ln -sf $(readlink -f ./Traktor) /usr/local/bin/traktor\n"
                     "\n"
                     "For AppImage:\n"
                     "  sudo ln -sf /path/to/Traktor.AppImage /usr/local/bin/traktor";
#elif defined(Q_OS_WIN)
    result.success = true;
    result.message = "The CLI is already available if Traktor was added to PATH during installation.\n"
                     "If not, add the Traktor installation directory to your PATH environment variable.";
#else
    result.success = false;
    result.message = "install-cli is not supported on this platform.";
#endif

    return result;
}

// ── CLI entry point ────────────────────────────────────────────────────────��

int cmdInstallCli()
{
    InstallResult result = installCli();

    if (result.success) {
        fprintf(stdout, "%s\n", result.message.toLocal8Bit().constData());
        return 0;
    } else {
        fprintf(stderr, "Error: %s\n", result.message.toLocal8Bit().constData());
        return 1;
    }
}

// ── Uninstall ───────────────────────────────────────────────────────────────

int cmdUninstall()
{
    fprintf(stdout, "This will remove Traktor CLI and all AI agent integrations.\n");
    fprintf(stdout, "Continue? [y/N] ");
    fflush(stdout);

    char c = 0;
    if (scanf(" %c", &c) != 1 || (c != 'y' && c != 'Y')) {
        fprintf(stdout, "Cancelled.\n");
        return 0;
    }

    // 1. Unregister MCP from all agents
    AgentConfigManager mgr;
    QStringList messages;
    mgr.unregisterAll(&messages);
    for (const QString &msg : messages)
        fprintf(stdout, "%s\n", msg.toLocal8Bit().constData());

#ifdef Q_OS_MAC
    // 2. Remove CLI symlink (needs admin privileges)
    if (QFile::exists(CLI_TARGET)) {
        QStringList osascriptArgs;
        osascriptArgs << "-e";
        osascriptArgs << "do shell script \"rm -f /usr/local/bin/traktor\" with administrator privileges";
        int rc = QProcess::execute("/usr/bin/osascript", osascriptArgs);
        if (rc == 0) {
            fprintf(stdout, "Removed CLI symlink at %s\n", CLI_TARGET.toLocal8Bit().constData());
        } else {
            fprintf(stderr, "Warning: could not remove CLI symlink (authorization denied)\n");
        }
    }
#endif

    // 3. Clear QSettings
    QSettings("com.servmask", "Traktor").clear();
    fprintf(stdout, "Cleared application settings.\n");

    // 4. Print instructions
    fprintf(stdout, "\nDone. Drag Traktor.app to Trash to complete uninstall.\n");
    fprintf(stdout, "If installed via Homebrew: brew uninstall traktor\n");
    return 0;
}
