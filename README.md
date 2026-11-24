<div align="center">

# libsqlite

**High-performance SQLite connector library for C**

![CI](https://img.shields.io/github/actions/workflow/status/christian-gutman/libsqlite/ci.yml?branch=main&label=CI&style=flat-square)
![Release](https://img.shields.io/github/v/release/christian-gutman/libsqlite?style=flat-square)
![License](https://img.shields.io/github/license/christian-gutman/libsqlite?style=flat-square)
![SQLite](https://img.shields.io/badge/SQLite-3.54.1-003B57?style=flat-square&logo=sqlite&logoColor=white)
![C11](https://img.shields.io/badge/C-11-00599C?style=flat-square&logo=c&logoColor=white)
![CMake](https://img.shields.io/badge/build-CMake-064F8C?style=flat-square&logo=cmake&logoColor=white)
![Platforms](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue?style=flat-square)
![CTest](https://img.shields.io/badge/CTest-3%20suites-brightgreen?style=flat-square)

**[Documentación en español](README.es.md)** · **[Behavior contract](docs/behavior.md)** · **[Roadmap](docs/ROADMAP.md)** · **[Changelog](CHANGELOG.md)** · **[License](LICENSE)**

</div>

> **Project note:** this codebase originally belonged to the `@christiangutman`
> account. After a hardware theft, access to that account and its original
> repository was permanently lost. The sources were recovered intact from
> external backups and are now released publicly. **This repository is the
> official and permanent home** for libsqlite — bug reports and contributions
> are welcome here.

libsqlite is a connector for SQLite databases written in portable C99/C11 on
top of the official `sqlite3.c` amalgamation. It exports a shared library
(`.dll` / `.so` / `.dylib`) with ADO.NET-style semantics
(`ExecuteReader`, `ExecuteScalar`, `ExecuteNonQuery`, parameters with
directions, transactions), callable from any language that can call C
(C#, C++, Delphi, VB.NET, Python via ctypes, ...).

The bundled engine means there is no ODBC, no OLE DB provider and no system
SQLite installation to deploy — ideal for XCOPY deployments.

**[Documentación en español](README.es.md)**

## Features

- **Embedded engine** — the official SQLite amalgamation (3.54.1) is compiled
  in; nothing else has to be installed on the target machine.
- **.NET semantics** — the API mirrors its ADO.NET counterparts, so the
  learning curve is minimal for .NET developers.
- **UTF-8 / ANSI conversion** — encoding is detected, not assumed
  (`sqlite_core_string_utf8_to_ansi`, `sqlite_core_string_ansi_to_utf8`).
- **Dynamic JSON output** — built-in JSON writer callback and one-call
  `sqlite_core_execute_reader_to_json`; JSON1-based bulk
  `sqlite_core_insert_json`.
- **Fully cross-platform** — one build system (CMake) for Windows (Win32/x64),
  Linux and macOS, shared or static.
- **No network at build time** — the only external pieces were absorbed into
  `src/sqlite/core/compat/`.
- **Frozen exports** — `sqlite.def` is the source of truth; an automated test
  verifies every public symbol at runtime.

## Requirements

| Platform | Toolchain |
|----------|-----------|
| Windows | Visual Studio 2022 (Desktop C++ workload) or equivalent, CMake ≥ 3.15 |
| Linux   | GCC or Clang, `build-essential`, CMake ≥ 3.15 |
| macOS   | Xcode command line tools, CMake ≥ 3.15 |

The code is standard C11 and compiles warning-clean (`/W4` on MSVC).

## Quickstart

Clone and build a shared Win32 DLL (or use the included helper
`cmp_dll.bat`, which does the same and cleans up afterwards):

```bash
git clone https://github.com/christian-gutman/libsqlite.git
cd libsqlite

# Windows x64 (use -A Win32 for 32-bit)
cmake -S src/sqlite -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure   # optional: run tests
```

Linux:

```bash
sudo apt-get install -y build-essential cmake
cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

macOS:

```bash
xcode-select --install
cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Build outputs are centralized under `bin/<arch>-<os>/{shared,static}/<config>/`:

```
bin/x64-Windows/shared/Release/libsqlite3.dll     + libsqlite3.lib (import library)
bin/x32-Windows/static/Release/libsqlite3.lib     (static library)
bin/x64-Linux/shared/Release/libsqlite3.so
bin/arm64-Darwin/shared/Release/libsqlite3.dylib
```

### CMake options

| Option | Default | Meaning |
|--------|---------|---------|
| `BUILD_SHARED_LIBS` | `ON` | Shared library (`ON`) or static library (`OFF`). |
| `LIBSQLITE3_BUILD_TESTS` | `ON` | Build the CTest suites (`regression`, `api`, `exports`). |
| `CMAKE_BUILD_TYPE` | `Release` | Single-config generators (Linux/macOS). |

`cmake --install build` installs the library, the headers and a CMake package
(`libsqlite3::libsqlite3`) for `find_package(libsqlite3)` consumers.

### Using the library

`src/sqlite/core/sqlite_core.h` is the umbrella header — include it and link
against the produced library:

```c
#include <sqlite_core.h>
#include <stdio.h>

/* One call per row; count them (DLL builds export only the 31
 * sqlite_core_* symbols, so this example prints rows as JSON below). */
static int on_row(sqlite3_stmt *stmt, int columns, int hasrows, void *ctx)
{
    (void)stmt; (void)columns;
    if (hasrows)
        (*(int *)ctx)++;
    return 0; /* non-zero aborts the iteration */
}

int main(void)
{
    int open_rc = 0;
    sqlite3 *db = sqlite_core_open_or_create("demo.db", &open_rc);
    if (db == NULL || open_rc != SQLITE_OK)
    {
        fprintf(stderr, "cannot open demo.db (rc=%d)\n", open_rc);
        return 1;
    }

    char err[128] = "";
    if (sqlite_core_execute_command(db,
            "CREATE TABLE IF NOT EXISTS person(id INTEGER PRIMARY KEY, name TEXT)",
            err, sizeof(err)) != SQLITE_OK)
    {
        fprintf(stderr, "ddl failed: %s\n", err);
        sqlite_core_close(db);
        return 1;
    }

    sqlite_core_param_t p[2];
    memset(&p, 0, sizeof(p));
    p[0].name = "id";
    p[0].type = SQLITE_CORE_TYPE_INT;
    p[0].Direction = SQLITE_CORE_DIRECTION_INPUT;
    p[0].value.iVal = 42;

    p[1].name = "name";
    p[1].type = SQLITE_CORE_TYPE_TEXT;
    p[1].Direction = SQLITE_CORE_DIRECTION_INPUT;
    p[1].value.blob_or_text.ptr = "Ada";
    p[1].value.blob_or_text.len = -1;              /* NUL-terminated */

    int rc = sqlite_core_execute_non_query(db,
        "INSERT INTO person(id, name) VALUES (:id, :name)", p, 2, err, sizeof(err));
    if (rc != 1) /* value functions return the affected rows */
    {
        fprintf(stderr, "insert failed (rc=%d): %s\n", rc, err);
        sqlite_core_close(db);
        return 1;
    }

    int seen = 0;
    rc = sqlite_core_execute_reader(db,
        "SELECT id, name FROM person ORDER BY id", NULL, 0,
        on_row, &seen, err, sizeof(err));
    if (rc < 0)
    {
        fprintf(stderr, "select failed (rc=%d): %s\n", rc, err);
        sqlite_core_close(db);
        return 1;
    }
    printf("reader saw %d rows\n", seen);

    /* Print the same rows as JSON (works on DLL and static builds). */
    char *json = NULL;
    rc = sqlite_core_execute_reader_to_json(db,
        "SELECT id, name FROM person ORDER BY id", NULL, 0, &json, err, sizeof(err));
    if (rc < 0 || json == NULL)
    {
        fprintf(stderr, "json failed (rc=%d): %s\n", rc, err);
        sqlite_core_close(db);
        return 1;
    }
    printf("json: %s\n", json);
    sqlite_core_string_free(json);

    if (sqlite_core_close(db) != 1)
    {
        fprintf(stderr, "close failed\n");
        return 1;
    }
    return 0;
}
```

MSVC (Developer prompt, DLL and import lib on the paths shown above):

```bat
cl /W4 /I src\sqlite\core demo.c bin\x64-Windows\shared\Release\libsqlite3.lib
```

GCC/Clang:

```bash
cc -I src/sqlite/core demo.c \
   -L bin/x64-Linux/shared/Release -llibsqlite3 \
   -Wl,-rpath,'$ORIGIN' -o demo
```

Strings returned by the library (for example from
`sqlite_core_execute_reader_to_json`) are heap-allocated by the SQLite
allocator and must be released with `sqlite_core_string_free`.

## Tests

```bash
# Windows
ctest --test-dir build -C Release --output-on-failure
# Linux / macOS
ctest --test-dir build --output-on-failure
```

| Suite | What it verifies |
|-------|------------------|
| `regression` | The audited bug fixes: readonly inversion, JSON writer escaping/truncation, injection, transaction theft, 64-bit integers, BLOB retrieval, ANSI→UTF-8 conversion. |
| `api` | .NET parity: named parameters and binding by name, output parameters via `RETURNING`, reader callback abort and empty sets, error paths, busy-timeout clamping, encoding round-trip. |
| `exports` | Every public symbol of the API is actually exported by the shared library (`sqlite.def` cannot drift from the headers). |

The same suites run on every push through GitHub Actions
(`.github/workflows/ci.yml`): shared matrix on Windows Win32/x64, Linux and
macOS, plus a static-build job.

## API reference

Version macros: `SQLITE_CORE_VERSION_STR` (`"1.0.0"`, this wrapper) and
`SQLITE_CORE_SQLITE_VERSION_STR` (embedded engine, `"3.54.1"`).

### Core types

```c
typedef enum { SQLITE_CORE_TYPE_NULL, SQLITE_CORE_TYPE_INT,
               SQLITE_CORE_TYPE_DOUBLE, SQLITE_CORE_TYPE_TEXT,
               SQLITE_CORE_TYPE_BLOB } sqlite_core_param_type_t;

typedef enum { SQLITE_CORE_DIRECTION_INPUT, SQLITE_CORE_DIRECTION_OUTPUT,
               SQLITE_CORE_DIRECTION_INPUTOUTPUT } sqlite_core_param_direction_t;

typedef struct {
    const char *name;
    int no_insensitive;                  /* 1 = case-sensitive name match  */
    sqlite_core_param_type_t type;
    sqlite_core_param_direction_t Direction;
    union {
        double dVal;
        sqlite3_int64 iVal;
        struct { char *ptr; int len; int max_len; } blob_or_text;
    } value;
} sqlite_core_param_t;

typedef int (*sqlite_core_row_callback_fn)(
    sqlite3_stmt *stmt, int columns, int hasrows, void *pUserContext);
```

Constants: `SQLITE_CORE_BUFFER_TOO_SMALL` (`-2`, `sqlite_core_get_blob`).

### Error diagnostics — `sqlite_core_error`

| Function | Description |
|----------|-------------|
| `int sqlite_core_error_code(sqlite3 *db)` | Last result code (`SqlException.Number`). `0` for `NULL`. |
| `int sqlite_core_error_code_extended(sqlite3 *db)` | Extended result code. |
| `const char *sqlite_core_error_message(sqlite3 *db)` | Last error message. |

### Strings and encoding — `sqlite_core_string`

| Function | Description |
|----------|-------------|
| `char *sqlite_core_string_format(const char *fmt, ...)` | `sprintf`-style, heap-allocated result. |
| `char *sqlite_core_string_concat(const char *a, const char *b)` | Concatenation; tolerates `NULL` operands. |
| `void sqlite_core_string_free(const char *s)` | Frees any string returned by the library; no-op on `NULL`. |
| `char *sqlite_core_string_utf8_to_ansi(const char *s)` | UTF-8 → ANSI (current code page on Windows). |
| `char *sqlite_core_string_ansi_to_utf8(const char *s)` | ANSI → UTF-8. |

UTF-8/ANSI conversions are needed only where Windows ANSI code pages are
involved; SQL text is accepted in both encodings and converted internally.

### Connections — `sqlite_core_cnx`

| Function | Description |
|----------|-------------|
| `sqlite3 *sqlite_core_open(const char *path, int *out_rc)` | Opens an existing file; fails (`SQLITE_CANTOPEN`) if missing. |
| `sqlite3 *sqlite_core_open_or_create(const char *path, int *out_rc)` | Creates the file when missing. |
| `sqlite3 *sqlite_core_open_readonly(const char *path, int *out_rc)` | Read-only; fails if the file is missing. |
| `sqlite3 *sqlite_core_open_uri_readonly(const char *path, int *out_rc)` | Read-only with URI interpretation, without touching global config. |
| `sqlite3 *sqlite_core_open_ex(const char *path, int flags, int *out_rc)` | Full control with raw `SQLITE_OPEN_*` flags. |
| `int sqlite_core_close(sqlite3 *db)` | Idempotent close; safe on `NULL`; `1` on success. |
| `int sqlite_core_is_readonly(sqlite3 *db)` | `1` if read-only, `0` if writable, `-1` on error. |
| `sqlite3_int64 sqlite_core_last_insert_rowid(sqlite3 *db)` | Rowid of the last successful INSERT (`0` if none or `db` is NULL). |
| `int sqlite_core_set_busy_timeout(sqlite3 *db, int ms)` | Accepts 0..30000 ms, clamps out-of-range values. |

### Parameters — `sqlite_core_parameter`

| Function | Description |
|----------|-------------|
| `const char *sqlite_core_param_clean_prefix(const char *name)` | Strips the `:` / `@` / `?` sigil (`":id"` → `"id"`). |

Binding is always by name, never by ordinal: a parameter missing from the SQL
is simply not bound, matching ADO.NET behavior.

### Commands — `sqlite_core_command`

| Function | Description |
|----------|-------------|
| `int sqlite_core_execute_command(sqlite3 *db, const char *sql, char *err_buf, int err_buf_max)` | Runs one or more statements (DDL/batch). |
| `int sqlite_core_execute_non_query(sqlite3 *db, const char *sql, sqlite_core_param_t *params, int count, char *err_buf, int max)` | `ExecuteNonQuery`; single statement; returns affected rows. |
| `int sqlite_core_execute_scalar(sqlite3 *db, const char *sql, sqlite_core_param_t *params, int count, sqlite_core_param_t *out, char *err_buf, int max)` | First column of the first row; `SQLITE_CORE_TYPE_NULL` when empty. |
| `int sqlite_core_execute_reader(sqlite3 *db, const char *sql, sqlite_core_param_t *params, int count, sqlite_core_row_callback_fn cb, void *ctx, char *err_buf, int max)` | `ExecuteReader`; one callback per row; callback returns non-zero to abort. |
| `int sqlite_core_execute_transaction(sqlite3 *db, const char *sql, int txexpected, char *err_buf, int max)` | Runs a statement expecting to be inside a transaction. |

### Transactions — `sqlite_core_transaction`

| Function | Description |
|----------|-------------|
| `int sqlite_core_begin_transaction(sqlite3 *db, char *err_buf, int max)` | `BEGIN IMMEDIATE`; nested begins fail. |
| `int sqlite_core_commit_transaction(sqlite3 *db, char *err_buf, int max)` | Commits; fails without an active transaction. |
| `int sqlite_core_rollback_transaction(sqlite3 *db, char *err_buf, int max)` | Rolls back; fails without an active transaction. |

### JSON — `sqlite_core_builder`

| Function | Description |
|----------|-------------|
| `int sqlite_core_execute_reader_to_json(sqlite3 *db, const char *sql, sqlite_core_param_t *params, int count, char **out_json, char *err_buf, int max)` | Result set as a JSON array of objects; `[]` when empty; free with `sqlite_core_string_free`. |
| `int sqlite_core_insert_json(sqlite3 *db, const char *table, const char *json, char *err_buf, int max)` | Bulk insert of a JSON array of objects (parsed by JSON1); returns rows inserted or a negative SQLite code. |
| `int sqlite_core_json_writer_callback(sqlite3_stmt *stmt, int columns, int hasrows, void *ctx)` | Ready-made `sqlite_core_row_callback_fn` that accumulates JSON into a `sqlite_core_json_builder_t`. |

### Fields — `sqlite_core_field`

| Function | Description |
|----------|-------------|
| `int sqlite_core_get_blob(sqlite3 *db, const char *sql, void *dest, int max_len, int *copied)` | Copies a BLOB into the caller's buffer; `SQLITE_CORE_BUFFER_TOO_SMALL` (-2) when it does not fit. |
| `int sqlite_core_insert_blob(sqlite3 *db, const char *table, const char *column, sqlite3_int64 rowid, const char *file_path, int chunk_size, int *written, char *err, int max)` | Streams a **file** into a BLOB column in chunks (64 KiB default). `rowid == 0` inserts; `rowid > 0` upserts that rowid. Returns bytes written, or `SQLITE_CORE_BLOB_TOO_SMALL` (-3) when the target blob is smaller than the file. |
| `int sqlite_core_update_blob(sqlite3 *db, const char *table, const char *column, sqlite3_int64 rowid, const char *file_path, int chunk_size, int *written, char *err, int max)` | Same chunked streaming onto an existing row; the column is never grown silently. |

### Version

| Function | Description |
|----------|-------------|
| `const char *sqlite_core_version(void)` | Wrapper version, `"1.0.0"`. |
| `const char *sqlite_core_sqlite_version(void)` | Embedded engine version, `"3.54.1"`. |

### JSON in one look

```c
char *json = NULL;
if (sqlite_core_execute_reader_to_json(
        db, "SELECT id, name FROM person ORDER BY id",
        NULL, 0, &json, NULL, 0) >= 0 && json != NULL)
{
    /* json holds:
     * [
     *   {"id": 1, "name": "Ada"},
     *   {"id": 42, "name": "Grace"}
     * ]
     */
    sqlite_core_string_free(json);
}
```

`sqlite_core_insert_json` is the mirror image: feed it the same document and
a table name, and the rows are inserted through parsed, parameter-bound
statements (no string concatenation, no injection).

## Documentation

| Document | Contents |
|----------|----------|
| [docs/behavior.md](docs/behavior.md) | Behavioral contract with .NET: mapping per function, return codes, JSON, encoding (the acceptance contract) |
| [docs/pragmas.md](docs/pragmas.md) | PRAGMA bundle (WAL, synchronous, busy_timeout, foreign_keys, temp_store): what each one does, scope and how to apply it at open time |
| [docs/behavior.es.md](docs/behavior.es.md) | Same contract, in Spanish |
| [README.es.md](README.es.md) | This README in Spanish |
| [docs/ROADMAP.md](docs/ROADMAP.md) | What is pending: Android/Flutter, iOS, WASM, FoxPro FLL, release |
| [CHANGELOG.md](CHANGELOG.md) | Release history and breaking changes |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Build/test workflow, rules for extending the frozen API |
| [SECURITY.md](SECURITY.md) | How to report vulnerabilities |

## Bindings to other languages

The frozen API is wrapped for other languages with a canonical example in
each one (same flow in every language, so results can be compared):

| Language | Folder |
|----------|--------|
| C (native) | [`src/sqlite/examples`](src/sqlite/examples) |
| Python | [`bindings/python`](bindings/python) |
| C# / .NET | [`bindings/csharp`](bindings/csharp) |
| C++ | [`bindings/cpp`](bindings/cpp) |
| Swift | [`bindings/swift`](bindings/swift) |
| Go | [`bindings/go`](bindings/go) |
| Rust | [`bindings/rust`](bindings/rust) |
| Node.js | [`bindings/nodejs`](bindings/nodejs) |
| Java | [`bindings/java`](bindings/java) |
| R | [`bindings/r`](bindings/r) |
| Ruby | [`bindings/ruby`](bindings/ruby) |
| Julia | [`bindings/julia`](bindings/julia) |
| Fortran | [`bindings/fortran`](bindings/fortran) |
| Visual FoxPro (legacy) | [`bindings/foxpro`](bindings/foxpro) |
| Clipper (legacy) | [`bindings/clipper`](bindings/clipper) |

Full table, rules (allocator ownership, callbacks, parameter struct) and
the remaining languages: **[bindings/README.md](bindings/README.md)**.

## Examples

Four validated programs live in [`src/sqlite/examples/`](src/sqlite/examples/).
They are built by default and run as CTest smoke tests
(`ctest -R example`), so they are verified on every platform in CI:

| File | What it shows |
|------|---------------|
| [basic.c](src/sqlite/examples/basic.c) | Open, DDL, insert, reader callback, rows printed as JSON |
| [parameters.c](src/sqlite/examples/parameters.c) | Named parameters, `clean_prefix`, scalar reads, output parameters via `RETURNING` |
| [transaction.c](src/sqlite/examples/transaction.c) | `begin` / `commit` / `rollback` and their failure paths |
| [json.c](src/sqlite/examples/json.c) | `execute_reader_to_json` and `insert_json` in both directions |

> Windows shared builds export exactly the 34 `sqlite_core_*` symbols
> (`sqlite.def`): direct engine calls (`sqlite3_column_*`, ...) need a
> **static** link, which is why the examples stay on the wrapper API.

## Building in a container (Docker)

Local build without installing toolchains on the host
([`docker/Dockerfile`](docker/Dockerfile)):

```bash
# Build the image and run the full CTest suite (tests + examples).
docker build -f docker/Dockerfile -t libsqlite:local .

# Extract the Linux binaries (shared + static) when it passes.
docker run --rm -v "$(pwd)/bin-local:/bin-local" \
    libsqlite:local bash -c "cp -r bin/* /bin-local/"
```

(PowerShell: `docker run --rm -v "${PWD}/bin-local:/bin-local" ...`.)

### Binding containers

Every verified binding also runs in its own Linux container: the image
builds the library, runs the full CTest suite and then compiles and runs
the language example against the real `.so`, and is removed at the end
(`docker system df` stays flat):

```bat
docker\build_all.cmd            :: python csharp cpp swift ruby julia fortran go rust nodejs java r
docker\build.cmd python         :: a single language
```

FoxPro and Clipper are excluded: they load the Windows DLL from a
16/32-bit legacy runtime and have no Linux container. Full matrix in
[`docker/readme.md`](docker/readme.md), per-language rules in
[`bindings/README.md`](bindings/README.md).

## Contributing & security

Bug reports and feature requests use the GitHub issue templates (they ask
for version, platform, linkage and a minimal reproduction), and pull
requests follow the checklist in
[.github/PULL_REQUEST_TEMPLATE.md](.github/PULL_REQUEST_TEMPLATE.md).
Contributor build/test workflow and API extension rules:
[CONTRIBUTING.md](CONTRIBUTING.md). Please report vulnerabilities privately
as described in [SECURITY.md](SECURITY.md).

## Behavior contract

The observable behavior of every function is documented as the acceptance
contract in **[docs/behavior.md](docs/behavior.md)**: return-code table,
`.NET` mapping per function, JSON and encoding rules. The regression and API
suites implement every rule of that document.

## Repository layout

```
libsqlite/
├── src/sqlite/
│   ├── CMakeLists.txt           Options, version parsing, install/export
│   ├── cmake/                   CMake package template
│   ├── core/
│   │   ├── sqlite_core.h        Umbrella public header — include this one
│   │   ├── sqlite_core_*.{c,h}  API modules (error, string, cnx, parameter,
│   │   │                        command, transaction, builder, field)
│   │   ├── compat/              Absorbed portability layer (SQLITE_CORE_API,
│   │   │                        encoding detection, case-insensitive compares)
│   │   ├── engine/              Embedded SQLite amalgamation (unmodified)
│   │   └── sqlite.def           Frozen export list (Windows)
│   ├── tests/                   CTest suites: regression, api, exports
│   └── examples/                Usage examples (planned)
├── bin/<arch>-<os>/{shared,static}/<config>/   Build output (git-ignored)
├── docs/behavior.md             .NET parity contract (English)
├── .github/workflows/ci.yml     CI: build matrix + ctest
├── LICENSE                      MIT
└── cmp_dll.bat                  Convenience script: Win32 shared build
```

## Third-party components

- **SQLite engine** (`src/sqlite/core/engine/sqlite3.c/.h`): the official
  amalgamation, unmodified, placed in the **public domain**
  (<https://sqlite.org/copyright.html>).
- **Portability layer** (`src/sqlite/core/compat/`): only the pieces actually
  used from the former external dependencies (core-c-sdk / libstring): the
  export macro, the encoding-detection family and the case-insensitive
  comparators. Everything is plain C99/C11 plus the narrowest possible
  platform shims, so no runtime dependency remains.

## License

MIT — see [LICENSE](LICENSE). © 2026 Christian Gutman.

The embedded SQLite engine is public domain, as stated above.

---

**libsqlite** · v1.0.0 · [English](README.md) · [Español](README.es.md)