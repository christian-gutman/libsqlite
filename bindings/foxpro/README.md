# Visual FoxPro

`libsqlite3.dll` (Win32/x64) is loadable directly with `DECLARE ... IN`.

- **FoxPro 2.5/2.6 (DOS/Windows, 16-bit)**: cannot load a 32-bit DLL.
  It can only call 16-bit DOS libraries via `LOAD ... CALL`. The path for
  those versions would be a 16-bit bridge binary, which is not worth the
  effort: use **Visual FoxPro 9** instead.
- **Visual FoxPro 9 SP2**: `DECLARE ... IN "libsqlite3.dll"` covers the
  whole frozen surface. `example_full.prg` implements the canonical flow.

The production route for this ecosystem remains the **FoxPro FLL**
deliverable (separated from this repository): an FLL exposes the same
operations as FoxPro-native functions with full parameter marshalling,
while `DECLARE DLL` callers hand-build the `sqlite_core_param_t` blocks
by hand (see the remarks in `example_full.prg`).

```foxpro
DO (example_full)     && compiled with VFP9: do example_full.prg
```

Notes:

- Strings returned by the library are released with
  `sqlite_core_string_free` (declared as `STRING` return).
- Text reaching FoxPro is ANSI; the API converts it transparently on
  every `Execute*` entry point (Windows only, per the contract).