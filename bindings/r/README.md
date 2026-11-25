# R

**English** · [Español](README.es.md)

Two files make up the binding:

- `libsqlite.R` — the R wrappers + the `SYMBOLS` table with the frozen
  34-symbol surface (`src/sqlite/core/sqlite.def`).
- `src/libsqlite_shim.c` — a small shim compiled once with `R CMD SHLIB`.

## Why the shim (and not pure R)

R's `.C()` interface cannot reach this API, for four concrete reasons:

1. `sqlite_core_open*()` **returns** the `sqlite3*` handle, and `.C()`
   discards the C return value — the handle would be unreachable.
2. `sqlite_core_param_t` (used by `execute_non_query`, `execute_scalar`
   and `execute_reader`) is a struct whose union contains **native
   pointers** (`name`, `blob_or_text.ptr`); R can neither allocate nor
   read those.
3. `sqlite3_int64` (`last_insert_rowid`, the `rowid` of the blob writers)
   has no `.C()` representation. It travels as a double here (exact up to
   2^53, printed as an integer by the example).
4. `sqlite_core_execute_reader` needs a **C function pointer**; an R
   closure cannot be converted into one without compiled code, so the
   shim passes a trampoline that counts rows (and the column count of the
   last visited row) — the canonical example prints the rows through
   `execute_reader_to_json`, which is the documented path on a shared
   build.

The shim does not re-implement any behaviour: every entry point calls the
frozen API (it even exercises `sqlite_core_json_writer_callback` through
`reader_json_callback()`, and the JSON built that way is compared with the
one from `execute_reader_to_json`). Memory rules of the library apply
unchanged: every string returned by the API is read and released inside
the shim with `sqlite_core_string_free`, and the connection handle is an
external pointer whose finalizer closes the database if R forgets to.

## Build and run

```bash
./src/build.sh                       # compiles the shim (needs R + gcc)
Rscript example_full.R ../../bin/x64-Linux/shared/Release/libsqlite3.so
```

Omitting the library path uses the usual relative locations. The shim is
compiled against the headers in `src/sqlite/core`, so it always matches
the frozen signatures.

## Container

```bat
docker\build.cmd r
```

Builds the library, runs the full CTest suite, compiles the shim with
`R CMD SHLIB` and runs the example against
`bin/x64-Linux/shared/Release/libsqlite3.so`.

## Notes learned while building this binding

- **`.Call` by symbol**: the shim is not a registered package, so the R
  side calls it with `getNativeSymbolInfo(name, DLL = <dll>)` plus
  `.Call()`, not `PACKAGE =` (that only works for registered routines).
- **Scalar output buffer**: `execute_scalar` needs
  `value.blob_or_text.ptr` + `max_len` pre-filled for TEXT results (the
  library only writes when the buffer fits), so the shim pre-allocates a
  512-byte buffer for every scalar call.
- **Blob reads grow the buffer**: `get_blob` returns
  `BUFFER_TOO_SMALL` (-2) when the destination does not fit, so the shim
  doubles it (64 MiB cap) and reports the exact `copied` length.
- **Text parameters**: `len = -1` tells the library the pointer is
  NUL-terminated UTF-8; the shim copies the R string into its own buffer
  (SQLite binds with `SQLITE_TRANSIENT`, so the copy is enough) and frees
  it after the call.
- **No engine internals**: the shim never calls `sqlite3_*` directly. The
  JSON path builds its document with `sqlite_core_string_format` /
  `sqlite_core_string_concat` / `sqlite_core_string_free`, because the
  engine helpers are not part of the frozen export list (they resolve on
  Linux, where the `.so` is built with default visibility, but not against
  the Windows DLL).
- **Verified in the container** (there is no R on the build host): 21
  checks, 0 failures, CTest 7/7.

