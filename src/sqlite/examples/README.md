# Examples

Four short, self-contained programs for the libsqlite connector (C API).
Each one validates every public call and exits with the number of failed
checks, so they also run as CTest smoke tests (`example_basic`,
`example_parameters`, `example_transaction`, `example_json`).

| File | What it shows |
|------|---------------|
| `basic.c` | open or create, DDL, insert, read rows with a callback, close |
| `parameters.c` | named parameters, `sqlite_core_param_clean_prefix`, scalar reads, output parameters via `RETURNING` |
| `transaction.c` | `begin` / `commit` / `rollback`, nested-begin and commit-without-transaction failures |
| `json.c` | `sqlite_core_execute_reader_to_json` and `sqlite_core_insert_json` |

## Building with the project

They are built by default; disable with
`-DLIBSQLITE3_BUILD_EXAMPLES=OFF`.

```bash
cmake -S src/sqlite -B build
cmake --build build --config Release
ctest --test-dir build -C Release -R example   # Windows
ctest --test-dir build -R example             # Linux / macOS
```

Binaries and the copied shared library land next to
`$<TARGET_FILE_DIR:example_basic>` in the build tree.

## Building standalone

```bash
cc -I ../core basic.c -L <bin dir> -llibsqlite3 -Wl,-rpath,'$ORIGIN' -o basic
```

Every string returned by the library is released with
`sqlite_core_string_free` (SQLite allocator), and a reader callback
aborts iteration by returning non-zero — the same rules the tests follow.

> **Windows shared builds (DLL)** export exactly the 31 `sqlite_core_*`
> symbols listed in `core/sqlite.def`. The engine helpers
> (`sqlite3_column_*`, ...) are **not** exported, so consumers of the DLL
> work through the wrapper API (that is why `basic.c` prints rows as
> JSON). Link statically if you need direct engine calls.