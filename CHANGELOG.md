# Changelog

## [1.11.0](https://github.com/servmask/Qtraktor/compare/v1.10.0...v1.11.0) (2026-05-12)


### Features

* **windows:** in-app auto-update via WinSparkle ([#48](https://github.com/servmask/Qtraktor/issues/48)) ([4fdc301](https://github.com/servmask/Qtraktor/commit/4fdc3012a44980806457b5f03bdc3addefb69ad1))

## [1.10.0](https://github.com/servmask/Qtraktor/compare/v1.9.1...v1.10.0) (2026-05-06)


### Features

* **macos:** in-app auto-update via Sparkle 2 ([#46](https://github.com/servmask/Qtraktor/issues/46)) ([ac34abb](https://github.com/servmask/Qtraktor/commit/ac34abbc6f7be15960bd79fc60b07a19dd0a2aa1))

## [1.9.1](https://github.com/servmask/Qtraktor/compare/v1.9.0...v1.9.1) (2026-05-05)


### Features

* **macOS Developer ID signing and notarization** ([#42](https://github.com/servmask/Qtraktor/issues/42))
  * Replaces ad-hoc signing with proper Developer ID Application + Installer certificates and Apple notarization (ASC API key auth)
  * Both the `.app` bundle (Sparkle-ready) and the `.pkg` installer are notarized, so first launch no longer trips Gatekeeper warnings

## [1.9.0](https://github.com/servmask/Qtraktor/compare/v1.8.0...v1.9.0) (2026-05-05)


### Features

* **Hybrid CLI/GUI mode** - split GUI behind a runtime probe ([#40](https://github.com/servmask/Qtraktor/issues/40))
  * Linux: thin executable links only `Qt6::Core` + `libdl`; the Widgets GUI lives in `libtraktor-gui.so` and is `dlopen`'d at runtime when `$DISPLAY`/`$WAYLAND_DISPLAY`, `libGL.so.1`, and the plugin all probe successfully - minimal/headless containers stay in CLI mode without loader errors
  * Windows: switched to the console subsystem so CLI subcommands pipe stdout cleanly from PowerShell/cmd; the console is detached only when Traktor owns it (never the user's inherited terminal) before the GUI event loop starts
  * Adds `--gui` / `--cli` / `--no-gui` flags, a global `--help` / `--version` handler, and refactors subcommand dispatch out of `main.cpp` into `clihandler.cpp` so the Linux thin exe and the macOS/Windows inline-GUI path share the same machinery
  * `cmdLegacyExtract` now honors the `TRAKTOR_PASSWORD` env var and uses `mkpath()` for parity with the new `extract` subcommand


### Bug Fixes

* remove extra escaping in Homebrew Cask URL template ([#35](https://github.com/servmask/Qtraktor/issues/35))


### Documentation

* update build instructions from Qt 5/qmake to Qt 6/CMake ([#37](https://github.com/servmask/Qtraktor/issues/37))


## [1.8.0](https://github.com/servmask/Qtraktor/compare/v1.7.0...v1.8.0) (2026-03-29)


### Features

* **Qt 6.8 LTS + CMake + C++17** - Full framework modernization ([#26](https://github.com/servmask/Qtraktor/issues/26)) ([fa019f3](https://github.com/servmask/Qtraktor/commit/fa019f3))
  * Upgraded from Qt 5.15 (end-of-life) to Qt 6.8 LTS (supported until 2028)
  * Migrated build system from qmake to CMake
  * Upgraded C++ standard from C++11 to C++17
  * macOS: minimum version raised to 11.0 (Big Sur), required by Qt 6
  * Windows: upgraded to MSVC 2022 and Qt 6.8
  * Leaner macOS app bundle - stripped unused Qt 6 frameworks (QML, SVG)


### Bug Fixes

* keep QtDBus.framework in macOS bundle, required by QtGui at runtime ([#30](https://github.com/servmask/Qtraktor/issues/30)) ([d3bb265](https://github.com/servmask/Qtraktor/commit/d3bb265))
* restore two-runner lipo matrix for macOS universal binary ([#31](https://github.com/servmask/Qtraktor/issues/31)) ([919ef16](https://github.com/servmask/Qtraktor/commit/919ef16))
* update Homebrew Cask template in release.yml to use pkg directive ([49273f0](https://github.com/servmask/Qtraktor/commit/49273f0))
* fix download page to match .pkg assets instead of .dmg ([a2ab180](https://github.com/servmask/Qtraktor/commit/a2ab180))
* use PAT for release-please to trigger release workflow ([6bbc44a](https://github.com/servmask/Qtraktor/commit/6bbc44a))

## [1.7.0](https://github.com/servmask/Qtraktor/compare/v1.6.0...v1.7.0) (2026-03-28)


### Features

* add CLI subcommands for AI-agent access to .wpress archives ([#23](https://github.com/servmask/Qtraktor/issues/23)) ([d013067](https://github.com/servmask/Qtraktor/commit/d013067e7093593008a6d9cb43c728dc8fec360c))
* add multi-agent MCP registration, setup dialog, and uninstall ([#24](https://github.com/servmask/Qtraktor/issues/24)) ([eb2956a](https://github.com/servmask/Qtraktor/commit/eb2956a4c65e1e7a9bcc2af4591c4c35c4af2b59))


### Documentation

* add CLAUDE.md with project context for AI-assisted development ([#22](https://github.com/servmask/Qtraktor/issues/22)) ([6c74de0](https://github.com/servmask/Qtraktor/commit/6c74de0267abbbc04b4e199a5f463d2bff622bca))
* add privacy policy and terms of use pages ([#21](https://github.com/servmask/Qtraktor/issues/21)) ([195f2de](https://github.com/servmask/Qtraktor/commit/195f2de3e03f6068de926d70d91279c626d13a98))

## [1.6.0](https://github.com/servmask/Qtraktor/compare/v1.5.0...v1.6.0) (2026-03-28)


### Features

* **archive:** add v2 archive format support with CRC32 verification ([#10](https://github.com/servmask/Qtraktor/issues/10)) ([7e29f10](https://github.com/servmask/Qtraktor/commit/7e29f10))
* **archive:** add Archive Utility-style native file handling ([#12](https://github.com/servmask/Qtraktor/issues/12)) ([ca18000](https://github.com/servmask/Qtraktor/commit/ca18000))
* **infrastructure:** add complete open source project infrastructure ([#13](https://github.com/servmask/Qtraktor/issues/13)) ([35a50da](https://github.com/servmask/Qtraktor/commit/35a50da))
* **site:** add GitHub Pages download site and remove BunnyCDN ([#17](https://github.com/servmask/Qtraktor/issues/17)) ([c42cfbf](https://github.com/servmask/Qtraktor/commit/c42cfbf))
* **ci:** upload build artifacts on PRs and post download links ([#16](https://github.com/servmask/Qtraktor/issues/16)) ([779d28a](https://github.com/servmask/Qtraktor/commit/779d28a))


### Bug Fixes

* handle existing extraction directory instead of failing silently ([#11](https://github.com/servmask/Qtraktor/issues/11)) ([a731133](https://github.com/servmask/Qtraktor/commit/a731133))
* rename release-please config to correct filename ([#14](https://github.com/servmask/Qtraktor/issues/14)) ([faf37bf](https://github.com/servmask/Qtraktor/commit/faf37bf))


### Security

* fix path traversal vulnerability in .wpress extractor
