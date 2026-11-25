# Julia (ccall)

**English** · [Español](README.es.md)

`libsqlite.jl` declares the full frozen API (34 symbols) with `ccall`, and
`example_full.jl` is the canonical flow. No packages and no compilation:
Julia's `ccall` talks to the built library directly.

```bash
julia example_full.jl ../../bin/x64-Linux/shared/Release/libsqlite3.so
```

## Container

```bat
docker\build.cmd julia
```

`julia:1.11` builds the library, runs the CTest suite and then the example
against the Linux shared object (the container adds only `build-essential`
and `cmake`, which the library itself needs).

## What the binding does

- `Libsqlite.load!(path)` `dlopen`s the library **with `RTLD_GLOBAL`** and
  the 34 symbols are then reached with the name-only `ccall` form
  (`ccall((:sqlite_core_version,), Ptr{Cchar}, ())`), which resolves in the
  current process;
- `ParamSet` builds `sqlite_core_param_t[]` in **raw memory**, because the
  struct's union holds native pointers (`name`, `blob_or_text.ptr`); the
  strings live in the set, and the offsets are computed from
  `Sys.WORD_SIZE ÷ 8`, so LP64 and ILP32 both lay out correctly;
- returned `char*` follow the allocator rule: `static_string` copies data
  the library owns (versions, `error_message`, `param_clean_prefix`) and
  `take_string` reads + releases the heap ones with
  `sqlite_core_string_free` (never the Julia GC);
- out parameters are `Ref{Cint}` / `Ref{Ptr{Cchar}}` / `Vector{UInt8}`
  buffers, and `get_blob` grows its destination on
  `SQLITE_CORE_BUFFER_TOO_SMALL`;
- `count_rows` is wrapped with `@cfunction` and passed to
  `sqlite_core_execute_reader`, so a plain Julia function is the row
  callback; `json_writer_callback` drives the exported
  `sqlite_core_json_writer_callback` through `Libdl.dlsym` and a builder
  context (mirror of `sqlite_core_json_builder_t`), and the example
  compares its document with the one of `reader_to_json`.

## Notes learned while building this binding

- **The library expression of `ccall` cannot be a local variable or a
  pointer**: Julia 1.11 rejects `(:sym, LIB_HANDLE[])` ("in dlopen, expected
  Union{String, Symbol, LazyLibrary}"). The documented solution is
  `Libdl.dlopen(path, RTLD_LAZY | RTLD_GLOBAL)` plus the name-only `ccall`
  form — without `RTLD_GLOBAL` the symbol is not visible and the call fails
  with `undefined symbol`.
- **`Sys.WORD_SIZE \ 8` is not integer division**: `\` is matrix left
  division and returns `64 \ 8 = 0.125`; Julia uses `÷`.
- **Storing negative ints**: `UInt32(-1)` raises `InexactError`, so the raw
  writes `reinterpret` from the signed types (`Int32`, `Int64`, `Float64`).
- **A closure cannot be used with `@cfunction`**: it needs a plain function,
  so the callback state lives in module-level `Ref`s.
- **Julia strings are NUL-terminated UTF-8**, so they are passed directly as
  `Cstring` arguments (no manual copies for SQL text).
- **`json_writer_callback` writes through a context struct** whose
  `string_ptr` is allocated by the SQLite allocator: it must be released
  with `sqlite_core_string_free` (mirroring the other bindings).

