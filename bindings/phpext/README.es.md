# PHP (extensión Zend)

[English](README.md) · **Español**

`bindings/php` envuelve la API congelada de 34 símbolos usando **PHP FFI**.
Esta carpeta hace el mismo trabajo como **extensión Zend nativa**: la misma
librería, el mismo ejemplo canónico, sin paquetes PHP externos — y con una
capacidad que FFI no alcanza en absoluto.

## Por qué existe

PHP FFI no puede convertir un closure de PHP en un puntero a función C. Ese
mecanismo ausente deja dos símbolos congelados fuera del alcance de
`bindings/php`, que cierra el hueco con un shim en C de ~120 líneas
(`bindings/php/src/php_shim.c`). Una extensión construye el puntero de forma
nativa, así que aquí `sqlite_core_execute_reader` recibe un `callable` de
PHP real y `sqlite_core_json_writer_callback` es su propia dirección, sin
trampolín de por medio.

Dos formas de usar la librería desde PHP, ambas soportadas: FFI para empezar
sin toolchain, y esta extensión para la superficie congelada completa.

## Construcción

La extensión enlaza contra el `libsqlite3` que construye este repositorio
(`SQLITE_CORE_API` está vacío fuera de Windows, así que el shared object
exporta los 34 símbolos directamente — verificado con `nm -D` en el
contenedor).

```bash
# primero la librería
cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# luego la extensión
cd bindings/phpext
phpize
./configure --with-libsqlite3-src=../../src/sqlite/core \
            --with-libsqlite3-dir=../../bin/x64-Linux/shared/Release
make
```

Dos opciones `--with` a propósito: este repositorio separa las cabeceras
(`src/sqlite/core`) del artefacto construido
(`bin/<arquitectura>-<so>/{shared,static}/<config>`), y suponer que un solo
lugar tenía ambas es lo que produjo el "sqlite_core.h: No such file or
directory" original.

### Habilitarla

Como cualquier otra extensión, mediante un `.ini` — no un flag `-d`, para
que el ejemplo corra como corre el `php` de un usuario:

```bash
CONF=$(php -i | grep -m1 'Scan this dir for additional .ini files' | sed 's/.*=> //' | tr -d ' ')
echo "extension=/ruta/a/bindings/phpext/.libs/libsqlite.so" > "$CONF/30-libsqlite.ini"
php -m | grep libsqlite
```

### Windows

```bat
buildconf.bat
configure --enable-libsqlite --with-libsqlite3-src=..\..\src\sqlite\core
             --with-libsqlite3-dir=..\..\bin\x64-Windows\shared\Release
nmake
```

## El contenedor

`docker/build.cmd phpext` construye la librería, corre la suite CTest,
compila la extensión con phpize contra ella, escribe el `.ini` y corre
`example_full.php` — **40 checks, 0 fallos**. Como todos los demás
contenedores de binding, **no es parte del CI** (`docker/readme.md`).

## Mapeo (C → PHP)

Uno a uno contra `sqlite.def`; `python bindings/verify_symbols.py` informa
`phpext 2 file(s) OK` (34/34).

| Símbolo C | PHP |
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
| `sqlite_core_string_free` | interno — la extensión copia cada `char*` devuelto a un `zend_string`, así que PHP nunca es dueño de un puntero de la librería |
| `sqlite_core_string_utf8_to_ansi` | `libsqlite_string_utf8_to_ansi(string $s): ?string` |
| `sqlite_core_string_ansi_to_utf8` | `libsqlite_string_ansi_to_utf8(string $s): ?string` |
| `sqlite_core_string_format` | `libsqlite_string_format(string $format, int\|float\|string ...$args): ?string` — **máximo 3 argumentos** (ver más abajo) |
| `sqlite_core_json_writer_callback` | `libsqlite_json_writer_callback(): int` — la dirección propia del callback |

`LIBSQLITE_TYPE_NULL` (0), `LIBSQLITE_TYPE_INT` (2), `LIBSQLITE_TYPE_DOUBLE`
(4), `LIBSQLITE_TYPE_TEXT` (6) y `LIBSQLITE_TYPE_BLOB` (8) se registran como
constantes globales; el ejemplo no las redeclara.

### Formas de retorno

Un método devuelve **el código de retorno congelado**, nunca una excepción,
para todo lo que la librería informa — incluidos los casos que parecen
fallos: `SQLITE_MISUSE` (21) es un número positivo, y el rechazo de un
guardia lo devuelve con `error_code() == SQLITE_OK` (ver `bindings/CONTRACT.md`
y el Doxygen en `src/sqlite/core/sqlite_core_error.h`). Las excepciones se
reservan para errores de programación: un callback de `reader` no invocable,
o una llamada sobre una conexión cerrada.

Tres de ellos devuelven más de un número, como array:

