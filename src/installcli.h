#ifndef INSTALLCLI_H
#define INSTALLCLI_H

#include <QString>

struct InstallResult {
    bool success;
    QString message;
};

// Create /usr/local/bin/traktor symlink (macOS) and register MCP server
// in ~/.claude.json for AI agent discovery.
InstallResult installCli();

// CLI entry point for "traktor install-cli" subcommand.
int cmdInstallCli();

// CLI entry point for "traktor uninstall" subcommand.
int cmdUninstall();

#endif // INSTALLCLI_H
