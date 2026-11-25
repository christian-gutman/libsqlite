# PHP — resumen en español

> Traducción del resumen de [`README.md`](README.md). La documentación
> canónica está en inglés (ver `CONTRIBUTING.md`); este fichero conserva la
> versión en español.

[English](README.md) · **Español**

`libsqlite.php` declara los 34 símbolos congelados con la extensión
**FFI incluida en PHP** — sin paquetes externos y sin compilar.
`example_full.php` es el flujo canónico. Único requisito: `php-cli` con la
extensión FFI cargada.

## Ficheros

| Fichero | Qué es |
|---------|--------|
| `libsqlite.php` | El binding FFI: scope, constructores de parámetros, helpers de la regla del alocador |
| `example_full.php` | Flujo canónico (22 checks) |
| `src/php_shim.c` | ~120 líneas de C que cierran la ausencia de callbacks en PHP FFI |
| `libsqlite_shim.so` | El shim compilado (lo construye el contenedor, ignorado por git) |

## Requisitos

```bash
php -m | grep -i ffi          # debe imprimir FFI
```

Detéctala con `extension_loaded('ffi')` / `class_exists('FFI')` —
**no** con `defined('FFI_SCOPE')`, que es falso incluso con FFI cargada
(verificado en PHP 8.2).

> **Nota de Docker:** las imágenes oficiales `php:*` se compilan desde
> fuente *sin* FFI (no aparece en `php -m` y no hay árbol
> `/etc/php/*/mods-available` donde hacer `phpenmod`). El contenedor usa
> por eso `debian:bookworm-slim` con `php-cli` + `php8.2-ffi`. Para
> instalación local en Debian/Ubuntu:
> `apt-get install php-cli php8.2-ffi`.

## Compilar el shim

`example_full.php` necesita el shim, porque PHP FFI no puede crear un
puntero a función C (ver *El hueco del callback* más abajo):

```bash
LIB=/path/to/libsqlite3.so
LIBDIR=$(dirname "$LIB")
cc -shared -fPIC -O2 -I ../../src/sqlite/core src/php_shim.c \
   -L "$LIBDIR" -lsqlite3 -Wl,-rpath,"$LIBDIR" -o libsqlite_shim.so
```

## Compilar y ejecutar

```bash
php example_full.php ../../bin/x64-Linux/shared/Release/libsqlite3.so ./libsqlite_shim.so
```

## Contenedores

Dos contenedores, ambos se lanzan con `docker/build.cmd` y **ninguno forma
parte del CI** (ver `docker/readme.md`):

| Contenedor | Base | Compila el shim con | Verifica |
|------------|------|---------------------|----------|
| `php.Dockerfile` | `debian:bookworm-slim` + `php-cli`/`php8.2-ffi` | `cc` dentro del contenedor | Construye la librería, CTest, y luego el ejemplo contra el `.so` |
| `php-windows.Dockerfile` | `mcr.microsoft.com/windows/servercore` | MSVC (`cl`) | El mismo flujo contra el `.dll` + la biblioteca de importación |

```bat
docker\build.cmd php
```

El shim se compila dentro de cada contenedor para la plataforma que lo va a
ejecutar, así que no hay binario precompilado que publicar.

## El hueco del callback, y cómo lo cierra el shim

PHP FFI **no puede** convertir un closure de PHP en un puntero a función C:
la extensión no tiene ningún mecanismo de callback (la lista completa de
métodos es cdef, load, scope, new, free, cast, type, typeof, arrayType,
addr, sizeof, alignof, memcpy, memcmp, memset, string, isNull — verificado
en PHP 8.2, y no existe `FFI::callback`).

Eso deja dos de los 34 símbolos congelados inalcanzables desde FFI puro, y
el shim los cubre sin reimplementar ningún comportamiento — el mismo
enfoque que el shim de R (`bindings/r/src/libsqlite_shim.c`) y el
trampolín de Swift:

| Función del shim | Qué cierra |
|------------------|------------|
| `sqlite_php_reader_rows(db, sql, &rows, &columns, err, max)` | Conduce `sqlite_core_execute_reader` mediante un trampolín en C y devuelve los contadores de filas y columnas |
| `sqlite_php_reader_json(db, sql, &json, &rows, err, max)` | Conduce `sqlite_core_execute_reader` con el exportado `sqlite_core_json_writer_callback` y un contexto `sqlite_core_json_builder_t`, envolviendo el documento con el mismo protocolo `"[\n"…"\n]"` que usa `sqlite_core_execute_reader_to_json` |
| `sqlite_php_json_writer_callback()` | La dirección del callback congelado, para quien quiera pasarlo por su cuenta |

