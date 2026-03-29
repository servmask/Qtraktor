# Changelog

## [1.7.1](https://github.com/servmask/Qtraktor/compare/v1.7.0...v1.7.1) (2026-03-29)


### Bug Fixes

* keep QtDBus.framework in macOS bundle (QtGui runtime dependency) ([#30](https://github.com/servmask/Qtraktor/issues/30)) ([d3bb265](https://github.com/servmask/Qtraktor/commit/d3bb2658359e0f38976eab090504037640e4040a))
* update Homebrew Cask template in release.yml to use pkg directive ([49273f0](https://github.com/servmask/Qtraktor/commit/49273f0ddbf4bee3cc5a2ee2799aedb35e551600))


### Documentation

* fix download page to match .pkg assets instead of .dmg ([a2ab180](https://github.com/servmask/Qtraktor/commit/a2ab18078d37b26ca6ba2348cdc377887963e523))

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
