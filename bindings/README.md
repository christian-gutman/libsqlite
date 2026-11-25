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

## Platform coverage

**Windows and Linux are verified in CI on every push.**

The sources also target macOS and the build produces `libsqlite3.dylib`
(either `bin/arm64-Darwin/…` or `bin/x64-Darwin/…`, see the
[build options](../README.md#cmake-options) in the root README), but
**macOS is not CI-verified**. This repository has no access to
GitHub-hosted macOS runners:

| `runs-on` | Result | Why |
|---|---|---|
| `macos-14`, `macos-15` (arm64) | rejected instantly | *"The configuration `macos-14-arm64-us-default` is not supported"* |
| `macos-13` (x64, pinned) | valid, queues forever | the label is accepted but no runner is ever assigned |

None of this affects the code: the engine and the wrapper carry explicit
`__APPLE__` paths, the same CMake project builds on a Mac, and every
binding in this table links against that same `.dylib`. The limitation is
runner access, not the sources.

To verify Darwin locally, on a Mac:

```bash
cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Registering a **self-hosted Mac runner** is the only way to restore CI
coverage for macOS (tracked in [`docs/ROADMAP.md`](../docs/ROADMAP.md)).
The Python binding in [`python/`](python) shows the canonical example for
every platform.

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

Legend: ✅ verified (example compiles and runs green against the built
library — "container" means the same example also ran in its Linux
container) · 🟡 written, needs toolchain · ⬜ planned.

### ✅ Verified in container (14/14)

| Language | Binding | Example | Technique | Checks | Container base |
|----------|---------|---------|-----------|--------|----------------|
| Python | `python/binding.py` | `python/example_full.py` | ctypes | 19 | `python:3.12-bookworm` |
| C# / .NET | `csharp/LibSqlite.cs` | `csharp/ExampleFull.cs` | P/Invoke | 19 | `mcr.microsoft.com/dotnet/sdk:8.0` |
| C++ | `cpp/libsqlite.hpp` | `cpp/example_full.cpp` | direct C ABI | 28 | `gcc:14-bookworm` |
| Swift | `swift/LibSqlite.swift` | `swift/ExampleFull.swift` | direct C ABI + modulemap | 32 | `swift:6.0-jammy` |
| Go | `go/libsqlite.go` | `go/example_full.go` | cgo | 19 | `golang:1.22-bookworm` |
| Rust | `rust/libsqlite.rs` | `rust/example_full.rs` | `extern "C"` | 19 | `rust:1-bookworm` |
| Node.js | `nodejs/libsqlite.js` | `nodejs/example_full.js` | koffi | 18 | `node:24-bookworm` |
| Java | `java/LibSqlite.java` | `java/ExampleFull.java` | JNA | 20 | `eclipse-temurin:17-jammy` |
| PHP | `php/libsqlite.php` | `php/example_full.php` | FFI (native, no packages) | 19 | `debian:bookworm-slim` + php-cli/php8.2-ffi |
| R | `r/libsqlite.R` | `r/example_full.R` | shim + `R CMD SHLIB` | 21 | `r-base:latest` |
| Ruby | `ruby/libsqlite.rb` | `ruby/example_full.rb` | ffi gem | 29 | `ruby:3.3-bookworm` |
| Julia | `julia/libsqlite.jl` | `julia/example_full.jl` | `ccall` + `@cfunction` | 32 | `julia:1.11-bookworm` |
| Kotlin | `kotlin/libsqlite.kt` | `kotlin/ExampleFull.kt` | JNA | 31 | `eclipse-temurin:17` + kotlinc |
| Nim | `nim/libsqlite.nim` | `nim/example_full.nim` | `{.importc, dynlib.}` | 34 | `nimlang/nim:2.0.14` |
| Fortran | `fortran/libsqlite.f90` | `fortran/example_full.f90` | `iso_c_binding` | 33 | `ubuntu:24.04` + gfortran |

### 🟡 Declared, outside container matrix (2/2)

Both declare all 34 symbols (verified by `bindings/verify_symbols.py`) but
have no Linux container — they consume the Win32/x64 DLL from a legacy
16/32-bit Windows runtime.

| Language | Binding | Example | Status |
|----------|---------|---------|--------|
| Visual FoxPro 9 | `foxpro/` | `foxpro/example_full.prg` | 🟡 `DECLARE DLL` example + README |
| Clipper (Harbour / xHarbour / NAM) | `clipper/` | `clipper/example_full.prg` | 🟡 `hb_libLoad` / `_dllfn` example + README |

### ⬜ Pending — Modern (one at a time)

| Language | Binding | Example | Status |
|----------|---------|---------|--------|
| Dart / Flutter | `dart/libsqlite.dart` | `dart/example_full.dart` | ⬜ deferred by the user |
| D | `d/libsqlite.d` | `d/example_full.d` | ⬜ |
| Haskell | `haskell/LibSqlite.hs` | `haskell/ExampleFull.hs` | ⬜ |
| PHP | `php/libsqlite.php` | `php/example_full.php` | 🟡 written, needs container verification |
| Perl | `perl/libsqlite.pl` | `perl/example_full.pl` | ⬜ |
| Zig | `zig/libsqlite.zig` | `zig/example_full.zig` | ⬜ |
| LuaJIT | `lua/libsqlite.lua` | `lua/example_full.lua` | ⬜ |
| Tcl | `tcl/libsqlite.tcl` | `tcl/example_full.tcl` | ⬜ |
| MATLAB | `matlab/libsqlite.m` | `matlab/example_full.m` | ⬜ |

### ⬜ Pending — Legacy (verification method to be agreed)

Most of these cannot run in a Linux container.

| Language | Binding | Example | Status |
|----------|---------|---------|--------|
| Delphi / FPC | `delphi/libsqlite.pas` | `delphi/example_full.dpr` | ⬜ |
| VB6 | `vb6/LibSqlite.vb` | `vb6/ExampleFull.vb` | ⬜ |
| COBOL (GnuCOBOLFFI) | `cobol/libsqlite.cob` | `cobol/example_full.cob` | ⬜ |
| PowerBuilder | `powerbuilder/libsqlite.pbl` | `powerbuilder/example_full.pbl` | ⬜ |
| RPG (IBM i) | `rpg/libsqlite.rpgle` | `rpg/example_full.rpgle` | ⬜ |

## Legacy (Windows DLL consumers)

The frozen 34-symbol `.def` is what these platforms load; all of them run
against the Win32/x64 DLL the project already produces.

| Language | Folder | Status |
|----------|--------|--------|
| Visual FoxPro 9 | `foxpro/` | 🟡 `DECLARE DLL` example + README (FoxPro 2.6 is 16-bit and **cannot** load the DLL; the production path is the separate FLL deliverable) |
| Clipper (Harbour/xHarbour/NAM) | `clipper/` | 🟡 runtime `hb_libLoad`/`_dllfn` example + README |
| Delphi / FPC | `delphi/` | ⬜ |
| VB6 | `vb6/` | ⬜ |
| COBOL (GnuCOBOLFFI) | `cobol/` | ⬜ |
| PowerBuilder | `powerbuilder/` | ⬜ |
| RPG (IBM i) | `rpg/` | ⬜ |

Each folder contains a `README.md` with the exact build/run commands.

## Running the examples through containers

Each verified language ships a container that builds the library, runs the
CTest suite and then compiles and runs its example:

```bat
docker\build.cmd python   :: builds, runs and removes the image
docker\build.cmd csharp   :: publishes the example, .so next to the DLL
docker\build.cmd cpp      :: g++ -std=c++17 against the header-only binding
docker\build.cmd swift    :: swiftc -I include + module.modulemap
docker\build.cmd ruby     :: the ffi gem (Fiddle::Closure is broken in Ruby 3.x)
docker\build.cmd julia    :: ccall/@cfunction, no dependencies
docker\build.cmd.cmd    :: gfortran (iso_c_binding); the module is compiled first
docker\build.cmd go
docker\build.cmd rust     :: plain rustc + the platform #[link] name
docker\build.cmd nodejs   :: npm install koffi inside the container
docker\build.cmd java     :: also fetches the JNA jar from Maven Central
docker\build.cmd kotlin  :: Kotlin compiler + JNA jar, kotlinc + kotlin
docker\build.cmd r        :: compiles the R shim (R CMD SHLIB) + runs it

docker\build_all.cmd      :: the thirteen of them in sequence + pass/fail matrix
```

FoxPro and Clipper are deliberately **not** containerized: they consume
the Windows DLL from a 16/32-bit legacy runtime and cannot run in a Linux
container (see `docker/readme.md`).

The wrapper removes the image afterwards, so nothing accumulates
(`docker system df` shows the reclaim). The examples mutate files only
inside the container; the host repository is never written to.
