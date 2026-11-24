# Fortran (iso_c_binding)

`libsqlite.f90` declares the full frozen API (34 symbols) as a Fortran
module with `iso_c_binding`, and `example_full.f90` is the canonical flow.
No compilation of the binding itself: link against `libsqlite3`.

```bash
./run.sh ../../bin/x64-Linux/shared/Release/libsqlite3.so
```

## Container

```bat
docker\build.cmd.cmd.cmd.cmd    :: see the note below
```

The container (`docker/bindings/fortran.Dockerfile`) uses `ubuntu:24.04`
because the official `gcc:<ver>` images cannot install `gfortran` (the
Debian `gfortran` alternatives package fails its post-installation script
there). It builds the library, runs the CTest suite, compiles the module
first (gfortran emits `libsqlite.mod`) and runs the example.

## What the binding does

- the 34 symbols are `interface` blocks with `bind(c, name="...")`, one per
  line of `src/sqlite/core/sqlite.def`, plus the `bind(C)` derived types for
  `sqlite_core_param_t` and its value union;
- `ParamSet`-style memory: the caller owns the native strings. `c_loc()` of
  a **local** buffer would dangle as soon as the subprogram returns, so the
  example declares every string the C side reads (SQL, names, text values,
  the scalar destination buffer) at program scope with the `target`
  attribute — this is the Fortran equivalent of the pins in the Ruby/Java
  bindings;
- the reader callback is a Fortran function passed with `c_funloc`
  (`count_rows_cb`), and the row/column counters are module variables;
- the union is represented by one `bind(C)` type `(addr, len, max_len)`:
  `dVal`/`iVal` share the first 8 bytes with `blob_or_text.ptr`, so
  `transfer()` writes the numeric values and `c_loc()` the text ones;
- returned `char*` follow the allocator rule: `ptr_to_string` copies static
  data (versions, `error_message`, `param_clean_prefix`) and
  `take_string` reads + releases the heap ones with
  `sqlite_core_string_free`;
- out parameters are `intent(out)` dummies (`int`, `int64`, the scalar
  struct, the `char **` of `execute_reader_to_json`), and blob reads
  compare bytes directly.

## Notes learned while building this binding

- **`c_loc` needs `TARGET` or `POINTER`** on the actual argument (not only
  on the dummy): every buffer that gets `c_loc` carries `target`.
- **Struct pointer arguments are scalar dummies**: the C API takes
  `sqlite_core_param_t *`, so the out-struct of `execute_scalar` is declared
  as a scalar `type(param_t), intent(out)` (an assumed-size `(*)` dummy
  would be a rank mismatch when passing a scalar).
- **`STOP` needs a constant code** in standard Fortran, so the example uses
  the GNU `exit(failures)` and is compiled without `-std=f2008`.
- **`c_int`/`c_char` are not inherited** by a user of the module unless the
  module's `use iso_c_binding` list is public: the example declares its own
  buffers with `kind=c_char`, so those names must stay visible.
- **Assumed-size character dummies** (`character(c_char) :: sql(*)`) are the
  clean way to pass a Fortran character buffer as a C `const char *`: the
  NUL terminator is part of the buffer, and no pointer arithmetic is needed.
- **`char **` out** (`execute_reader_to_json`) is read with `c_f_pointer`
  into a `type(c_ptr), pointer` variable (the `FPTR` argument of
  `c_f_pointer` must itself be a pointer) and then freed with
  `sqlite_core_string_free`.

## Resumen (español)

Binding Fortran puro con `iso_c_binding`: los 34 símbolos congelados como
bloques `interface` con `bind(c, name=...)` y tipos derivados `bind(C)` para
`sqlite_core_param_t` y su unión (un solo tipo `(addr, len, max_len)` donde
`dVal`/`iVal` comparten bytes con `blob_or_text.ptr`, escrito con
`transfer()`). La regla de oro: **el llamador es dueño de la memoria** —
`c_loc()` de un buffer local se cuelga al volver, así que todos los strings
que lee C (SQL, nombres, valores, buffer del scalar) se declaran en el
programa con `target`. El callback de filas se pasa con `c_funloc` y las
cadenas devueltas se liberan con `sqlite_core_string_free`. Verificado en el
contenedor (33 checks, 0 fallos).