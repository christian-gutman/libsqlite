# PHP (Zend extension)

**English** · [Español](README.es.md)

`bindings/php` wraps the frozen 34-symbol API through **PHP FFI**. This
folder does the same work as a **native Zend extension**: same library, same
canonical example, no external PHP packages — and one capability FFI cannot
reach at all.

## Why this exists

PHP FFI cannot turn a PHP closure into a C function pointer. That single
missing mechanism leaves two frozen symbols out of reach for
`bindings/php`, which closes the gap with a ~120-line C shim
(`bindings/php/src/php_shim.c`). An extension builds the pointer natively,
so here `sqlite_core_execute_reader` takes a real PHP `callable` and
`sqlite_core_json_writer_callback` is its own address, no trampoline in
between.

Two ways to use the library from PHP, and both stay supported: FFI for a
no-toolchain start, this extension for the full frozen surface.

## Building

The extension links against the `libsqlite3` this repository builds
(`SQLITE_CORE_API` is empty on non-Windows, so the shared object exports the
34 symbols directly — verified with `nm -D` in the container).

```bash
# library first
cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# then the extension
cd bindings/phpext
phpize
./configure --with-libsqlite3-src=../../src/sqlite/core \
            --with-libsqlite3-dir=../../bin/x64-Linux/shared/Release
make
```

Two --with options on purpose: this repository keeps the headers
(`src/sqlite/core`) apart from the built artifact
(`bin/<arch>-<os>/{shared,static}/<config>`), and assuming one place had
both is what produced the original "sqlite_core.h: No such file or
directory".

### Enabling it

Like any other extension, through an `.ini` — not a `-d` flag, so the
example runs the way a user's `php` runs:

```bash
CONF=$(php -i | grep -m1 'Scan this dir for additional .ini files' | sed 's/.*=> //' | tr -d ' ')
echo "extension=/path/to/bindings/phpext/.libs/libsqlite.so" > "$CONF/30-libsqlite.ini"
php -m | grep libsqlite
```

### Windows

```bat
buildconf.bat
configure --enable-libsqlite --with-libsqlite3-src=..\..\src\sqlite\core
             --with-libsqlite3-dir=..\..\bin\x64-Windows\shared\Release
nmake
```

## The container

`docker/build.cmd phpext` builds the library, runs the CTest suite, compiles
the extension with phpize against it, writes the `.ini`, and runs
`example_full.php` — **40 checks, 0 failures**. Like every other binding
container it is **not part of CI** (`docker/readme.md`).

## Mapping (C → PHP)

One-to-one against `sqlite.def`; `python bindings/verify_symbols.py` reports
`phpext 2 file(s) OK` (34/34).

| C symbol | PHP |
|---|---|
| `sqlite_core_open_or_create` | `Connection::openOrCreate(string $path): Connection` |
| `sqlite_core_open` | `Connection::open(string $path): Connection` |
| `sqlite_core_open_readonly` | `Connection::openReadonly(string $path): Connection` |
| `sqlite_core_open_uri_readonly` | `Connection::openUriReadonly(string $path): Connection` |
| `sqlite_core_open_ex` | `Connection::openEx(string $path, int $flags): Connection` |
| `sqlite_core_close` | `Connection::close(): int` |
| `sqlite_core_is_readonly` | `Connection::isReadonly(): int` |
| `sqlite_core_last_insert_rowid` | `Connection::lastInsertRowid(): int` |
| `sqlite_core_set_busy_timeout` | `Connection::setBusyTimeout(int $ms): int` |
| `sqlite_core_error_code` | `Connection::errorCode(): int` |
| `sqlite_core_error_code_extended` | `Connection::errorCodeExtended(): int` |
| `sqlite_core_error_message` | `Connection::errorMessage(): string` |
| `sqlite_core_execute_command` | `Connection::command(string $sql): int` |
| `sqlite_core_execute_non_query` | `Connection::nonQuery(string $sql, array $params = []): int` |
| `sqlite_core_execute_transaction` | `Connection::executeTransaction(string $sql, int $txexpected = 0): int` |
| `sqlite_core_execute_scalar` | `Connection::scalar(string $sql, array $params = []): array` |
| `sqlite_core_execute_reader` | `Connection::reader(callable $cb, string $sql, array $params = []): int` |
| `sqlite_core_execute_reader_to_json` | `Connection::readerToJson(string $sql, array $params = []): array` |
| `sqlite_core_insert_json` | `Connection::insertJson(string $table, string $json): int` |
| `sqlite_core_get_blob` | `Connection::getBlob(string $sql, int $maxLen): array` |
| `sqlite_core_insert_blob` | `Connection::insertBlob(string $table, string $column, int $rowid, string $file, int $chunkSize = 0): array` |
| `sqlite_core_update_blob` | `Connection::updateBlob(...): array` |
| `sqlite_core_begin_transaction` | `Connection::begin(): int` |
| `sqlite_core_commit_transaction` | `Connection::commit(): int` |
| `sqlite_core_rollback_transaction` | `Connection::rollback(): int` |
| `sqlite_core_param_clean_prefix` | `libsqlite_param_clean_prefix(string $name): string` |
| `sqlite_core_version` | `libsqlite_version(): string` |
| `sqlite_core_sqlite_version` | `libsqlite_sqlite_version(): string` |
| `sqlite_core_string_concat` | `libsqlite_string_concat(string $a, string $b): ?string` |
| `sqlite_core_string_free` | internal — the extension copies every returned `char*` into a `zend_string`, so PHP never owns a library pointer |
| `sqlite_core_string_utf8_to_ansi` | `libsqlite_string_utf8_to_ansi(string $s): ?string` |
| `sqlite_core_string_ansi_to_utf8` | `libsqlite_string_ansi_to_utf8(string $s): ?string` |
| `sqlite_core_string_format` | `libsqlite_string_format(string $format, int\|float\|string ...$args): ?string` — **max 3 arguments** (see below) |
| `sqlite_core_json_writer_callback` | `libsqlite_json_writer_callback(): int` — the callback's own address |

