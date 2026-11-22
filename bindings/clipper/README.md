# Clipper (Harbour / xHarbour / NAM)

The 34-symbol DLL surface is loaded at runtime; no compiled library is
needed.

- **Harbour / xHarbour** (`hbmk2`): `hb_libLoad("libsqlite3.dll")` +
  `hb_libDo(hLib, "sqlite_core_...")` — typed calls through a small
  wrapper per function (see the `FnLoad` helper in `example_full.prg`).
- **NAM** (Clipper 5.x classic): `_dllfn("libsqlite3.dll", name)` +
  `CALLDLL`-style calls.

`example_full.prg` implements the canonical flow with a shared
`FnLoad()` that selects the dialect at compile time.

Notes:

- Text is ANSI in the Clipper world; the API converts it transparently
  on every `Execute*` entry point (Windows only, per the contract).
- Strings returned by the library are heap-allocated by the SQLite
  allocator: release with `sqlite_core_string_free`, never with the
  Clipper memory manager.