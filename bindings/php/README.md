# PHP (FFI)

**English** · [Español](README.es.md)

`libsqlite.php` declares the full frozen API (34 symbols) with PHP's
**compiled-in FFI extension** — no external packages, no compilation.
`example_full.php` is the canonical flow. Single requirement: `php-cli`
with the FFI extension loaded.

## Requirements

The FFI extension must be loaded. It is a compiled-in PHP extension:

```bash
php -m | grep -i ffi          # must print FFI
```

Detect it in code with `extension_loaded('ffi')` /
`class_exists('FFI')` — **not** `defined('FFI_SCOPE')`, which is false
even when FFI is loaded (verified on PHP 8.2).

> **Docker note:** the official `php:*` images are compiled from source
> *without* FFI (no `php -m` entry and no `/etc/php/*/mods-available` tree
> to `phpenmod` it). The container therefore uses `debian:bookworm-slim`
> with `php-cli` + `php8.2-ffi`. For a local install, Debian/Ubuntu:
> `apt-get install php-cli php8.2-ffi`.

## Build and run

```bash
php example_full.php ../../bin/x64-Linux/shared/Release/libsqlite3.so
# (omit the path to use the usual relative locations)
```

## Container

```bat
docker\build.cmd php
```

`debian:bookworm-slim` + `php-cli`/`php8.2-ffi`: builds the library, runs
the CTest suite, then runs the example against the Linux shared object.

## What the PHP layer adds

- `LibSqlite` builds the FFI scope once (`FFI::cdef($code, $lib)`) and
  owns every buffer whose native pointer C will read;
- `intParam` / `textParam` / `paramArray` build the named parameters as
  one contiguous `sqlite_core_param_t[]`; the text is copied into
  **non-owned** buffers pinned in a keep-list, because FFI refuses to
  assign an owned pointer into a struct's `char *` field;
- `staticString()` reads library-owned static data (versions,
  `error_message`, `param_clean_prefix`) — those arrive already as PHP
  strings and must **not** be freed;
- `takeString()` reads a SQLite-allocator string into a PHP string and
  releases the original with `sqlite_core_string_free` (never the PHP
  side);
- `scalarOutText()` pre-allocates the destination buffer
  `execute_scalar` needs for TEXT results.

## Notes learned while building this binding

Every one of these was hit, and is encoded in the files:

1. **FFI detection** — `defined('FFI_SCOPE')` is false even when the
   extension is loaded; use `extension_loaded('ffi')`.
2. **`die()` exits 0** — a container `CMD` that reports the example's
   exit code would therefore call a crash a success. The binding writes
   to STDERR and `exit(2)`.
3. **`FFI::load()` is for header files** — it takes one argument. To
   declare inline code and link a shared object: `FFI::cdef($code, $lib)`.
4. **C type syntax, not PHP closure syntax** —
   `FFI::type('int function(FFI\CData, ...): int')` raises a
   `ParserException`; a function pointer type is
   `int(*)(void*,int,int,void*)`.
5. **Structs need the instance scope** — the static `FFI::new()` resolves
   against the *preloaded* scope, which does not know this cdef, so
   `FFI::new('sqlite_core_param_t')` fails with "Undefined C type". Use
   `$lib->new(...)`.
6. **Owned pointers cannot be assigned into structs** — "Attempt to
   perform assign of owned C pointer". Create the buffers with
   `$lib->new('char[N]', false)` (non-owned) and pin them.
7. **`char *` returns become PHP strings** — FFI converts them on return,
   which destroys the pointer before it can be freed. The
   heap-returning functions (`sqlite_core_string_format`,
   `sqlite_core_string_concat`, `sqlite_core_string_utf8_to_ansi`,
   `sqlite_core_string_ansi_to_utf8`, `sqlite_core_execute_reader_to_json`)
   are declared `void *` so the allocator rule can still be honoured.
8. **`memcpy` string boundary** — asking for `strlen+1` bytes from a PHP
   string raises "Attempt to read over string boundary". Copy the length
   and terminate by hand.
9. **Only `new` and `type` have instance forms** — `addr`, `memcpy`,
   `sizeof`, `string`, `isNull` and `cast` fall through the FFI object's
   `__call` into a C function lookup and fail with "undefined C
   function". Those two go through `$lib->`, the rest stay static.
10. **No callbacks at all** — PHP FFI has no mechanism to turn a closure
    into a C function pointer (`FFI::callback` does not exist). This is
    the same limitation the Java, C#, Go, C++ and Kotlin bindings hit:
    `sqlite_core_execute_reader` cannot be driven from PHP, and
    `sqlite_core_json_writer_callback` — declared like every other frozen
    symbol — cannot be handed to it either. The reader is exercised
    through the exported `sqlite_core_execute_reader_to_json`, which is
    the documented path on a shared build. A real row callback requires a
    C trampoline, as the R and Swift bindings ship.

## Resumen (español)

Ver [`README.es.md`](README.es.md).

Verified in the container: **19 checks, 0 failures**, CTest 7/7.