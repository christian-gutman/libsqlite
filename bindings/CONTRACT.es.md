# Contrato de interfaz de los bindings

[English](CONTRACT.md) · **Español**

> Traducción de [`CONTRACT.md`](CONTRACT.md). La documentación canónica está
> en inglés (ver `CONTRIBUTING.md`); este fichero conserva la versión en
> español.

Todos los bindings de esta carpeta envuelven la **API congelada de 34
símbolos** (`src/sqlite/core/sqlite.def`). Este documento es el contrato de
interfaz que todos deben cumplir, para que un ejemplo canónico produzca el
mismo comportamiento en cualquier lenguaje y los resultados se puedan
comparar línea a línea.

Cubre: nombres, parámetros, tipos de datos, códigos de retorno, la regla del
alocador y las aserciones exigidas en cada paso.

## 1. Contrato del código de salida

Todo `example_full.*` **sale con el número de checks fallidos** (`0` si todo
pasa). Es lo que hace que un ejemplo sirva de smoke test: un `CMD` de
contenedor que reporta el código de salida falla cuando el ejemplo falla.

> Dos bugs contra los que existe este contrato: `die()` en PHP sale con
> estado 0, y `System.exit` de Java ante una excepción no capturada es
> distinto de cero pero silencioso. Imprimí siempre
> `N checks, N failures` y salí con el contador de fallos.

## 2. El flujo canónico (19 pasos)

Cada paso nombra el símbolo congelado que debe llamar y qué afirma.

| # | Símbolo congelado | Afirma |
|---|-------------------|--------|
| 1 | `sqlite_core_open_or_create` | El handle no es `NULL` y `out_rc == SQLITE_OK` |
| 2 | `sqlite_core_execute_command` | `CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)` devuelve `SQLITE_OK` |
| 3 | `sqlite_core_execute_non_query` | `INSERT INTO person(id, name) VALUES (:id, :name)` con id `42`, nombre `"Ada"` → **1** fila |
| 4 | `sqlite_core_execute_reader` | `SELECT id, name FROM person ORDER BY id` → el callback visita **1** fila; un retorno distinto de cero aborta |
| 5 | `sqlite_core_execute_scalar` | `SELECT COUNT(*) FROM person` → **1** |
| 6 | `sqlite_core_execute_reader_to_json` | `SELECT id, name FROM person ORDER BY id` → 1 fila, el documento empieza con `[` y contiene `{"id": 42, "name": "Ada"}` |
| 7 | `sqlite_core_insert_json` | `[{"id":2,"name":"Line1"},{"id":3,"name":"Line2"}]` en `person` → **2** filas |
| 8 | `sqlite_core_insert_blob` → `sqlite_core_get_blob` → `sqlite_core_last_insert_rowid` | Fichero de 1024 bytes con `0x00` dentro; `insert_blob` devuelve el número de bytes; `get_blob` coincide byte a byte; `last_insert_rowid > 0` |
| 9 | `sqlite_core_commit_transaction` → `sqlite_core_begin_transaction` → `sqlite_core_rollback_transaction` | commit sin transacción falla; begin funciona; insert dentro de la transacción; rollback funciona; el contador vuelve a 3 |
| 10 | `sqlite_core_update_blob` | Un fichero del doble de tamaño → `SQLITE_CORE_BLOB_TOO_SMALL` (-3) |
| 11 | `sqlite_core_open_readonly` | Abrir un fichero inexistente falla con `SQLITE_CANTOPEN` |
| 12 | `sqlite_core_execute_non_query` | `SELECT 1; SELECT 2` → `SQLITE_MISUSE` |
| 13 | `sqlite_core_close` | Devuelve **1** |

Los pasos 8 a 13 son la mitad difícil: es donde todos los bindings han
encontrado bugs de verdad.

## 3. Convención de nombres

La API C es `snake_case` bajo el prefijo `sqlite_core_`. Cada binding lo
mapea a su propio idioma, pero el mapeo debe ser **uno a uno y documentado en
el README del binding**. Ningún símbolo puede omitirse en silencio: si un
lenguaje no alcanza alguno, se dice en el README (con el motivo) en vez de
descartarlo.