| Método | Claves |
|---|---|
| `scalar()` | `rc`, `type`, `value` |
| `readerToJson()` | `rc`, `rows`, `json` |
| `getBlob()` | `rc`, `copied`, `data` |
| `insertBlob()` / `updateBlob()` | `rc`, `written` |

`scalar()` reserva de antemano 511 bytes de destino para resultados TEXT/BLOB,
porque sin él `execute_scalar` informa solo el tipo.

## Notas aprendidas al construir esta extensión

1. **El nombre del símbolo C no es el que uno escribe.** `PHP_METHOD(a_b, c)`
   pone en minúscula todos los tokens excepto el último, así que
   `LibSqlite_Connection` se convierte en `zim_libsqlite_Connection_rollback`
   mientras `ZEND_ME(a_b, c)` expande a `zim_` + el nombre de clase tal cual.
   Las dos escrituras deben coincidir o la entrada de clase referencia
   símbolos que no existen. `libsqlite_Connection` satisface a ambas.
2. **`config.m4` necesita `PHP_SUBST`, no solo una variable.** Asignar
   `-L<dir> -l:libsqlite3.so -Wl,-rpath,<dir>` a `LIBSQLITE_SHARED_LIBADD` (o
   mediante `PHP_EVAL_LIBLINE` / `PHP_ADD_LIBRARY_WITH_PATH`) deja el Makefile
   generado sin ella. La regla de enlace usa `$(LIBSQLITE_SHARED_LIBADD)`, así
   que el `.so` enlazó con **todos** los símbolos `sqlite_core_*` sin resolver
   — el linker lo permite en un módulo compartido — y `dlopen` fallaba al
   cargar con `Could not load dynamic library ... undefined symbol:
   sqlite_core_json_writer_callback` mientras `readelf -d` mostraba un
   DT_NEEDED que solo contenía `libc.so.6`. La corrección es
   `PHP_SUBST(LIBSQLITE_SHARED_LIBADD)` después de la asignación, y `-lsqlite3`
   (no `-llibsqlite3`, que buscaría `liblibsqlite3.so`).
3. **`Z_PARAM_VARIADIC` toma tres argumentos** en PHP 8.2:
   `Z_PARAM_VARIADIC(pass_by_ref_flag, dest, dest_num)`. La forma de dos
   argumentos que usan extensiones más antiguas no compila aquí.
4. **`zend_fcall_info_init` devuelve un `zend_string**` como nombre**, no un
   `char**`: `(callable, check_flags, fci, fcc, &callable_name, &error)` donde
   `error` es un `char*` simple y hay que liberarlo con `efree`.
5. **`ZEND_ARG_CALLABLE` no existe** en este PHP; usar
   `ZEND_ARG_TYPE_INFO(0, callback, IS_CALLABLE, 0)`.
6. **`zend_bool_t` se eliminó** en PHP 8 — usar `bool`.
7. **El `.ini` importa.** Cargar la extensión con
   `-d extension=libsqlite.so` solo prueba que el `.so` existe; escribir
   `30-libsqlite.ini` en el conf.d que reporta `php -i` prueba que el
   *motor* la carga, que es lo que hace una instalación real y lo que saca a
   la luz un DT_NEEDED faltante.
8. **`txexpected` se compara contra `autocommit`,** así que `1` significa *no
   debe haber transacción* y `0` *sí debe haberla* — lo contrario de lo que
   sugiere el nombre. `execute_transaction` devuelve `-1` cuando la
   expectativa no se cumple, `1` cuando la sentencia corre y `0` cuando falla
   la sentencia. El ejemplo afirma los tres.
9. **Las constantes registradas son globales.** Las constantes
   `LIBSQLITE_TYPE_*` que registra esta extensión no deben ser redeclaradas
   por el ejemplo ("Constant already defined"), y `use const NOMBRE` sobre un
   nombre no compuesto no hace nada y PHP lo advierte.
10. **El abort del callback del reader es visible.** `person` tiene tres
    filas cuando corre la comprobación de abort, así que el callback se
    invoca exactamente una vez y `reader()` informa las filas realmente
    procesadas — que es como se comprueba el "un retorno distinto de cero
    aborta" del contrato en vez de solo afirmarlo.

## Limitación conocida

`libsqlite_string_format` acepta **como máximo tres argumentos**. PHP no
puede construir una `va_list` de C en tiempo de ejecución, así que el
wrapper despacha según el tipo PHP de cada argumento y reenvía el literal
que ese tipo espera — la única forma de llamar a una variadic sin
reimplementarla. Todas las combinaciones de hasta tres argumentos
`int`/`float`/`string` están cubiertas; un cuarto, o cualquier otro tipo,
 lanza. Es el único símbolo de los 34 que no se puede reenviar tal cual
desde PHP, y se declara aquí en lugar de omitirse en silencio, según
`bindings/CONTRACT.md` §3.