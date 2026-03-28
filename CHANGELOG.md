# Changelog

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
