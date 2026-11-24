# Nim (importc + dynlib)

`libsqlite.nim` declares the full frozen API (34 symbols) with
`{.importc, dynlib: libname.}` — Nim resolves them in `libsqlite3.so` /
`libsqlite3.dll` / `libsqlite3.dylib` at run time, so there is **no wrapper
library and no build-time linking** — and `example_full.nim` is the
canonical flow.

```bash
./run.sh ../../bin/x64-Linux/shared/Release/libsqlite3.so
```

## Container

```bat
docker\build.cmd.cmd    :: compiles with nim c -d:release and runs it
```

`docker/bindings/nim.Dockerfile` uses `nimlang/nim:2.0.14`, builds the
library, runs the CTest suite and then compiles and runs the example with
`nim c -d:release` against the Linux shared object.

## What the binding does

- the 34 symbols are `proc ... {.importc, dynlib: libname.}` declarations
  with their exact signatures, one per line of `src/sqlite/core/sqlite.def`;
- `sqlite_core_param_t` uses Nim's `{.union.}` pragma for the C union —
  `dVal`, `iVal` and `blob_or_text` share the same 16 bytes, so the layout
  is exactly the C one (verified by the CTest `exports`/`api` suites and by
  the example itself);
- `ParamSet` owns the `cstring` copies (name and value) so the native
  pointers stay valid during the call, and hands out a contiguous
  `seq[Param]` as the parameter array;
- the reader callback is a top-level `{.cdecl.}` proc (a cdecl proc cannot
  capture the environment, so the counters live at module scope);
- returned `char*` follow the allocator rule: Nim strings are copied from
  the static data directly, and heap results are read and released with
  `sqlite_core_string_free` (`takeString`).

## Notes learned while building this binding

- **Reserved identifiers**: `ptr` and `out` are Nim keywords, so the
  `blob_or_text.ptr` field and the scalar out struct are written with
  backticks (`` `ptr` ``) / renamed (`outScalar`).
- **`int` is not `cint`**: Nim's `int` is the platform int (64-bit on
  64-bit Linux) while `cint` is always 32-bit, so every `var rc = 0` that is
  passed as `ptr cint` must be declared `var rc: cint`.
- **`{.union.}` is the tool for C unions**: Nim objects are laid out by
  field, so the `ParamValue` union (16 bytes on LP64) is declared with the
  pragma — without it the struct would be 24 bytes and the union writes
  would land in the wrong place.
- **A `{.cdecl.}` callback cannot be a closure**: the reader proc is
  declared at module scope, like the Fortran and Julia bindings.
- **`char **` out** (`execute_reader_to_json`) is a `ptr cstring`, and the
  heap string is read + freed with `sqlite_core_string_free`.
- **Nim strings compare by content AND length**: a 512-byte destination
  buffer is never `== "Ada"` — the comparison uses the length the library
  reports in `blob_or_text.len`. The same applies to the JSON builder: the
  row objects do not carry the trailing `\n` that `execute_reader_to_json`
  adds before `\n]`, so the inner part is `doc[2 ..< ^2]`.
- **`cstring` vs `string`**: Nim `cstring` values are already NUL-terminated
  UTF-8, so SQL text is passed directly; the copies that must outlive the
  call are owned by `ParamSet`.
- **Nim's `seq[Param]` memory is contiguous**, so `addr ps.items[0]` is the
  `sqlite_core_param_t *` the API expects.

## Resumen (español)

Binding Nim sin librería envolvente ni enlazado en tiempo de compilación:
los 34 símbolos congelados se declaran con `{.importc, dynlib: libname.}`
y se resuelven en tiempo de ejecución. La unión de `sqlite_core_param_t` se
modela con el pragma `{.union.}` (los 16 bytes de LP64 exactos),
`ParamSet` es dueño de las copias `cstring`, el callback de filas es un
`proc` de alto nivel `{.cdecl.}` (no captura entorno) y las cadenas
devueltas se liberan con `sqlite_core_string_free`. Verificado en el
contenedor `nimlang/nim:2.0.14`.