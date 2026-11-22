# Rust

`libsqlite.rs` declares the full frozen API (34 symbols) as `extern "C"`
plus `#[repr(C)]` `Param` with an explicit `union` for the value; the
example is `example_full.rs` (`include!`s the binding, so a single
`rustc` invocation is enough — no cargo project needed).

```bash
# Windows (DLL + import library)
rustc -L native=../../bin/x64-Windows/shared/Release example_full.rs -o example.exe
# libsqlite3.dll next to the exe or on PATH

# Linux
rustc -C link-arg=-l:libsqlite3.so \
      -L native=../../bin/x64-Linux/shared/Release example_full.rs -o example
LD_LIBRARY_PATH=../../bin/x64-Linux/shared/Release ./example
```

Notes:

- `-l:libsqlite3.so` on Unix: the file already carries the `lib` prefix,
  so `-llibsqlite3` would look for `liblibsqlite3.so`.
- `static_str` reads strings owned by the library's static data
  (versions, error messages); `take_str` releases the
  SQLite-allocated ones with `sqlite_core_string_free`.
- `Param::int_param` / `text_param` / `scalar_out_int` build the common
  parameter kinds; `as_i64` reads an integer value out of the union.
- The row callback is a plain `extern "C"` function; its context is a
  pointer to a Rust value (used only during the call).
- Verified against the x64 build: 19 checks, 0 failures.