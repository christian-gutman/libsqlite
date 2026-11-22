# Node.js (koffi)

`libsqlite.js` declares the full frozen API (34 symbols) with `koffi`;
`example_full.js` is the canonical flow. Single dependency: `koffi`.

```bash
npm install koffi
node example_full.js ../../bin/x64-Windows/shared/Release/libsqlite3.dll
# (or the .so / .dylib path; argv[2] overrides the default search)
```

Notes learned while building this binding (all applied in the files):

- **Unions**: pass plain JS objects to struct-pointer parameters —
  `koffi` converts them (`{ name, type, Direction, value: { iVal } }`).
- **Out pointers**: declare them in the signature with
  `koffi.out(koffi.pointer("int"))` (not the `"int *"` string) and pass
  `[0]`; `koffi` writes through it.
- **`char **` out**: `koffi` dereferences it for you — the value arrives
  already as a JS string, so **do not** call `sqlite_core_string_free` on
  it (that was a heap corruption before this was understood).
- **Callbacks**: `koffi.register(fn, koffi.pointer(proto))` where the
  proto is `koffi.proto("name", "int", ["void *", "int", "int", "void *"])`;
  the proto must be declared before the function table that uses it.
- Verified against the x64 build: 18 checks, 0 failures.