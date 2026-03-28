# Traktor

[![CI](https://github.com/servmask/Qtraktor/actions/workflows/ci.yml/badge.svg)](https://github.com/servmask/Qtraktor/actions/workflows/ci.yml)
[![Release](https://github.com/servmask/Qtraktor/actions/workflows/release.yml/badge.svg)](https://github.com/servmask/Qtraktor/actions/workflows/release.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Windows%20%7C%20Linux-lightgrey)](https://github.com/servmask/Qtraktor/releases)

Cross-platform desktop application for extracting WordPress `.wpress` backup files created by [All-in-One WP Migration](https://wordpress.org/plugins/all-in-one-wp-migration/).

**Double-click a `.wpress` file and it just works.** Traktor extracts the archive next to the source file, opens the result folder, and gets out of your way. Like macOS Archive Utility, but for WordPress backups.

## Features

- **Plain, compressed, and encrypted archives** ... zlib, bzip2, and AES-256-CBC
- **v1 and v2 archive formats** with CRC32 verification
- **Auto-extract on open** ... double-click a `.wpress` file, extraction starts automatically
- **Drag and drop** ... drop files onto the window or the app icon
- **Cross-platform** ... native on macOS, Windows, and Linux
- **CLI with JSON output** ... `list`, `info`, `extract`, `cat`, `verify` subcommands
- **MCP server** ... AI agent integration via Model Context Protocol
- **Multi-file queue** ... drop multiple archives, they extract in sequence

## Install

### macOS

**Installer (recommended):**

Download [Traktor.pkg](https://github.com/servmask/Qtraktor/releases/latest) — installs the app to `/Applications` and adds the `traktor` CLI to your terminal.

**Homebrew:**

```bash
brew tap servmask/traktor
brew install --cask traktor
```

**DMG (manual):**

Download [Traktor.dmg](https://github.com/servmask/Qtraktor/releases/latest) and drag to Applications.

### Windows

Download [Traktor.exe](https://github.com/servmask/Qtraktor/releases/latest) installer.

### Linux

Download [Traktor.AppImage](https://github.com/servmask/Qtraktor/releases/latest) — make executable and run.

## Command Line

After installation, `traktor` is available in your terminal:

```bash
# List archive contents
traktor list backup.wpress
traktor list --json backup.wpress

# Stream a single file without extracting
traktor cat backup.wpress wp-config.php | grep DB_PASSWORD

# Show archive metadata
traktor info --json backup.wpress

# Verify archive integrity
traktor verify backup.wpress

# Extract everything
traktor extract backup.wpress ./output/
```

Run `traktor --help` for the full command reference.

## AI Agent Integration

Traktor includes an MCP server that lets AI coding agents (Claude Code, Gemini CLI, and others) inspect and extract `.wpress` files directly.

**Register with your agents:**

```bash
traktor mcp register
```

**Check which agents are detected:**

```bash
traktor mcp status
```

After registration, your AI agent can use Traktor as a tool — ask it "what's in this .wpress backup?" and it just works.

## Building from Source

### Prerequisites

<details>
<summary><strong>macOS</strong></summary>

```bash
brew install qt@5 openssl pkg-config
export PATH="/opt/homebrew/opt/qt@5/bin:$PATH"
```
</details>

<details>
<summary><strong>Linux (Debian/Ubuntu)</strong></summary>

```bash
sudo apt install qt5-qmake qtbase5-dev libssl-dev zlib1g-dev pkg-config
```
</details>

<details>
<summary><strong>Windows (MSVC)</strong></summary>

- Qt 5.x with MSVC kit
- OpenSSL (install via `choco install openssl` or download from [slproweb.com](https://slproweb.com/products/Win32OpenSSL.html))
- Set environment variable: `OPENSSL_DIR=C:\Program Files\OpenSSL-Win64`
</details>

### Build

```bash
qmake Qtraktor.pro
make -j$(nproc)        # Linux
make -j$(sysctl -n hw.ncpu)  # macOS
```

On Windows with MSVC:

```powershell
qmake.exe Qtraktor.pro
nmake -f Makefile.Release
```

### Run Tests

```bash
cd tests
qmake tests.pro
make -j$(nproc)
./tst_qtraktor
```

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for build instructions, code style guide, and PR guidelines.

## Security

To report a vulnerability, see [SECURITY.md](SECURITY.md).

## License

GPLv3 - see [LICENSE](LICENSE).
