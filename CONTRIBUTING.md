# Contributing to Traktor

Thank you for your interest in contributing to Traktor! This guide will help you get started.

## Getting Started

See the [README](README.md) for build instructions on macOS, Windows, and Linux.

### Running the Test Suite

```bash
cd tests
qmake tests.pro
make -j$(nproc)        # Linux
make -j$(sysctl -n hw.ncpu)  # macOS
./tst_qtraktor
```

On Windows with MSVC:

```powershell
cd tests
qmake.exe tests.pro
nmake -f Makefile.Release
release\tst_qtraktor.exe
```

## Development Workflow

1. Fork the repository
2. Create a feature branch from `master`
3. Make your changes
4. Ensure tests pass
5. Open a pull request

### Commit Messages

This project uses [Conventional Commits](https://www.conventionalcommits.org/). Your PR title must follow this format:

```
type(optional scope): description
```

**Examples:**

- `feat: add bzip2 progress reporting`
- `fix: handle empty encrypted files`
- `docs: update build instructions for Qt 6`

Allowed types: `feat`, `fix`, `chore`, `docs`, `test`, `refactor`, `build`, `ci`

## Code Style

This project uses `clang-format` to enforce a consistent code style. The configuration is in `.clang-format` at the repo root.

### Automatic formatting (recommended)

Install the [pre-commit](https://pre-commit.com/) framework:

```bash
pip install pre-commit
pre-commit install
```

This automatically formats your code on every `git commit`. If formatting changes are needed, the commit is aborted and the files are reformatted. Stage the reformatted files and commit again.

### Manual formatting

```bash
find src tests \( -name '*.cpp' -o -name '*.h' \) | xargs clang-format -i
```

CI will reject PRs with style violations regardless of which method you use.

## Testing

All new functionality should include tests. Tests use the [Qt Test](https://doc.qt.io/qt-5/qtest-overview.html) framework.

Test files live in `tests/` and follow the naming convention `tst_<module>.cpp`.

## PR Guidelines

- Use conventional commit format for your PR title
- Fill out the PR template checklist
- Ensure CI passes on all platforms (Linux, macOS, Windows)
- Link related issues using `Fixes #123` syntax

## Reporting Bugs

Use the [Bug Report](https://github.com/servmask/Qtraktor/issues/new?template=bug_report.yml) issue template.

## Requesting Features

Use the [Feature Request](https://github.com/servmask/Qtraktor/issues/new?template=feature_request.yml) issue template.

## Questions?

Open a discussion in [GitHub Discussions](https://github.com/servmask/Qtraktor/discussions).
