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
- **Multi-file queue** ... drop multiple archives, they extract in sequence
- **Progress indicators** ... Dock badge (macOS), taskbar progress (Windows), progress window

## Download

Get the latest release for your platform:

| Platform | Download |
|----------|----------|
| macOS (Universal) | [Traktor.dmg](https://github.com/servmask/Qtraktor/releases/latest) |
| Windows | [Traktor.exe](https://github.com/servmask/Qtraktor/releases/latest) |
| Linux | [Traktor.AppImage](https://github.com/servmask/Qtraktor/releases/latest) |

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

- Qt 5.x or 6.x with MSVC kit
- OpenSSL (install via `choco install openssl` or download from [slproweb.com](https://slproweb.com/products/Win32OpenSSL.html))
- Set environment variable: `OPENSSL_DIR=C:\Program Files\OpenSSL-Win64`
</details>

<details>
<summary><strong>Windows (MinGW)</strong></summary>

- Qt 5.x or 6.x with MinGW kit
- OpenSSL for MinGW
- Set environment variable: `OPENSSL_DIR=C:\path\to\openssl`
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

### Run

| Platform | Command |
|----------|---------|
| macOS | `open Traktor.app` |
| Linux | `./Traktor` |
| Windows | `release\Traktor.exe` |

### Run Tests

```bash
cd tests
qmake tests.pro
make -j$(nproc)
./tst_qtraktor
```

## Project Structure

```
.
├── Qtraktor.pro                  # qmake project file
├── Info.plist                    # macOS app metadata and .wpress file association
├── wpress.xml                    # MIME type definition for .wpress files
│
├── src/
│   ├── main.cpp                  # Entry point
│   ├── mainwindow.h/cpp/ui      # Main application window
│   ├── backupfile.h/cpp         # .wpress format parser and extractor
│   ├── cryptoutils.h/cpp        # Decompression (zlib, bzip2) and AES decryption
│   ├── extractionworker.h/cpp   # Background extraction thread
│   ├── autoextractor.h/cpp      # Auto-extract mode (double-click -> extract -> open)
│   ├── progresswindow.h/cpp     # Minimal progress window for auto-extract
│   ├── passworddialog.h/cpp     # Password prompt for encrypted archives
│   ├── dropoverlay.h/cpp        # Drag-and-drop overlay
│   ├── appdelegate.h/cpp        # macOS file-open event handling
│   └── dockprogress.h/mm        # macOS Dock progress badge (+ stub for other platforms)
│
├── tests/
│   ├── tests.pro                # Test project file
│   ├── tst_main.cpp             # Test runner entry point
│   ├── tst_backupfile.cpp       # Archive parsing and extraction tests
│   ├── tst_cryptoutils_streaming.cpp
│   ├── tst_extractionworker.cpp # Worker thread tests
│   ├── tst_qsettings.cpp       # Settings persistence tests
│   ├── tst_fuzz.cpp             # Fuzz tests (truncated, corrupted, path traversal)
│   ├── generate_fixtures.cpp    # Test fixture generator
│   └── fixtures/                # Generated .wpress test files
│
├── vendor/bzip2-1.0.8/          # Vendored bzip2 library
├── icons/                       # App and file type icons (.ico, .icns, .svg, .png)
├── config/                      # Qt Installer Framework config
└── packages/                    # Qt Installer Framework package metadata
```

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for:

- Build and test instructions for all platforms
- Code style guide (enforced by clang-format)
- Conventional commit message format
- PR checklist and guidelines

## Security

To report a vulnerability, see [SECURITY.md](SECURITY.md).

## License

GPLv3 - see [LICENSE](LICENSE).
