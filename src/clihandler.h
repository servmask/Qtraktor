#ifndef CLIHANDLER_H
#define CLIHANDLER_H

// CLI subcommand handlers for Qtraktor.
// Each function configures its own QCommandLineParser,
// operates on a BackupFile, and returns an exit code.
//
// Exit codes:
//   0 = success
//   1 = general error (I/O, permissions, usage)
//   2 = invalid/corrupted archive
//   3 = wrong or missing password
//   4 = CRC32 verification failure

int cmdList(int argc, char *argv[]);
int cmdInfo(int argc, char *argv[]);
int cmdExtract(int argc, char *argv[]);
int cmdCat(int argc, char *argv[]);
int cmdVerify(int argc, char *argv[]);

// Print the global --help text shown for `traktor` and `traktor --help`.
void printGlobalHelp();

// Dispatch a known CLI subcommand (list/info/extract/cat/verify/mcp/
// install-cli/uninstall). Sets *handled = true and returns the
// subcommand's exit code on a match. Sets *handled = false if argv[1]
// is not a recognized subcommand. Each branch constructs its own
// QCoreApplication; no GUI dependency.
int dispatchCliSubcommand(int argc, char *argv[], bool *handled);

// Legacy `--source <file> --destination <dir> [-p password]` extract
// path. Constructs its own QCoreApplication. Used by the Linux CLI
// fallback when GUI is unavailable but the user asked for an
// extraction.
int cmdLegacyExtract(int argc, char *argv[]);

#endif // CLIHANDLER_H
