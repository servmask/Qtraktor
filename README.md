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

Download [Traktor.pkg](https://github.com/servmask/Qtraktor/releases/latest) - installs the app to `/Applications` and adds the `traktor` CLI to your terminal.

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

Download [Traktor.AppImage](https://github.com/servmask/Qtraktor/releases/latest) - make executable and run.

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

After registration, your AI agent can use Traktor as a tool - ask it "what's in this .wpress backup?" and it just works.

## Building from Source

### Prerequisites

<details>
<summary><strong>macOS</strong></summary>

```bash
brew install qt openssl pkg-config cmake
```
</details>

<details>
<summary><strong>Linux (Debian/Ubuntu)</strong></summary>

```bash
sudo apt install cmake qt6-base-dev libgl1-mesa-dev libssl-dev zlib1g-dev pkg-config
```
</details>

<details>
<summary><strong>Windows (MSVC)</strong></summary>

- Qt 6.8+ with MSVC kit (via [online installer](https://www.qt.io/download-qt-installer))
- CMake (via [cmake.org](https://cmake.org/download/) or `winget install Kitware.CMake`)
- vcpkg for OpenSSL: `vcpkg install openssl`
</details>

### Build

```bash
# macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl)"
cmake --build build -j$(sysctl -n hw.ncpu)

# Linux
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

On Windows with MSVC:

```powershell
cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_INSTALLATION_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build
```

### Run Tests

```bash
cd build && ctest --output-on-failure
```

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for build instructions, code style guide, and PR guidelines.

## Security

To report a vulnerability, see [SECURITY.md](SECURITY.md).

## License

GPLv3 - see [LICENSE](LICENSE).
