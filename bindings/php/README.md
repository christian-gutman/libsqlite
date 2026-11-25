# PHP (FFI)

**English** · [Español](README.es.md)

`libsqlite.php` declares the full frozen API (34 symbols) with PHP's
**compiled-in FFI extension** — no external packages, no compilation.
`example_full.php` is the canonical flow. Single requirement: `php-cli`
with the FFI extension loaded.

## Files

| File | What it is |
|------|------------|
| `libsqlite.php` | The FFI binding: scope, parameter builders, allocator-rule helpers |
| `example_full.php` | Canonical flow (22 checks) |
| `src/php_shim.c` | ~120 lines of C that close PHP FFI's missing callback mechanism |
| `libsqlite_shim.so` | The compiled shim (built by the container, git-ignored) |

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

## Building the shim

`example_full.php` needs the shim, because PHP FFI cannot create a C
function pointer (see *The callback gap* below):

```bash
LIB=/path/to/libsqlite3.so
LIBDIR=$(dirname "$LIB")
cc -shared -fPIC -O2 -I ../../src/sqlite/core src/php_shim.c \
   -L "$LIBDIR" -lsqlite3 -Wl,-rpath,"$LIBDIR" -o libsqlite_shim.so
```

## Build and run

```bash
php example_full.php ../../bin/x64-Linux/shared/Release/libsqlite3.so ./libsqlite_shim.so
```

## Containers

Two containers, both run from `docker/build.cmd` and **neither is part of
CI** (see `docker/readme.md`):

| Container | Base | Builds the shim with | Verifies |
|-----------|------|----------------------|----------|
| `php.Dockerfile` | `debian:bookworm-slim` + `php-cli`/`php8.2-ffi` | `cc` in the container | Builds the library, CTest, then the example against the `.so` |
| `php-windows.Dockerfile` | `mcr.microsoft.com/windows/servercore` | MSVC (`cl`) | Same flow against the `.dll` + import library |

```bat
docker\build.cmd php
```

The shim is compiled inside each container for the platform that will run
it, so there is no prebuilt binary to ship.

## The callback gap, and how the shim closes it

PHP FFI **cannot** turn a PHP closure into a C function pointer: the
extension has no callback mechanism at all (the complete method list is
cdef, load, scope, new, free, cast, type, typeof, arrayType, addr, sizeof,
alignof, memcpy, memcmp, memset, string, isNull — verified on PHP 8.2, and
there is no `FFI::callback`).

That leaves two of the 34 frozen symbols unreachable from pure FFI, and the
shim supplies both without re-implementing any behaviour — the same approach
the R shim (`bindings/r/src/libsqlite_shim.c`) and the Swift trampoline
take:

| Shim function | What it closes |
|---------------|----------------|
| `sqlite_php_reader_rows(db, sql, &rows, &columns, err, max)` | Drives `sqlite_core_execute_reader` through a C trampoline and returns the row/column counts |
| `sqlite_php_reader_json(db, sql, &json, &rows, err, max)` | Drives `sqlite_core_execute_reader` with the exported `sqlite_core_json_writer_callback` and a `sqlite_core_json_builder_t` context, wrapping the document with the same `"[\n"…"\n]"` protocol that `sqlite_core_execute_reader_to_json` uses |
| `sqlite_php_json_writer_callback()` | The address of the frozen callback, for callers that want to pass it themselves |

The example compares the shim's document with the one from
`execute_reader_to_json` **byte for byte**, so the wrapping is proven, not
assumed. Verified in the container: **22 checks, 0 failures**, CTest 7/7.

Callers that want only the FFI layer (no shim) still work for the other 32
symbols: the reader is reached through the exported
`sqlite_core_execute_reader_to_json`, which is the documented path on a
shared build.

## What the PHP layer adds

- `LibSqlite` builds the FFI scope once (`FFI::cdef($code, $lib)`) and owns
  every buffer whose native pointer C will read;
- `intParam` / `textParam` / `paramArray` build the named parameters as one
  contiguous `sqlite_core_param_t[]`; the text is copied into **non-owned**
  buffers pinned in a keep-list, because FFI refuses to assign an owned
  pointer into a struct's `char *` field;
- `staticString()` reads library-owned static data (versions,
  `error_message`, `param_clean_prefix`) — those arrive already as PHP
  strings and must **not** be freed;
- `takeString()` reads a SQLite-allocator string into a PHP string and
  releases the original with `sqlite_core_string_free` (never the PHP side);
- `scalarOutText()` pre-allocates the destination buffer `execute_scalar`
  needs for TEXT results;
- `readerCallbackSupport()` reports exactly what the FFI layer alone cannot
  do, so a caller is never left guessing.

## Notes learned while building this binding

Every one of these was hit, and is encoded in the files:

1. **FFI detection** — `defined('FFI_SCOPE')` is false even when the
   extension is loaded; use `extension_loaded('ffi')`.
2. **`die()` exits 0** — a container `CMD` that reports the example's exit
   code would therefore call a crash a success. The binding writes to
   STDERR and `exit(2)`.
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
10. **No callbacks at all** — see *The callback gap* above: this is why the
    shim exists, and why the reader is not driven from pure FFI.
11. **`uintptr_t` needs `<stdint.h>`** — `SIZE_MAX`/`uintptr_t` are not
    available through the headers MSVC pulls in transitively, so the shim
    includes `<stdint.h>` explicitly (the same fix `sqlite_core_builder.c`
    needed on Linux).

## Resumen (español)

Ver [`README.es.md`](README.es.md).