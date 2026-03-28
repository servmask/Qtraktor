#include "installcli.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <cstdio>

static const QString CLI_TARGET = "/usr/local/bin/traktor";

// ── MCP config registration ─────────────────────────────────────────────────

static bool registerMcpConfig(QString *errorMsg)
{
    const QString configPath = QDir::homePath() + "/.claude.json";
    QJsonObject root;

    // Read existing config
    QFile configFile(configPath);
    if (configFile.exists()) {
        if (configFile.open(QIODevice::ReadOnly)) {
            QJsonParseError parseErr;
            QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll(), &parseErr);
            configFile.close();

            if (parseErr.error != QJsonParseError::NoError) {
                // Invalid JSON — back up and create new
                const QString backupPath = configPath + ".bak";
                QFile::remove(backupPath);
                QFile::copy(configPath, backupPath);
                if (errorMsg) {
                    *errorMsg += QString("Backed up invalid %1 to %2. ").arg(configPath, backupPath);
                }
            } else {
                root = doc.object();
            }
        }
    }

    // Get or create mcpServers object
    QJsonObject mcpServers = root["mcpServers"].toObject();

    // Add traktor MCP server config
    QJsonObject traktorConfig;
    traktorConfig["command"] = "traktor";
    QJsonArray argsArray;
    argsArray.append("mcp");
    traktorConfig["args"] = argsArray;

    mcpServers["traktor"] = traktorConfig;
    root["mcpServers"] = mcpServers;

    // Write back
    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMsg) {
            *errorMsg += QString("Cannot write to %1").arg(configPath);
        }
        return false;
    }

    configFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    configFile.close();
    return true;
}

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

                // Still register MCP in case that's missing
                QString mcpError;
                registerMcpConfig(&mcpError);
                if (!mcpError.isEmpty()) {
                    result.message += "\nMCP registration: " + mcpError;
                } else {
                    result.message += "\nMCP server registered in ~/.claude.json";
                }
                return result;
            }
            // Symlink points elsewhere — will overwrite
        } else {
            // Regular file — refuse
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

    // Register MCP config
    QString mcpError;
    registerMcpConfig(&mcpError);
    if (!mcpError.isEmpty()) {
        result.message += "\nMCP registration: " + mcpError;
    } else {
        result.message +=
            "\nMCP server registered in ~/.claude.json — Claude Code will discover Traktor automatically.";
    }

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
