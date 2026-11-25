# PHP — resumen en español

> Traducción del resumen de [`README.md`](README.md). La documentación
> canónica está en inglés (ver `CONTRIBUTING.md`); este fichero conserva la
> versión en español.

**English** · [Español](README.md)

`libsqlite.php` declara los 34 símbolos congelados con la extensión
**FFI incluida en PHP** — sin paquetes externos y sin compilar.
`example_full.php` es el flujo canónico. Único requisito: `php-cli` con la
extensión FFI cargada.

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

## Compilar y ejecutar

```bash
php example_full.php ../../bin/x64-Linux/shared/Release/libsqlite3.so
```

## Contenedor

```bat
docker\build.cmd php
```

`debian:bookworm-slim` + `php-cli`/`php8.2-ffi`: compila la librería, corre
la suite CTest y luego el ejemplo contra el objeto compartido de Linux.

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
  necesita para resultados TEXT.

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
10. **No hay callbacks en absoluto** — PHP FFI no tiene mecanismo para
    convertir un closure en puntero a función C (`FFI::callback` no
    existe). Es la misma limitación que tienen los bindings de Java, C#,
    Go, C++ y Kotlin: `sqlite_core_execute_reader` no se puede conducir
    desde PHP, y `sqlite_core_json_writer_callback` — declarado como el
    resto de símbolos congelados — tampoco puede pasársele. El reader se
    ejercita con `sqlite_core_execute_reader_to_json`, la vía documentada
    en un build compartido. Un callback real requiere un trampolín en C,
    como el que llevan R y Swift.

Verificado en el contenedor: **19 checks, 0 fallos**, CTest 7/7.