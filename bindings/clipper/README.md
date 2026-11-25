# Clipper (Harbour / xHarbour / NAM)

The 34-symbol DLL surface is loaded at runtime; no compiled library is
needed.

- **Harbour / xHarbour** (`hbmk2`): `hb_libLoad("libsqlite3.dll")` +
  `hb_libDo(hLib, "sqlite_core_...")` — typed calls through a small
  wrapper per function (see the `FnLoad` helper in `example_full.prg`).
- **NAM** (Clipper 5.x classic): `_dllfn("libsqlite3.dll", name)` +
  `CALLDLL`-style calls.

`example_full.prg` holds the shared machinery — `FnLoad()` selects the
dialect at compile time, plus `Check`, `Fixture` and the `RowCount` callback
convention. **It does not implement the canonical flow**: it calls no
symbol at all. `libsqlite.prg` declares the frozen 34-symbol surface
(34/34 per `verify_symbols.py`), so the binding exists, but the example
that proves the API works end to end is still to be written. Recorded in
`bindings/CONTRACT.md` section 8 as a stub rather than a gap, because a gap
implies something was tried and skipped.

Notes:

- Text is ANSI in the Clipper world; the API converts it transparently
  on every `Execute*` entry point (Windows only, per the contract).
- Strings returned by the library are heap-allocated by the SQLite
  allocator: release with `sqlite_core_string_free`, never with the
  Clipper memory manager.