El ejemplo compara el documento del shim con el de
`execute_reader_to_json` **byte a byte**, así que el envoltorio está
probado, no supuesto. Verificado en el contenedor: **22 checks, 0 fallos**,
CTest 7/7.

Quien solo quiera la capa FFI (sin shim) sigue teniendo los otros 32
símbolos: el reader se alcanza con el exportado
`sqlite_core_execute_reader_to_json`, la vía documentada en un build
compartido.

## Lo que añade la capa PHP

- `LibSqlite` construye el scope FFI una sola vez
  (`FFI::cdef($code, $lib)`) y es dueño de todo buffer cuyo puntero nativo
  va a leer C;
- `intParam` / `textParam` / `paramArray` construyen los parámetros
  nombrados como un `sqlite_core_param_t[]` contiguo; el texto se copia a
  buffers **no propios** fijados en una lista de retención, porque FFI se
  niega a asignar un puntero propio a un campo `char *` de un struct;
- `staticString()` lee datos estáticos de la librería (versiones,
  `error_message`, `param_clean_prefix`) — esos llegan ya como strings de
  PHP y **no** deben liberarse;
- `takeString()` lee un string del alocador de SQLite, lo copia a PHP y
  libera el original con `sqlite_core_string_free` (nunca del lado PHP);
- `scalarOutText()` reserva el buffer de destino que `execute_scalar`
  necesita para resultados TEXT;
- `readerCallbackSupport()` informa exactamente de lo que la capa FFI sola
  no puede hacer, para que nadie tenga que adivinarlo.

## Notas aprendidas al construir este binding

1. **Detección de FFI** — `defined('FFI_SCOPE')` es falso incluso con la
   extensión cargada; usa `extension_loaded('ffi')`.
2. **`die()` sale con código 0** — un `CMD` de contenedor que reporta el
   código de salida del ejemplo trataría un fallo como éxito. El binding
   escribe a STDERR y hace `exit(2)`.
3. **`FFI::load()` es para ficheros de cabecera** — toma un solo
   argumento. Para declarar código en línea y enlazar una biblioteca:
   `FFI::cdef($code, $lib)`.
4. **Sintaxis de tipo C, no de closure PHP** —
   `FFI::type('int function(FFI\CData, ...): int')` lanza
   `ParserException`; un puntero a función es
   `int(*)(void*,int,int,void*)`.
5. **Los structs necesitan el scope de la instancia** — el `FFI::new()`
   estático resuelve contra el scope *precargado*, que no conoce este
   cdef, así que `FFI::new('sqlite_core_param_t')` falla con
   "Undefined C type". Usa `$lib->new(...)`.
6. **Los punteros propios no se asignan a structs** — "Attempt to perform
   assign of owned C pointer". Crea los buffers con
   `$lib->new('char[N]', false)` (no propios) y fíjalos.
7. **Los retornos `char *` se vuelven strings de PHP** — FFI los
   convierte al retornar, destruyendo el puntero antes de poder liberarlo.
   Las funciones que devuelven memoria del heap
   (`sqlite_core_string_format`, `sqlite_core_string_concat`,
   `sqlite_core_string_utf8_to_ansi`, `sqlite_core_string_ansi_to_utf8`,
   `sqlite_core_execute_reader_to_json`) se declaran `void *` para poder
   respetar la regla del alocador.
8. **Límite de string en `memcpy`** — pedir `strlen+1` bytes de un string
   de PHP lanza "Attempt to read over string boundary". Copia la longitud
   y termina a mano.
9. **Solo `new` y `type` tienen forma de instancia** — `addr`, `memcpy`,
   `sizeof`, `string`, `isNull` y `cast` caen por el `__call` del objeto
   FFI a una búsqueda de función C y fallan con "undefined C function".
   Esos dos van con `$lib->`, el resto siguen estáticos.
10. **No hay callbacks en absoluto** — ver *El hueco del callback*: por eso
    existe el shim, y por eso el reader no se conduce desde FFI puro.
11. **`uintptr_t` necesita `<stdint.h>`** — `SIZE_MAX`/`uintptr_t` no
    están disponibles con las cabeceras que MSVC incluye de forma
    transitiva, así que el shim incluye `<stdint.h>` explícitamente (el
    mismo arreglo que necesitó `sqlite_core_builder.c` en Linux).