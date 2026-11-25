# libsqlite — progress

**English** · [Español](PROGRESS.es.md)

*Last updated: 2026-11-24 (commit `428063e`).*

---

## 1. Where we are

`libsqlite` is a stable, cross-platform C SQLite wrapper around the
frozen 34-symbol public API (`src/sqlite/core/sqlite.def`). Milestones 0–5
are closed: hygiene, cross-platform build (Win32 / Win32-static / x64 /
Linux / macOS, shared + static), .NET parity, the frozen API, the test
suite with CI, and bilingual documentation.

The binding project is **stage 13 of the plan**: 15 languages are
*verified end to end inside containers* (library built, CTest suite, and
the canonical example), 2 more (Visual FoxPro and Clipper) have their
full 34-symbol declarations only and are deliberately excluded from the
container matrix, and the remaining modern and legacy targets are
pending.

---

## 2. The two decisions that explain the numbers

1. **The public API is frozen at 34 symbols.** Nothing has been added to
   `libsqlite3.dll` / `.so` / `.dylib`: every binding declares exactly
   those 34 symbols and no binding extends the library. The file
   `sqlite.def` is the single source of truth.

2. **The check counters differ because each example exercises a
   different subset of the same 34 symbols.** The canonical example is a
   19-step flow; every language writes its own assertions over it, so
   Nim reports 34 checks, Fortran 33, Kotlin 31, R 21… but they all
   *declare and use* the same 34 functions, which is what
   `bindings/verify_symbols.py` enforces (and what caught Julia's three
   missing symbols before they shipped).

---

## 3. Bindings verified inside containers

Every row below means: `docker/build.cmd <lang>` builds the Linux shared
library, runs the CTest suite (7/7) and then compiles and runs the
canonical example. The image is removed afterwards, keeping disk usage
flat.

| # | Language | Binding + example | Technique | Checks | Container base |
|---|----------|-------------------|-----------|--------|----------------|
| 1 | Python | `python/binding.py` + `example_full.py` | `ctypes` | 19 | `python:3.12-bookworm` |
| 2 | C# | `csharp/LibSqlite.cs` + `ExampleFull.cs` | P/Invoke | 19 | `mcr.microsoft.com/dotnet/sdk:8.0` |
| 3 | C++ | `cpp/libsqlite.hpp` + `example_full.cpp` | direct C ABI | 28 | `gcc:14-bookworm` |
| 4 | Swift | `swift/LibSqlite.swift` + `ExampleFull.swift` | direct C ABI | 32 | `swift:6.0-jammy` |
| 5 | Ruby | `ruby/libsqlite.rb` + `example_full.rb` | `ffi` (Fiddle::Closure is broken on Ruby 3.x) | 29 | `ruby:3.3-bookworm` |
| 6 | Julia | `julia/libsqlite.jl` + `example_full.jl` | `ccall` + `@cfunction`, `RTLD_GLOBAL` | 32 | `julia:1.11-bookworm` |
| 7 | Kotlin | `kotlin/libsqlite.kt` + `ExampleFull.kt` | JNA (`jna.encoding=UTF-8`) | 31 | `eclipse-temurin:17-jammy` + kotlinc |
| 8 | Nim | `nim/libsqlite.nim` + `example_full.nim` | `{.importc, dynlib.}` + `{.union.}` | 34 | `nimlang/nim:2.0.14` |
| 9 | Fortran | `fortran/libsqlite.f90` + `example_full.f90` | `iso_c_binding`, `c_loc` needs `target` | 33 | `ubuntu:24.04` + gfortran |
| 10 | Go | `go/example_full.go` (+ `go.mod`) | cgo | 19 | `golang:1.22-bookworm` |
| 11 | Rust | `rust/libsqlite.rs` + `example_full.rs` | `extern "C"` + `#[no_mangle]` callback | 19 | `rust:1-bookworm` |
| 12 | Node.js | `nodejs/libsqlite.js` + `example_full.js` | koffi | 18 | `node:24-bookworm` |
| 13 | Java | `java/LibSqlite.java` + `ExampleFull.java` | JNA | 20 | `eclipse-temurin:17-jammy` |
| 14 | R | `r/libsqlite.R` + `example_full.R` | `.Call` via `dyn.load` | 21 | `r-base:latest` |
| 15 | PHP | `php/libsqlite.php` + `example_full.php` | FFI (no closure → C callback: the reader goes through `execute_reader_to_json`) | 19 | `debian:bookworm-slim` + `php-cli`/`php8.2-ffi` |

*Verified in the last full run (`docker/build_all.cmd`): all 14 PASSED.*

## 4. Declared but not in the matrix

- **Visual FoxPro (VFP 9)** — `bindings/foxpro`
- **Clipper (Harbour / xHarbour)** — `bindings/clipper`

Both declare all 34 symbols (verified by `verify_symbols.py`) but have
**no container**: they are excluded from the matrix at the user's
request.

## 5. Pending

| Stage | Targets | Notes |
|---|---|---|
| Modern | **D**, Haskell, Perl | one at a time |
| Modern (last) | Dart / Flutter | deferred by the user |
| Legacy | Delphi / FPC, VB6, COBOL, PowerBuilder, RPG | the *verification method* must be agreed (most of these cannot run in a Linux container) |

Housekeeping:

- force-move tag `v1.0.0` to HEAD, push and push tags (established
  pattern at the end of each batch).
- GitHub Release page for `v1.0.0`: the API answered 401 (no PAT); it
  needs either the user in the UI or a token.

## 6. How to verify

```bat
docker\build.cmd nim      :: build -> run -> remove the image
docker\build_all.cmd       :: the 14-language matrix + PASSED/FAILED print
python bindings\verify_symbols.py   :: 34 symbols vs sqlite.def
```

### Answers to the recurring questions

- **Did you add symbols to the original DLL / touch the component?** No.
- **Why do some bindings show 19 checks and others twenty- or
  thirty-something?** Different subsets of the same 34 symbols.
- **Why does the PRAGMA set-up not change?** It is documented, not
  patched: `docs/pragmas.md` lists the five PRAGMAs applied at open,
  their measured values and the compile-time defaults. See the Spanish
  version in `docs/pragmas.es.md`.