| Símbolo C | Python | C# | Java | Go | Rust | Node.js | Ruby | Swift | PHP (FFI) | PHP (Zend) |
|---|---|---|---|---|---|---|---|---|---|---|
| `sqlite_core_open_or_create` | `open_or_create` | `OpenOrCreate` | `openOrCreate` | `OpenOrCreate` | `open_or_create` | `openOrCreate` | `open_or_create` | `openOrCreate` | `open_or_create` | `LibSqlite\Connection::openOrCreate` |
| `sqlite_core_execute_command` | `execute_command` | `ExecuteCommand` | `executeCommand` | `ExecuteCommand` | `execute_command` | `executeCommand` | `execute_command` | `executeCommand` | `execute_command` | `Connection::command()` |
| `sqlite_core_execute_non_query` | `execute_non_query` | `ExecuteNonQuery` | `executeNonQuery` | `ExecuteNonQuery` | `execute_non_query` | `executeNonQuery` | `execute_non_query` | `executeNonQuery` | `execute_non_query` | `Connection::nonQuery()` |
| `sqlite_core_execute_scalar` | `execute_scalar` | `ExecuteScalar` | `executeScalar` | `ExecuteScalar` | `execute_scalar` | `executeScalar` | `execute_scalar` | `executeScalar` | `execute_scalar` | `Connection::scalar()` |
| `sqlite_core_execute_reader` | `execute_reader` | `ExecuteReader` | `executeReader` | `ExecuteReader` | `execute_reader` | `executeReader` | `execute_reader` | `executeReader` | `execute_reader` (vía shim) | `Connection::reader()` |
| `sqlite_core_execute_reader_to_json` | `execute_reader_to_json` | `ReaderToJson` | `readerToJson` | `ReaderToJson` | `execute_reader_to_json` | `readerToJson` | `execute_reader_to_json` | `readerToJson` | `execute_reader_to_json` | `Connection::readerToJson()` |
| `sqlite_core_insert_json` | `insert_json` | `InsertJson` | `insertJson` | `InsertJson` | `insert_json` | `insertJson` | `insert_json` | `insertJson` | `insert_json` | `Connection::insertJson()` |
| `sqlite_core_get_blob` | `get_blob` | `GetBlob` | `getBlob` | `GetBlob` | `get_blob` | `getBlob` | `get_blob` | `getBlob` | `get_blob` | `Connection::getBlob()` |
| `sqlite_core_insert_blob` | `insert_blob` | `InsertBlob` | `insertBlob` | `InsertBlob` | `insert_blob` | `insertBlob` | `insert_blob` | `insertBlob` | `insert_blob` | `Connection::insertBlob()` |
| `sqlite_core_update_blob` | `update_blob` | `UpdateBlob` | `updateBlob` | `UpdateBlob` | `update_blob` | `updateBlob` | `update_blob` | `updateBlob` | `update_blob` | `Connection::updateBlob()` |
| `sqlite_core_begin_transaction` | `begin_transaction` | `BeginTransaction` | `beginTransaction` | `BeginTransaction` | `begin_transaction` | `beginTransaction` | `begin_transaction` | `beginTransaction` | `begin_transaction` | `Connection::begin()` |
| `sqlite_core_commit_transaction` | `commit_transaction` | `CommitTransaction` | `commitTransaction` | `CommitTransaction` | `commit_transaction` | `commitTransaction` | `commit_transaction` | `commitTransaction` | `commit_transaction` | `Connection::commit()` |
| `sqlite_core_rollback_transaction` | `rollback_transaction` | `RollbackTransaction` | `rollbackTransaction` | `RollbackTransaction` | `rollback_transaction` | `rollbackTransaction` | `rollback_transaction` | `rollbackTransaction` | `rollback_transaction` | `Connection::rollback()` |
| `sqlite_core_last_insert_rowid` | `last_insert_rowid` | `LastInsertRowId` | `lastInsertRowid` | `LastInsertRowId` | `last_insert_rowid` | `lastInsertRowid` | `last_insert_rowid` | `lastInsertRowid` | `last_insert_rowid` | `Connection::lastInsertRowid()` |
| `sqlite_core_set_busy_timeout` | `set_busy_timeout` | `SetBusyTimeout` | `setBusyTimeout` | `SetBusyTimeout` | `set_busy_timeout` | `setBusyTimeout` | `set_busy_timeout` | `setBusyTimeout` | `set_busy_timeout` | `Connection::setBusyTimeout()` |
| `sqlite_core_error_code` | `error_code` | `ErrorCode` | `errorCode` | `ErrorCode` | `error_code` | `errorCode` | `error_code` | `errorCode` | `error_code` | `Connection::errorCode()` |

Convención por familia de lenguajes: `snake_case` para las familias
derivadas de C y de scripting (Python, Julia, Nim, Ruby, R, PHP, Fortran,
C++), `PascalCase` para .NET/Go/Java/Swift, `camelCase` para Node.js.

## 4. Contrato de parámetros

El enlace es **siempre por nombre**, nunca por ordinal. Un parámetro ausente
del SQL simplemente no se enlaza.

Un parámetro es un registro con tres campos:

