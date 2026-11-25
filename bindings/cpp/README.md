# C++ (header-only)

**English** · [Español](README.es.md)

`libsqlite.hpp` declares the full frozen API (34 symbols) as `extern "C"`
plus a thin C++ layer on top; `example_full.cpp` is the canonical flow.
No library of its own is built: compile the header straight into your
binary and link against `libsqlite3`.

## Build and run

```bash
./run.sh ../../bin/x64-Linux/shared/Release/libsqlite3.so
```

```bat
cl /EHsc /I. example_full.cpp libsqlite3.lib
example_full.exe
```

The compile line is the same on every platform (see `run.sh`): include the
header, link `-l:libsqlite3.so` (or the import library `libsqlite3.lib` on
Windows).

## Container

```bat
docker\build.cmd cpp
```

`g++ -std=c++17 -Wall -Wextra` against the Linux shared object, with the
CTest suite running first.

## What the C++ layer adds

- `ParamSet` builds the named parameters and **owns their native text**
  (`sqlite_core_param_t` holds raw pointers, so the strings must outlive
  the call), then hands out a contiguous `sqlite_core_param_t[]`;
- `Connection` is an RAII handle: the destructor closes the database, the
  moved-from object clears its pointer, and the destructor of a failed
  open is a no-op;
- `take_string()` reads a `char*` allocated by the SQLite allocator and
  releases it with `sqlite_core_string_free` (never `free`/`delete`);
  `static_string()` copies the library's static data and frees nothing;
- `get_blob()` grows its buffer on `SQLITE_CORE_BUFFER_TOO_SMALL`, so the
  caller passes only the maximum size;
- `scalar_text()` pre-allocates the output buffer the C `out_scalar_result`
  needs for TEXT results (the library only writes when it fits);
- the `static_assert`s at the bottom of the header fail the build if the
  frozen `sqlite_core_param_t` layout (40 bytes on LP64, 28 on ILP32) or
  the 64-bit `sqlite3_int64` ever drifts.

## Notes learned while building this binding

- **Own the parameter buffers, and capture the pointer before pushing**:
  `sqlite_core_param_t` holds a raw `char*` for the name and another for the
  text value, so both must outlive the call. `ParamSet` copies them into its
  `keep_` list, but the value pointer must be captured *before* `add()`
  pushes the name copy — reading `keep_.back()` inside the fill lambda
  returns the name (this stored `"name"` where `"Ada"` belonged, and the
  JSON check is what exposed it).
- **Flag-style returns are `1`, not `SQLITE_OK`**: `close`, `begin`,
  `commit`, `rollback`, `set_busy_timeout` and `is_readonly` all follow
  `docs/behavior.md`: `>= 0` means success and `1` is the "flag" value
  (checking for `SQLITE_OK` fails).
- **`sqlite_core_error_message` mirrors the engine**: after a successful
  statement it returns "not an error" rather than an empty string, so the
  example reads it after a deliberate failure.
- **Empty result in `execute_scalar` is a success** with
  `SQLITE_CORE_TYPE_NULL` (`final_rc = SQLITE_OK` on `SQLITE_DONE`), so a
  text scalar may legitimately come back empty.
- **Variadic call**: `sqlite_core_string_format` is a plain C variadic
  function, so it is declared and called as such — no trampoline needed.
- **`char **` out**: `execute_reader_to_json` writes a heap pointer that
  only `sqlite_core_string_free` may release; `reader_json()` returns it
  as `std::string` after freeing the original.

