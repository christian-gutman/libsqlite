# Ruby (ffi gem)

`libsqlite.rb` declares the full frozen API (34 symbols) with the `ffi` gem
and `example_full.rb` is the canonical flow. One dependency:

```bash
gem install ffi
ruby example_full.rb ../../bin/x64-Linux/shared/Release/libsqlite3.so
```

## Container

```bat
docker\build.cmd ruby
```

`ruby:3.3` builds the library, runs the CTest suite, installs `ffi` and then
runs the example against the Linux shared object.

## Why ffi and not Fiddle (the standard library)

`Fiddle::Closure` is the standard-library way to turn a Ruby block into a C
function pointer, and it is **broken on Ruby 3.x**: calling the resulting
pointer raises `undefined method 'call' for an instance of
Fiddle::Closure` (the trampoline no longer dispatches to the block). The
binding documents the attempt; `ffi` has first-class callbacks, so the
reader callback is a plain `Proc` passed to the attached function and ffi
converts it to the C signature (`callback :row_callback, [...]` +
`execute_reader(..., callback_proc, ctx, ...)`).

## What the binding does

- `Libsqlite.load(path)` `dlopen`s the library (position-independent: dynamic
  loading is done through `ffi_lib`) and binds every symbol by name with its
  exact signature, one entry per line of `src/sqlite/core/sqlite.def`;
- `ParamSet` builds `sqlite_core_param_t[]` in **raw memory**, because the
  struct's union holds native pointers (`name`, `blob_or_text.ptr`): the
  Ruby strings are copied into `FFI::MemoryPointer`s owned by the set, and
  `#pointer` hands out a contiguous array for the call. Offsets are derived
  from `FFI::Platform::ADDRESS_SIZE`, so LP64 and ILP32 both lay out
  correctly;
- returned `char*` follow the allocator rule: `:string` results are copied
  by ffi (`version`, `error_message`, `param_clean_prefix`), and
  `take_string` reads + releases the heap ones with
  `sqlite_core_string_free` (never the Ruby GC);
- out parameters are `FFI::MemoryPointer`s: `int_out`, `long_out`,
  `pointer_out` (for the `char **` of `execute_reader_to_json`);
- blob reads grow the destination buffer on `SQLITE_CORE_BUFFER_TOO_SMALL`;
- `Libsqlite.address("sqlite_core_json_writer_callback")` exposes the raw
  address of a frozen symbol so it can be passed as the reader callback, and
  the example compares the JSON built that way with the one from
  `execute_reader_to_json`.

## Notes learned while building this binding

- **`Fiddle::Closure` is unusable on Ruby 3.x** (see above): this is why the
  binding depends on `ffi`.
- **Union layout**: `sqlite_core_param_t` is 40 bytes on LP64 (24 + 16) and
  28 on ILP32 (16 + 12); the value offset is `PTR == 8 ? 24 : 16`, and the
  union's `blob_or_text` is `ptr`, `len` (`-1` = NUL-terminated UTF-8) and
  `max_len` (0 when no output is expected).
- **The scalar out struct doubles as the output**: for TEXT results the
  destination buffer has to be pre-filled (`blob_or_text.ptr` + `max_len`),
  otherwise the library only reports the type.
- **`char **` out**: `execute_reader_to_json` writes a heap pointer that only
  `sqlite_core_string_free` may release; `take_string` wraps exactly that.
- **Reading a C pointer back**: `FFI::MemoryPointer#read_string(len)` /
  `#get_pointer` give the bytes and addresses; `pointer.null?` is the NULL
  test for pointers returned by attached functions.

## Resumen (español)

Binding Ruby con la gema `ffi` (Fiddle de la stdlib queda descartado porque
`Fiddle::Closure` está roto en Ruby 3.x): los 34 símbolos congelados
enlazados uno a uno con su firma exacta, `ParamSet` que construye
`sqlite_core_param_t[]` en memoria cruda con desplazamientos válidos en LP64
e ILP32, `take_string` siguiendo la regla del alocador, buffers de salida
para `int*`, `long long` y `char**`, callback de filas con un `Proc` (ffi lo
convierte al firma C), lectura de BLOB que crece el buffer, y el documento
JSON también construido con `sqlite_core_json_writer_callback`. Verificado en
el contenedor `ruby:3.3`.