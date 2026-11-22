# Go (cgo)

Complete frozen API in `libsqlite.go`; canonical example in
`example_full.go`. Because cgo cannot touch the fields of a C union
directly, the preamble provides `param_set_int` / `param_set_text` /
`param_get_int` helpers for `sqlite_core_param_t`.

```bash
# Linux (module layout: go.mod lives in this folder)
# note -l:libsqlite3.so (the file already carries the "lib" prefix)
CGO_LDFLAGS="-L../bin/x64-Linux/shared/Release -l:libsqlite3.so" \
  LD_LIBRARY_PATH=../bin/x64-Linux/shared/Release go run .

# Windows (MSYS2/MinGW gcc)
CGO_LDFLAGS="-L../bin/x64-Windows/shared/Release -llibsqlite3" \
  PATH="../bin/x64-Windows/shared/Release;$PATH" go run .
```

Notes:

- The row callback is passed as `C.sqlite_core_row_callback_fn(C.countRows)`
  with a `//export`ed Go function; the context is a `C.malloc`'ed int.
- Strings returned by the library are released with `take()`
  (`sqlite_core_string_free`), versions with `C.GoString` (they are static).
- For a pure-Go (no cgo) route, the same declarations can be generated with
  `bindgen`-style tooling; the cgo surface is the reference layout.