| Campo | Tipo | Significado |
|-------|------|-------------|
| `name` | string | El nombre del parámetro, **con o sin** el sigilo `:` / `@` / `?` — ambos deben funcionar, porque `sqlite_core_param_clean_prefix` lo quita |
| `type` | int | Uno de los cinco valores `SQLITE_CORE_TYPE_*` de abajo |
| `value` | mixto | El valor, coercionado a `type`; para TEXT es un string UTF-8 |
| `Direction` | int | `INPUT` (1), `OUTPUT` (2), `INPUTOUTPUT` (3). Por defecto `INPUT` |

Los cinco valores de tipo, que todo binding debe exponer como constantes con
nombre:

| Constante | Valor | Tipo C |
|----------|-------|--------|
| `SQLITE_CORE_TYPE_NULL` | 0 | — |
| `SQLITE_CORE_TYPE_INT` | 2 | `sqlite3_int64` (64 bits de punta a punta) |
| `SQLITE_CORE_TYPE_DOUBLE` | 4 | `double` |
| `SQLITE_CORE_TYPE_TEXT` | 6 | `char *` (UTF-8, `len == -1` significa terminado en NUL) |
| `SQLITE_CORE_TYPE_BLOB` | 8 | `void *` + `len` |

`sqlite_core_param_t` es `{ const char *name; int no_insensitive;
sqlite_core_param_type_t type; sqlite_core_param_direction_t Direction;
union { double dVal; sqlite3_int64 iVal; struct { char *ptr; int len; int
max_len; } blob_or_text; } value; }` — 40 bytes en LP64, 28 en ILP32. La
alineación de la unión importa: un binding que se equivoque falla solo en
una plataforma.

**Resultados TEXT de `execute_scalar`**: quien llama debe reservar antes
`value.blob_or_text.ptr` + `max_len`; los valores mayores se truncan de forma
silenciosa. Sin buffer solo se reporta el tipo.

## 5. Contrato de códigos de retorno

De `docs/behavior.md`, sin cambios:

| Retorno | Significado |
|--------|-------------|
| `>= 0` | Éxito: filas afectadas/devueltas en funciones con valor, `1` en las de bandera |
| `0` | Booleano falso (`is_readonly` sobre una base escribible) |
| `-1` | Error genérico; detalles en `err_buf` |
| `-2` | `SQLITE_CORE_BUFFER_TOO_SMALL` (`get_blob`) |
| `-3` | `SQLITE_CORE_BLOB_TOO_SMALL` (`update_blob`, upsert de `insert_blob`) |
| Código SQLite negativo | Fallo del motor (refleja `SqlException.Number` negado) |
| Código SQLite positivo | Las `execute_*` conservan los códigos nativos (`SQLITE_MISUSE`, `SQLITE_CANTOPEN`) |

Toda función que acepte `err_buf` **lo limpia al entrar** y lo rellena con el
mensaje de SQLite en caso de fallo.

## 6. La regla del alocador

Cualquier `char *` que devuelva la librería desde una función que retorna
memoria del heap (`sqlite_core_string_format`,
`sqlite_core_string_concat`, `sqlite_core_string_utf8_to_ansi`,
`sqlite_core_string_ansi_to_utf8`, `sqlite_core_execute_reader_to_json`) se
reserva con el **alocador de SQLite** y debe liberarse con
`sqlite_core_string_free` — nunca el alocador del huésped, nunca un GC.

Los datos estáticos (`sqlite_core_version`, `sqlite_core_sqlite_version`,
`sqlite_core_error_message`, `sqlite_core_param_clean_prefix`) son de la
librería y **no** deben liberarse.

## 7. Contrato del callback

`sqlite_core_row_callback_fn(stmt, columns, hasrows, ctx)`: una llamada por
fila, y **exactamente una con `hasrows == 0` cuando el conjunto está vacío**.
Devolver distinto de cero aborta la iteración.

