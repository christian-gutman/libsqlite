# Contributing to libsqlite3

Thanks for your interest in improving libsqlite3! This document explains how
to build, test and extend the library so that every change lands consistent
with the project's contracts.

## Ground rules

- Code, comments, identifiers, messages and documentation are in **English**.
- [docs/behavior.md](docs/behavior.md) is the **acceptance contract**: the
  observable behavior of every function must match its .NET counterpart.
  Every rule there is enforced by the test suite.
- The public API (`sqlite_core_*`, 31 frozen symbols in
  `src/sqlite/core/sqlite.def`) only changes in a discussed major version.
  Helpers that start with `sqlite_core_` and lack `SQLITE_CORE_API` are not
  public and may change freely.
- `LICENSE` is MIT (© 2026 Christian Gutman). The embedded SQLite engine is
  public domain (<https://sqlite.org/copyright.html>).

## Build

```bash
# Windows x64 (use -A Win32 for 32-bit)
cmake -S src/sqlite -B build -A x64
cmake --build build --config Release

# Linux / macOS (add -DBUILD_SHARED_LIBS=OFF for a static build)
cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Test

```bash
# Windows
ctest --test-dir build -C Release --output-on-failure
# Linux / macOS
ctest --test-dir build --output-on-failure
```

| Suite | Purpose |
|-------|---------|
| `regression` | The audited bug fixes stay fixed. |
| `api` | .NET parity: parameters, outputs via `RETURNING`, callbacks, error paths. |
| `exports` | Every `SQLITE_CORE_API` function resolves in the shared library. |

A pull request that changes behavior must add or update the corresponding
checks; a behavior is done when its rule in `docs/behavior.md` has a test.

## Adding to the public API

1. Declare the function with `SQLITE_CORE_API` in the module header
   (`sqlite_core_cnx.h`, `sqlite_core_command.h`, ...).
2. Implement it in the matching `.c` — C11, no compiler warnings
   (`/W4` on MSVC, `-Wall -Wextra` on GCC/Clang).
3. Add the symbol to `src/sqlite/core/sqlite.def` **and** to
   `src/sqlite/tests/test_exports.c` (the `exports` CTest guards the list).
4. Document it in `docs/behavior.md` (both languages) and in the README API
   reference; add it to `CHANGELOG.md` under `Unreleased`.

## Commit messages

Conventional style with the milestone as the scope when relevant:

```
Milestone 6: fix X in sqlite_core_command
```

Keep the subject under ~72 characters; explain *why* in the body.

## Reporting bugs

Use the issue templates — they ask for the platform, the linkage
(shared/static), both version strings and a minimal reproduction.