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
rustc -L native=../../bin/x64-Linux/shared/Release example_full.rs -o example
LD_LIBRARY_PATH=../../bin/x64-Linux/shared/Release ./example
```

Notes (the first two were found by the Linux container, not by the host
run):

- **Platform-specific `#[link]` name**: the binding uses
  `#[cfg_attr(windows, link(name = "libsqlite3"))]` /
  `#[cfg_attr(unix, link(name = "sqlite3"))]`. A single
  `link(name = "libsqlite3")` works on Windows (there the import library
  is `libsqlite3.lib`) but on Unix it becomes `-llibsqlite3`, and the
  linker then looks for `liblibsqlite3.so` — hence no `-l:libsqlite3.so`
  link-arg is needed anymore; `-L native=<dir>` is enough on every
  platform.
- **No inner attribute in the included file** (rustc 1.97+): `example_full.rs`
  `include!`s `libsqlite.rs`, and an inner attribute (`#![allow(dead_code)]`)
  inside an included file is rejected with "an inner attribute is not
  permitted in this context". The attribute lives in `example_full.rs` (the
  file that does the include) and `libsqlite.rs` documents why.
- Running from Visual Studio (cargo-free): the DLL must be next to the
  exe or on `PATH` (`PATH=...;.\..\bin\x64-Windows\shared\Release`).
- `static_str` reads strings owned by the library's static data
  (versions, error messages); `take_str` releases the
  SQLite-allocated ones with `sqlite_core_string_free`.
- `Param::int_param` / `text_param` / `scalar_out_int` build the common
  parameter kinds; `as_i64` reads an integer value out of the union.
- The row callback is a plain `extern "C"` function; its context is a
  pointer to a Rust value (used only during the call).
- Verified against the x64 build: 23 checks, 0 failures.