# Bindings

Every language in this folder wraps the **frozen public API**: the 34
symbols exported by `libsqlite3` (`src/sqlite/core/sqlite.def`). Because
the export list is frozen and enforced by the `exports` CTest, a binding
written once keeps working for the whole 1.x line.

## Common rules (all languages)

| Topic | Rule |
|-------|------|
| Handles | `sqlite3*` / `sqlite3_stmt*` are opaque pointers — treat them as `void*`/`IntPtr`/`uintptr`. |
| Library file | `libsqlite3.dll` (Windows), `libsqlite3.so` (Linux), `libsqlite3.dylib` (macOS). Shared builds export **only** the 34 `sqlite_core_*` symbols — engine calls are not available in a DLL; link statically for those. |
| Returned strings | Any `char*` returned by the library is allocated by the **SQLite allocator** and must be released with `sqlite_core_string_free` — never the host allocator. |
| Callbacks | `sqlite_core_row_callback_fn(stmt, columns, hasrows, ctx)`: one call per row; `hasrows == 0` marks an empty result set; return non-zero to abort iteration. |
| `sqlite_core_param_t` | Reproduce the struct exactly: `const char *name; int no_insensitive; int type; int Direction; union { double dVal; long long iVal; struct { char *ptr; int len; int max_len; } blob_or_text; } value;` (alignment of the union matters). |
| Text | The API speaks UTF-8 (`char*`). Convert from/to the host string type explicitly. |
| Errors | `>= 0` success (bytes/rows written for value functions, `1` for flag ones); `SQLITE_MISUSE`, `SQLITE_CANTOPEN`, negative SQLite codes for failures; `-2` `BUFFER_TOO_SMALL`, `-3` `BLOB_TOO_SMALL`. |

## The canonical example

Every language ships `example_full.*` implementing the **same** flow, so
behavior can be compared side by side:

1. `open_or_create` (check `rc == SQLITE_OK` and the handle)
2. `execute_command` — `CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)`
3. `execute_non_query` with **named parameters** (`:id`, `:name`) — expect `1`
4. `execute_reader` — callback counts rows; print them via
   `execute_reader_to_json` (the exported surface, works on the DLL)
5. `execute_scalar` — `COUNT(*)` → `1`
6. `execute_reader_to_json` — print the JSON array
7. `insert_json` — batch of two objects → `2`
8. binary fixture with `0x00` bytes → `insert_blob` (chunked) → `get_blob`
   round trip compared byte by byte; `last_insert_rowid > 0`
9. transaction: `begin` → insert → `rollback` → count unchanged; then the
   commit path
10. error paths: `open_readonly` of a missing file, multi-statement
    `execute_non_query`, `close`

Exit code = number of failed checks (0 on success).

## Coverage

Legend: ✅ verified locally (example compiles and runs green against the
built library) · 🟡 written, needs toolchain/container · ⬜ planned.

| Language | Binding | Example | Status |
|----------|---------|---------|--------|
| Python | `python/binding.py` | `python/example_full.py` | ✅ ctypes |
| C# / .NET | `csharp/LibSqlite.cs` | `csharp/ExampleFull.cs` | ✅ dotnet 6 |
| Go | `go/libsqlite.go` | `go/example_full.go` | 🟡 cgo (needs gcc / container) |
| Rust | `rust/libsqlite.rs` | `rust/example_full.rs` | ⬜ |
| Node.js | `nodejs/libsqlite.js` | `nodejs/example_full.js` | ⬜ |
| Dart | `dart/libsqlite.dart` | `dart/example_full.dart` | ⬜ |
| Java | `java/LibSqlite.java` | `java/ExampleFull.java` | ⬜ |
| R | `r/libsqlite.R` | `r/example_full.R` | ⬜ |
| C++ | `cpp/libsqlite.hpp` | `cpp/example_full.cpp` | ⬜ |
| Swift | `swift/LibSqlite.swift` | `swift/example_full.swift` | ⬜ |
| Kotlin | `kotlin/LibSqlite.kt` | `kotlin/ExampleFull.kt` | ⬜ |
| Delphi / FPC | `delphi/libsqlite.pas` | `delphi/example_full.dpr` | ⬜ |
| VB.NET | `vbnet/LibSqlite.vb` | `vbnet/ExampleFull.vb` | ⬜ |
| D | `d/libsqlite.d` | `d/example_full.d` | ⬜ |
| Nim | `nim/libsqlite.nim` | `nim/example_full.nim` | ⬜ |
| Zig | `zig/libsqlite.zig` | `zig/example_full.zig` | ⬜ |
| Ruby | `ruby/libsqlite.rb` | `ruby/example_full.rb` | ⬜ |
| PHP | `php/libsqlite.php` | `php/example_full.php` | ⬜ |
| LuaJIT | `lua/libsqlite.lua` | `lua/example_full.lua` | ⬜ |
| Perl | `perl/libsqlite.pl` | `perl/example_full.pl` | ⬜ |
| Julia | `julia/libsqlite.jl` | `julia/example_full.jl` | ⬜ |
| Fortran | `fortran/libsqlite.f90` | `fortran/example_full.f90` | ⬜ |
| Haskell | `haskell/LibSqlite.hs` | `haskell/ExampleFull.hs` | ⬜ |
| Tcl | `tcl/libsqlite.tcl` | `tcl/example_full.tcl` | ⬜ |
| MATLAB | `matlab/libsqlite.m` | `matlab/example_full.m` | ⬜ |

Each folder contains a `README.md` with the exact build/run commands.

## Running the examples through containers

Build the library on the host (or via `docker/Dockerfile`) and then, per
language:

```bash
bash docker/run-binding.sh python      # builds the image, runs the example, removes the image
```

The script mounts the repository read-only, so no host toolchain is
needed; images are removed after each run to keep disk usage flat
(`docker system df` shows the reclaim).