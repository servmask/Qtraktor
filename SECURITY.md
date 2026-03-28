# Security Policy

## Reporting a Vulnerability

**Do not open public issues for security vulnerabilities.**

Please report vulnerabilities via [GitHub's private vulnerability reporting](https://github.com/servmask/Qtraktor/security/advisories/new).

### Expected response time

We aim to respond within 7 days.

### Scope

Traktor is a local file extraction tool. The primary security concerns are:

- Malformed `.wpress` files causing crashes or memory corruption
- Path traversal during extraction (writing files outside the destination directory)
- Buffer overflows in the archive parser
