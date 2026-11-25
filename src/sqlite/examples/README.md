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

## Language bindings (`bindings/`)

The same canonical flow (open → DDL → named parameters → reader →
scalar → JSON out/in → BLOB from file → `get_blob` → transactions →
error paths → close) is implemented for other languages; see
[`bindings/README.md`](../../bindings/README.md) for the rules and the
full table.

Covered so far — 15 languages verified end to end in containers
(library built, CTest suite green, canonical example green):

| Language | Folder | Status | Checks |
|----------|--------|--------|--------|
| C (this folder) | `src/sqlite/examples` | native | — |
| Python | `bindings/python` | ✅ verified, container | 19 |
| C# / .NET | `bindings/csharp` | ✅ verified, container | 19 |
| C++ | `bindings/cpp` | ✅ verified, container | 28 |
| Swift | `bindings/swift` | ✅ verified, container | 32 |
| Go | `bindings/go` | ✅ verified, container | 19 |
| Rust | `bindings/rust` | ✅ verified, container | 19 |
| Node.js | `bindings/nodejs` | ✅ verified, container | 18 |
| Java | `bindings/java` | ✅ verified, container | 20 |
| R | `bindings/r` | ✅ verified, container | 21 |
| Ruby | `bindings/ruby` | ✅ verified, container | 29 |
| Julia | `bindings/julia` | ✅ verified, container | 32 |
| Kotlin | `bindings/kotlin` | ✅ verified, container | 31 |
| Nim | `bindings/nim` | ✅ verified, container | 34 |
| Fortran | `bindings/fortran` | ✅ verified, container | 33 |
| PHP | `bindings/php` | ✅ verified, container | 19 |

### 🟡 Written, pending container verification

| Language | Folder | Status | Checks |
|----------|--------|--------|--------|
| — | — | — | — |

Also declared (34 symbols each) but outside the container matrix:

| Language | Folder | Status |
|----------|--------|--------|
| Visual FoxPro 9 | `bindings/foxpro` | 🟡 declarations + README only |
| Clipper | `bindings/clipper` | 🟡 declarations + README only |

Pending (no example yet): Dart/Flutter, D, Haskell, Perl, Zig,
LuaJIT, Tcl, MATLAB, Delphi/FPC, VB6, COBOL, PowerBuilder, RPG.

> **Windows shared builds (DLL)** export exactly the 31 `sqlite_core_*`
> symbols listed in `core/sqlite.def`. The engine helpers
> (`sqlite3_column_*`, ...) are **not** exported, so consumers of the DLL
> work through the wrapper API (that is why `basic.c` prints rows as
> JSON). Link statically if you need direct engine calls.