`LIBSQLITE_TYPE_NULL` (0), `LIBSQLITE_TYPE_INT` (2), `LIBSQLITE_TYPE_DOUBLE`
(4), `LIBSQLITE_TYPE_TEXT` (6) and `LIBSQLITE_TYPE_BLOB` (8) are registered
as global constants, not redeclared by the example.

### Return shapes

A method returns **the frozen return code**, never a PHP exception, for
anything the library itself reports — including the cases that look like
failures: `SQLITE_MISUSE` (21) is a positive number, and a guard rejection
returns it with `error_code() == SQLITE_OK` (see
`bindings/CONTRACT.md` and the Doxygen in `src/sqlite/core/sqlite_core_error.h`).
Exceptions are reserved for programming errors: a non-callable `reader`
callback, or a call on a closed connection.

Three of them return more than one number, as an array:

| Method | Keys |
|---|---|
| `scalar()` | `rc`, `type`, `value` |
| `readerToJson()` | `rc`, `rows`, `json` |
| `getBlob()` | `rc`, `copied`, `data` |
| `insertBlob()` / `updateBlob()` | `rc`, `written` |

`scalar()` pre-allocates a 511-byte destination for TEXT/BLOB results,
because without one `execute_scalar` reports only the type.

## Notes learned while building this extension

1. **The C symbol name is not the one you write.** `PHP_METHOD(a_b, c)`
   lower-cases every token except the last, so `LibSqlite_Connection` becomes
   `zim_libsqlite_Connection_rollback` while `ZEND_ME(a_b, c)` expands to
   `zim_` + the class name verbatim. The two spellings must agree or the
   class entry references symbols that do not exist. `libsqlite_Connection`
   satisfies both.
2. **`config.m4` needs `PHP_SUBST`, not just a variable.** Assigning
   `-L<dir> -l:libsqlite3.so -Wl,-rpath,<dir>` to `LIBSQLITE_SHARED_LIBADD`
   (or through `PHP_EVAL_LIBLINE` / `PHP_ADD_LIBRARY_WITH_PATH`) leaves the
   generated Makefile without it. The link rule uses `$(LIBSQLITE_SHARED_LIBADD)`,
   so the `.so` linked with **every** `sqlite_core_*` symbol unresolved — the
   linker allows that in a shared module — and `dlopen` failed at load time
   with `Could not load dynamic library ... undefined symbol:
   sqlite_core_json_writer_callback` while `readelf -d` showed a DT_NEEDED
   holding only `libc.so.6`. The fix is `PHP_SUBST(LIBSQLITE_SHARED_LIBADD)`
   after the assignment, and `-lsqlite3` (not `-llibsqlite3`, which would
   look for `liblibsqlite3.so`).
3. **`Z_PARAM_VARIADIC` takes three arguments** in PHP 8.2:
   `Z_PARAM_VARIADIC(pass_by_ref_flag, dest, dest_num)`. The two-argument
   form used by older extensions does not compile here.
4. **`zend_fcall_info_init` returns a `zend_string**` name**, not a
   `char**`: `(callable, check_flags, fci, fcc, &callable_name, &error)`
   where `error` is a plain `char*` and must be `efree`d.
5. **`ZEND_ARG_CALLABLE` does not exist** in this PHP; use
   `ZEND_ARG_TYPE_INFO(0, callback, IS_CALLABLE, 0)`.
6. **`zend_bool_t` was removed** in PHP 8 — use `bool`.
7. **The `.ini` matters.** Loading the extension with
   `-d extension=libsqlite.so` proves the `.so` exists; writing
   `30-libsqlite.ini` into the conf.d that `php -i` reports proves the
   *engine* loads it, which is what a real installation does and what
   surfaces a missing DT_NEEDED.
8. **`txexpected` is compared against `autocommit`,** so `1` means *there
   must be no transaction* and `0` means *there must be one* — the opposite
   of what the name suggests. `execute_transaction` returns `-1` when the
   expectation does not hold, `1` when the statement runs, `0` when the
   statement itself fails. The example asserts all three.
9. **Registered constants are global.** The `LIBSQLITE_TYPE_*` constants this
   extension registers must not be re-declared by the example ("Constant
   already defined"), and `use const NAME` on a non-compound name is a no-op
   that PHP warns about.
10. **The reader callback abort is visible.** `person` holds three rows when
    the abort check runs, so the callback is asked exactly once and
    `reader()` reports the rows actually processed — which is how the
    contract's "a non-zero callback return aborts" is checked rather than
    merely claimed.

## Known limitation

`libsqlite_string_format` accepts **at most three arguments**. PHP cannot
build a C `va_list` at run time, so the wrapper dispatches on each
argument's PHP type and forwards the literal that type expects — the only
way to call a variadic without reimplementing it. Every combination of up to
three `int`/`float`/`string` arguments is handled; a fourth, or any other
type, raises. This is the single symbol of the 34 that cannot be forwarded
verbatim from PHP, and it is stated here rather than silently dropped, per
`bindings/CONTRACT.md` §3.