# CLAUDE.md

## Project

Traktor is a cross-platform desktop application for extracting WordPress `.wpress` backup files created by All-in-One WP Migration. Built with C++17 and Qt 6.8 Widgets. Licensed under GPLv3.

## Build

```bash
# macOS
brew install qt openssl pkg-config
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl)"
cmake --build build -j$(sysctl -n hw.ncpu)

# Linux (Qt 6.8 required — install via package manager or aqtinstall)
sudo apt install cmake qt6-base-dev libgl1-mesa-dev libssl-dev zlib1g-dev pkg-config
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Windows (MSVC + vcpkg)
cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_INSTALLATION_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build
```

## Testing

```bash
# Tests are built automatically with the main project (BUILD_TESTING=ON by default)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure  # Linux/macOS
cd build && ctest --output-on-failure  # Windows
```

97 tests across 6 test classes: BackupFile (13), CryptoUtils streaming (7), ExtractionWorker (9), QSettings (6), Fuzz (15), CLI (39 — covers CrcDevice, path normalization, header iteration, single-file extraction, archive info, MCP protocol, and AgentConfigManager). Tests use QTest framework. Test entry point is `tests/tst_main.cpp` which runs all classes sequentially.

On headless Linux (CI), set `QT_QPA_PLATFORM=offscreen` because tests use QApplication.

## Linting

Code style is enforced by `.clang-format` (4-space indent, Linux brace style, pointer right-aligned). CI uses clang-format v18 via `cpp-linter/cpp-linter-action`.

```bash
# Format all source files
find src tests \( -name '*.cpp' -o -name '*.h' \) ! -name 'moc_*' | xargs clang-format -i

# Check without modifying
find src tests \( -name '*.cpp' -o -name '*.h' \) ! -name 'moc_*' | xargs clang-format --dry-run --Werror
```

## Architecture

```
src/
  main.cpp              Entry point. Two-phase init: argv[1] check routes to CLI
                        (QCoreApplication) or GUI (QApplication). --help intercepted
                        before QApplication to show subcommand listing.
  mainwindow.h/cpp      Main GUI window. Open file, validate, extract, show progress.
                        Tools menu: Install CLI, Manage AI Agent Integrations, Uninstall.
                        First-run setup dialog on initial launch.
  backupfile.h/cpp      .wpress format parser. Reads headers, extracts files, verifies CRC.
                        Has path traversal protection in buildOutputPath().
                        Public APIs: iterateHeaders(), extractSingleFile(), getArchiveInfo().
                        CrcDevice: QIODevice sink for streaming CRC without disk writes.
  cryptoutils.h/cpp     Streaming decompression (zlib, bzip2) and AES-256-CBC decryption.
  clihandler.h/cpp      CLI subcommand handlers: list, info, extract, cat, verify.
                        JSON output via --json flag. NDJSON for list/verify.
  mcpserver.h/cpp       MCP server (JSON-RPC 2.0 over stdio). 5 tools: list, info,
                        extract, cat, verify. Sub-routing for mcp register/unregister/status.
  agentconfig.h/cpp     AgentConfigManager: detects Claude Code + Gemini CLI, registers/
                        unregisters MCP server in their config files. Atomic writes via
                        QSaveFile. configRoot param for test isolation.
  installcli.h/cpp      install-cli: creates /usr/local/bin/traktor symlink on macOS
                        with osascript admin privileges. uninstall: removes symlink,
                        MCP configs, and QSettings.
  setupdialog.h/cpp     First-run Qt dialog. Detects agents, shows checkboxes, registers
                        selected agents on "Set Up". Re-accessible via Tools menu.
  extractionworker.h/cpp  QThread worker for background extraction with progress signals.
  autoextractor.h/cpp   Auto-extract mode: file queue, single-instance IPC via QLocalServer,
                        progress window, system tray notifications.
  progresswindow.h/cpp  Minimal progress window shown during auto-extract.
  passworddialog.h/cpp  Password prompt dialog for encrypted archives.
  dropoverlay.h/cpp     Drag-and-drop overlay widget.
  appdelegate.h/cpp     macOS QEvent::FileOpen handling.
  dockprogress.h/mm     macOS Dock badge progress (Objective-C). Stub for other platforms.

tests/
  tst_backupfile.cpp        Archive parsing and extraction tests.
  tst_cryptoutils_streaming.cpp  Streaming decompression/decryption tests.
  tst_extractionworker.cpp  Worker thread signal and extraction tests.
  tst_qsettings.cpp         Settings persistence tests.
  tst_fuzz.cpp              Fuzz tests: truncated, corrupted, oversized, path traversal,
                            garbage input, malformed JSON, boundary sizes.
  tst_cli.cpp               CLI, MCP, CrcDevice, AgentConfigManager tests.
  generate_fixtures.cpp     Standalone tool to generate test .wpress fixtures.
  fixtures/                 Generated test archives (plain, empty, corrupted, multifile,
                            v2crc, compressed).

scripts/macos-pkg/
  postinstall             PKG post-install: creates /usr/local/bin/traktor symlink.
  distribution.xml        PKG installer UI: welcome, conclusion, title.
  resources/              welcome.html, conclusion.html for installer screens.
```

## Key conventions

- Source files live in `src/`, tests in `tests/`, vendored deps in `vendor/`.
- Build system is CMake (`CMakeLists.txt`, `tests/CMakeLists.txt`).
- Qt 6.8 LTS. Minimum macOS 11.0 (Big Sur).
- C++17 standard.
- Conventional commits required for PR titles (`feat:`, `fix:`, `docs:`, `test:`, `refactor:`, `build:`, `ci:`, `chore:`).
- `moc_*.cpp`, `ui_*.h` are generated by Qt and listed in `.gitignore`. Never edit or commit them.
- `vendor/bzip2-1.0.8/` is vendored. Do not modify.

## CI

- `ci.yml`: Runs on PRs and pushes to master. Builds and tests on Linux, macOS, Windows with Qt 6.8 via install-qt-action. Lints with clang-format v18. Validates conventional commit PR titles. Uploads build artifacts (PKG + app tar for macOS) on PRs.
- `release.yml`: Triggered by GitHub Release publish. Builds universal macOS PKG (via CMAKE_OSX_ARCHITECTURES), Windows installer (Qt IFW), Linux AppImage. Uploads to release. Submits to VirusTotal. Auto-updates Homebrew Cask in servmask/homebrew-traktor.
- `release-please.yml`: Auto-creates release PRs with changelog from conventional commits.
- `label.yml`: Auto-labels PRs by file path.

## Release process

1. Merge PRs to master with conventional commit titles.
2. release-please auto-creates/updates a Release PR with changelog.
3. Merge the Release PR when ready to cut a release.
4. release-please creates a GitHub Release, which triggers `release.yml`.
5. Builds are uploaded to the release. VirusTotal scans are appended to release notes.
6. Homebrew Cask (servmask/homebrew-traktor) is auto-updated with new version + SHA256.

## Security notes

- `backupfile.h` `buildOutputPath()` sanitizes `../` in archive paths to prevent directory traversal. Fuzz tests verify this.
- Encrypted archives use AES-256-CBC via OpenSSL.
- The app makes zero network requests. Fully offline.