Los lenguajes que no pueden formar un puntero a función C desde sus propios
callables (PHP FFI, las variantes JNA de Java/C#/Go/C++) deben documentar el
hueco y ejercitar el reader mediante
`sqlite_core_execute_reader_to_json`, o mediante un pequeño trampolín
compilado — como hacen `bindings/r/src/libsqlite_shim.c`, `bindings/swift` y
`bindings/php/src/php_shim.c`.

## 8. Cumplimiento actual (medido, no supuesto)

Comprobado escaneando cada `example_full.*` por los nombres que llama.

> **Leé esto antes de confiar en un número.** Esta tabla es una
> **heurística**, y tres borradores seguidos se equivocaron de la misma
> forma: el escáner solo conocía una ortografía por símbolo. `db.command()`
> es `execute_command`; `db.read_rows()` es `execute_reader`; `scalar_int` /
> `scalarInt` / `reader_count` son `execute_scalar` / `execute_reader` con
> otro nombre. Cada una de esas infravaloró un binding que de hecho estaba
> completo. **Cada binding debe confirmar su propia fila antes de creer el
> número**: el escaneo encuentra candidatos, no pruebas.

| Binding | Cobertura observada | Huecos conocidos |
|---------|---------------------|------------------|
| C++ | 17/17 | — |
| Fortran | 17/17 | — |
| Go | 17/17 | — |
| Java | 17/17 | — |
| Julia | 17/17 | — |
| Kotlin | 17/17 | — |
| Nim | 17/17 | — |
| Python | 17/17 | — |
| Ruby | 17/17 | — |
| Swift | 17/17 | — |
| FoxPro | 15/17 | `error_code`, `error_message` |
| C# | 14/17 | `set_busy_timeout`, `error_code`, `error_message` |
| PHP (FFI) | 14/17 | ídem, más `execute_reader`/`json_writer_callback` vía el shim |
| R | 14/17 | ídem |
| Rust | 14/17 | ídem |
| Node.js | 16/17 | `execute_scalar` se evita **a propósito y está documentado**: lee el escalar por JSON "avoids the out-struct in this example" |
| Clipper | 0/17 | `libsqlite.prg` declara la superficie congelada (34/34 según `verify_symbols.py`), pero `example_full.prg` es un **stub**: contiene solo los helpers (`Check`, `FnLoad`, `Fixture`, `RowCount`) y **no llama a ningún símbolo**, así que el flujo canónico no está implementado. La afirmación del README de que `example_full.prg` "implementa el flujo canónico" es falsa. |

**Las obligaciones que crea esta tabla:** cada fila con huecos, o
(a) amplía su ejemplo para cerrarlos, o (b) documenta en su README por qué el
lenguaje no alcanza ese símbolo. Un hueco silencioso no es aceptable, porque
la siguiente persona asume que el símbolo se probó.

**Lote cerrado:** el hueco más común era `set_busy_timeout`, `error_code` y
`error_message`. C#, PHP, R, Rust y Node.js ahora afirman los tres (Node.js
leyéndolos tras el fallo deliberado de multi-sentencia que ya provoca), y C++ y
Swift ganaron `error_code`/`error_code_extended`. R también necesitaba esos
tres wrappers en su superficie R: el shim los tenía (`sqlite_r_connection` op
2/3/4/5) pero nada los exponía. Catorce bindings llegan a 17/17.

**Lo que queda necesita un toolchain que este repositorio no puede ejercitar:**
`error_code`/`error_message` de FoxPro, cuyo manejo de strings es específico
de VFP (y cuyo ejemplo declara `sqlite_core_string_free` como `STRING`
mientras `binding.prg` lo declara `void`); y Clipper, cuyo flujo canónico es un
stub y necesita Harbour/xHarbour para escribirlo y correrlo. Ambos quedan
anotados como limitaciones documentadas bajo 8(b), porque escribirlos a ciegas
arriesga romper un ejemplo que hoy se mantiene.

**Las obligaciones que crea esta tabla:** cada fila con huecos, o
(a) amplía su ejemplo para cerrarlos, o (b) documenta en su README por qué el
lenguaje no alcanza ese símbolo. Un hueco silencioso no es aceptable, porque
la siguiente persona asume que el símbolo se probó.

## 9. Añadir un binding (checklist)

1. `bindings/<lang>/` con el fichero del binding y `example_full.*`.
2. `README.md` (inglés, canónico) + `README.es.md` si llevaba contenido en
   español — los dos se enlazan entre sí para que el par sea descubrible.
3. `example_full.*` implementa los 13 pasos del §2 y sale con el contador de
   fallos (§1).
4. Expone los cinco valores `SQLITE_CORE_TYPE_*` como constantes con nombre.
5. Sigue la convención de nombres del §3 y documenta el mapeo en el README.
6. Registra el conjunto de símbolos con `python bindings/verify_symbols.py`
   (34/34).
7. Añade un contenedor (`docker/bindings/<lang>.Dockerfile`) y el lenguaje a
   `docker/build_all.cmd` — **no** al CI (`docker/readme.md`).
8. Actualiza: tabla de cobertura de `bindings/README.md`, la tabla §8 de este
   contrato, `docs/PROGRESS.md`, `docs/ROADMAP.md`, `CHANGELOG.md`.

## 10. Los artefactos de compilación se quedan fuera del repositorio

El escaneo que produjo el §8 tropezó con salida compilada en el árbol de
trabajo: `bindings/java/classes/ExampleFull.class` y
`bindings/swift/ExampleFull.o`.

Verificado con `git ls-files` y `git check-ignore`: **ninguno está
trackeado**, y ambos ya están ignorados (`.gitignore:34
bindings/java/classes/` y `.gitignore:10 *.o`). Así que esto no es un
defecto: se deja anotado como la regla para bindings nuevos — la salida de
compilación va en `.gitignore` en el mismo commit que el binding, porque
queda obsoleta en cuanto cambia el toolchain y rompe un clonar-y-compilar
limpio.
