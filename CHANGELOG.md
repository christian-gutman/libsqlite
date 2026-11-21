# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and this project adheres to [Semantic Versioning](https://semver.org/).

Version numbers follow `major.minor.patch`, where `major` is `0` while the
API is considered unstable. The embedded SQLite engine version is reported
separately by `sqlite_core_sqlite_version()`.

## [Unreleased]

### Fixed

- **Linux/Unix compilation**: `sqlite_core_builder.c` now includes
  `<stdint.h>` explicitly. `SIZE_MAX` was only available through headers
  MSVC pulls in transitively, so the shared library failed to build with
  gcc/clang (found by the CI Linux jobs).
- **Unix linkage**: the library now links `Threads::Threads`,
  `${CMAKE_DL_LIBS}` and `m` explicitly. Required by `SQLITE_THREADSAFE=1`
  and by the engine's runtime extension/math use on distributions where
  glibc has not merged them into libc.
- **Regression suite on Unix**: the long-string conversion check no longer
  assumes Windows-only ANSI conversion; on Linux/macOS the documented
  UTF-8 passthrough is verified instead (this was the CI macOS failure).

### Documentation

- The project is referred to as **libsqlite** in all documentation
  (the repository name); the produced artifacts keep their historical
  names (`libsqlite3.dll`, `libsqlite3.so`, `libsqlite3.dylib`).

## [1.0.0] — 2026-11-21

First stable release. The library was audited end to end, the public API was
renamed and frozen, the build is cross-platform, and the .NET behavior
contract is enforced by tests.

### Added

- **Stable public API in English**: 31 exported symbols under the
  `sqlite_core_*` prefix (e.g. `sqlite_core_open`,
  `sqlite_core_execute_reader`, `sqlite_core_insert_json`) with types
  (`sqlite_core_param_t`, ...) and the `SQLITE_CORE_API` export macro.
  `sqlite.def` is the single source of truth for Windows exports.
- **`sqlite_core_execute_reader_to_json`**: result set as an RFC 8259 JSON
  array of objects (`[]` when empty).
- **`sqlite_core_insert_json`**: bulk insert of a JSON array of objects,
  parsed server-side by the embedded SQLite JSON1 engine — nested documents,
  escaped strings and numeric formats handled natively; values are bound as
  parameters (no SQL injection); the batch runs in a transaction the library
  owns only when none is active.
- **`sqlite_core_open_ex` / `sqlite_core_open_uri_readonly`**: full flag
  control and URI-mode read-only open without touching the global
  `sqlite3_config`.
- **`SQLITE_CORE_BUFFER_TOO_SMALL`** (`-2`) for `sqlite_core_get_blob`.
- **Test suites** (CTest): `regression` (audited bug fixes), `api`
  (.NET parity: named parameters, output parameters via `RETURNING`, reader
  callback abort, error paths, busy-timeout clamping, encoding round-trip)
  and `exports` (all 31 public symbols resolve at runtime).
- **GitHub Actions CI** (`.github/workflows/ci.yml`): shared-build matrix on
  Windows Win32/x64, Linux and macOS, plus a static-build job; every job
  runs `ctest`.
- **Documentation**: `docs/behavior.md` (behavioral contract with .NET) and
  `docs/behavior.es.md` (Spanish), bilingual `README.md` / `README.es.md`
  with quickstart and API reference, and `CHANGELOG.md`.
- **`LICENSE`**: MIT (© 2026 Christian Gutman).

### Changed

- **Cross-platform build**: one CMake project builds on Windows, Linux and
  macOS, shared or static (`BUILD_SHARED_LIBS`), with unified output layout
  `bin/<arch>-<os>/{shared,static}/<config>/`.
- **Self-contained sources**: the only pieces used from the former external
  dependencies (core-c-sdk / libstring) were absorbed into
  `src/sqlite/core/compat/` — export macro, encoding-detection family and
  case-insensitive comparators. No network access at build time.
- **Unified error contract**: return-value table documented in
  `docs/behavior.md` (`>= 0` success, `-1` generic error, `-2`
  buffer-too-small, negative SQLite codes for engine failures).
- **Unified encoding**: every `Execute*` entry point accepts ANSI or UTF-8
  SQL text and converts transparently on Windows.
- Version is parsed from `sqlite_core.h` (single source); the Windows
  resource file is generated from it so the DLL version can never drift.

### Fixed

- `sqlite_core_is_readonly` returned the inverted result.
- `sqlite_core_insert_json` dropped the last character of the document,
  concatenated values into the SQL (injection) and stole transactions owned
  by the caller.
- The JSON writer escaped incorrectly, mishandled UTF-8 and did not emit a
  valid array wrapper (rewritten).
- `sqlite_core_get_blob` was declared but not implemented.
- `_alloca` usage replaced with heap allocation (no stack overflow on large
  buffers).
- SQL text conversion to UTF-8 was inconsistent between entry points.
- URI opens required a global `sqlite3_config` side effect.
- Integer values were truncated below 64 bits.
- Diagnostics/error messages are now English and consistent.

### Removed

- FoxPro FLL remnants (`cmp_fll.bat`), the out-of-tree `shell.c` copy,
  `sqlite3ext.h` and the drifted in-tree doc header.

### Breaking

- The public API was renamed to English with **no aliases**: the previous
  `SqlLite_Core_*` names no longer exist. Rename your calls to the
  `sqlite_core_*` equivalents listed in `README.md`.

[Unreleased]: https://github.com/christian-gutman/libsqlite/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/christian-gutman/libsqlite/releases/tag/v1.0.0