# Python (ctypes)

No compilation, no external packages. Full frozen API in `binding.py`.

```bash
# from bindings/python, against a locally built library
python example_full.py ../../bin/x32-Windows/shared/Release/libsqlite3.dll
python example_full.py ../../bin/x64-Linux/shared/Release/libsqlite3.so
python example_full.py ../../bin/arm64-Darwin/shared/Release/libsqlite3.dylib
```

Notes:

- Strings returned by the library are decoded as UTF-8 and released with
  `sqlite_core_string_free` (see `Lib.string`) — never with `free()`.
- `SqliteCoreParam` mirrors the C union; `text()`/`integer()` build the
  common parameter kinds.
- The row callback receives `hasrows == 0` once for empty result sets.