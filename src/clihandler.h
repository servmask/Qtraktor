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

#endif // CLIHANDLER_H
