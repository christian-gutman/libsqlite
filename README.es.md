<div align="center">

# libsqlite

**Biblioteca conectora de SQLite de alto rendimiento para C**

![CI](https://img.shields.io/github/actions/workflow/status/christian-gutman/libsqlite/ci.yml?branch=main&label=CI&style=flat-square)
![Release](https://img.shields.io/github/v/release/christian-gutman/libsqlite?style=flat-square)
![Licencia](https://img.shields.io/github/license/christian-gutman/libsqlite?style=flat-square)
![SQLite](https://img.shields.io/badge/SQLite-3.54.1-003B57?style=flat-square&logo=sqlite&logoColor=white)
![C11](https://img.shields.io/badge/C-11-00599C?style=flat-square&logo=c&logoColor=white)
![CMake](https://img.shields.io/badge/build-CMake-064F8C?style=flat-square&logo=cmake&logoColor=white)
![Plataformas](https://img.shields.io/badge/plataforma-Windows%20%7C%20Linux%20%7C%20macOS-blue?style=flat-square)
![CTest](https://img.shields.io/badge/CTest-3%20suites-brightgreen?style=flat-square)

**[Documentation in English](README.md)** · **[Contrato de comportamiento](docs/behavior.es.md)** · **[Hoja de ruta](docs/ROADMAP.md)** · **[Changelog](CHANGELOG.md)** · **[Licencia](LICENSE)**

</div>

> **Nota sobre el proyecto:** originalmente este código pertenecía a la cuenta
> `@christiangutman`. Tras un robo de hardware se perdió el acceso definitivo
> a dicha cuenta y a su repositorio original. Las fuentes fueron recuperadas
> de manera íntegra desde respaldos externos y ahora se publican de forma
> abierta. **Este repositorio es el hogar oficial y definitivo** de
> libsqlite — los reportes de bugs y las contribuciones son bienvenidos
> aquí.

libsqlite es un conector de bases de datos SQLite escrito en C99/C11
portable sobre la amalgamación oficial `sqlite3.c`. Expone una biblioteca
compartida (`.dll` / `.so` / `.dylib`) con semántica estilo ADO.NET
(`ExecuteReader`, `ExecuteScalar`, `ExecuteNonQuery`, parámetros con
dirección, transacciones), invocable desde cualquier lenguaje que pueda
hacer llamadas a C (C#, C++, Delphi, VB.NET, Python vía ctypes, ...).

Al llevar el motor embebido no hace falta ODBC, proveedores OLE DB ni
instalar SQLite en el sistema — ideal para distribución XCOPY.

**[Documentation in English](README.md)**

## Características

- **Motor embebido** — se compila la amalgamación oficial de SQLite (3.54.1);
  no hay que instalar nada más en la máquina destino.
- **Semántica .NET** — la API replica sus equivalentes de ADO.NET, mínimo
  esfuerzo para desarrolladores .NET.
- **Conversión UTF-8 / ANSI** — la codificación se detecta, no se asume
  (`sqlite_core_string_utf8_to_ansi`, `sqlite_core_string_ansi_to_utf8`).
- **Salida JSON dinámica** — callback de escritura JSON incorporado y
  `sqlite_core_execute_reader_to_json` en una sola llamada; inserción masiva
  `sqlite_core_insert_json` basada en JSON1.
- **Totalmente multiplataforma** — un único sistema de build (CMake) para
  Windows (Win32/x64), Linux y macOS, compartida o estática.
- **Sin acceso a la red al compilar** — las únicas dependencias externas
  fueron absorbidas en `src/sqlite/core/compat/`.
- **Exportaciones congeladas** — `sqlite.def` es la fuente de verdad; un
  test automatizado verifica en runtime todos los símbolos públicos.

## Requisitos

| Plataforma | Herramientas |
|------------|--------------|
| Windows | Visual Studio 2022 (carga de desarrollo para escritorio con C++) o equivalente, CMake ≥ 3.15 |
| Linux | GCC o Clang, `build-essential`, CMake ≥ 3.15 |
| macOS | Xcode command line tools, CMake ≥ 3.15 |

El código es C11 estándar y compila sin advertencias (`/W4` en MSVC).

## Inicio rápido

Clona y compila la DLL compartida Win32 (o usa el helper incluido
`cmp_dll.bat`, que hace lo mismo y limpia al terminar):

```bash
git clone https://github.com/christian-gutman/libsqlite.git
cd libsqlite

# Windows x64 (usar -A Win32 para 32 bits)
cmake -S src/sqlite -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure   # opcional: correr los tests
```

Linux:

```bash
sudo apt-get install -y build-essential cmake
cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

macOS:

```bash
xcode-select --install
cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Las salidas de build se centralizan en
`bin/<arch>-<os>/{shared,static}/<config>/`:

```
bin/x64-Windows/shared/Release/libsqlite3.dll     + libsqlite3.lib (biblioteca de importación)
bin/x32-Windows/static/Release/libsqlite3.lib     (biblioteca estática)
bin/x64-Linux/shared/Release/libsqlite3.so
bin/arm64-Darwin/shared/Release/libsqlite3.dylib
```

### Opciones de CMake

| Opción | Predeterminado | Significado |
|--------|----------------|-------------|
| `BUILD_SHARED_LIBS` | `ON` | Biblioteca compartida (`ON`) o estática (`OFF`). |
| `LIBSQLITE3_BUILD_TESTS` | `ON` | Compila las suites CTest (`regression`, `api`, `exports`). |
| `CMAKE_BUILD_TYPE` | `Release` | Generadores de configuración única (Linux/macOS). |

`cmake --install build` instala la biblioteca, las cabeceras y un paquete
CMake (`libsqlite3::libsqlite3`) para consumidores con
`find_package(libsqlite3)`.

### Uso de la biblioteca

`src/sqlite/core/sqlite_core.h` es la cabecera paraguas — inclúyela y enlaza
con la biblioteca generada:

```c
#include <sqlite_core.h>
#include <stdio.h>

/* Una llamada por fila; las contamos (los builds DLL exportan solo los 31
 * símbolos sqlite_core_*, por eso más abajo las filas se muestran como JSON). */
static int on_row(sqlite3_stmt *stmt, int columns, int hasrows, void *ctx)
{
    (void)stmt; (void)columns;
    if (hasrows)
        (*(int *)ctx)++;
    return 0; /* distinto de cero aborta la iteración */
}

int main(void)
{
    int open_rc = 0;
    sqlite3 *db = sqlite_core_open_or_create("demo.db", &open_rc);
    if (db == NULL || open_rc != SQLITE_OK)
    {
        fprintf(stderr, "no se pudo abrir demo.db (rc=%d)\n", open_rc);
        return 1;
    }

    char err[128] = "";
    if (sqlite_core_execute_command(db,
            "CREATE TABLE IF NOT EXISTS person(id INTEGER PRIMARY KEY, name TEXT)",
            err, sizeof(err)) != SQLITE_OK)
    {
        fprintf(stderr, "falló el DDL: %s\n", err);
        sqlite_core_close(db);
        return 1;
    }

    sqlite_core_param_t p[2];
    memset(&p, 0, sizeof(p));
    p[0].name = "id";
    p[0].type = SQLITE_CORE_TYPE_INT;
    p[0].Direction = SQLITE_CORE_DIRECTION_INPUT;
    p[0].value.iVal = 42;

    p[1].name = "name";
    p[1].type = SQLITE_CORE_TYPE_TEXT;
    p[1].Direction = SQLITE_CORE_DIRECTION_INPUT;
    p[1].value.blob_or_text.ptr = "Ada";
    p[1].value.blob_or_text.len = -1;              /* terminada en NUL */

    int rc = sqlite_core_execute_non_query(db,
        "INSERT INTO person(id, name) VALUES (:id, :name)", p, 2, err, sizeof(err));
    if (rc != 1) /* las funciones con valor devuelven las filas afectadas */
    {
        fprintf(stderr, "falló el insert (rc=%d): %s\n", rc, err);
        sqlite_core_close(db);
        return 1;
    }

    int vistas = 0;
    rc = sqlite_core_execute_reader(db,
        "SELECT id, name FROM person ORDER BY id", NULL, 0,
        on_row, &vistas, err, sizeof(err));
    if (rc < 0)
    {
        fprintf(stderr, "falló el select (rc=%d): %s\n", rc, err);
        sqlite_core_close(db);
        return 1;
    }
    printf("el reader vio %d filas\n", vistas);

    /* Mismas filas como JSON (funciona en DLL y en estático). */
    char *json = NULL;
    rc = sqlite_core_execute_reader_to_json(db,
        "SELECT id, name FROM person ORDER BY id", NULL, 0, &json, err, sizeof(err));
    if (rc < 0 || json == NULL)
    {
        fprintf(stderr, "falló el json (rc=%d): %s\n", rc, err);
        sqlite_core_close(db);
        return 1;
    }
    printf("json: %s\n", json);
    sqlite_core_string_free(json);

    if (sqlite_core_close(db) != 1)
    {
        fprintf(stderr, "falló el cierre\n");
        return 1;
    }
    return 0;
}
```

MSVC (Developer prompt, DLL y biblioteca de importación en las rutas
indicadas arriba):

```bat
cl /W4 /I src\sqlite\core demo.c bin\x64-Windows\shared\Release\libsqlite3.lib
```

GCC/Clang:

```bash
cc -I src/sqlite/core demo.c \
   -L bin/x64-Linux/shared/Release -llibsqlite3 \
   -Wl,-rpath,'$ORIGIN' -o demo
```

Las cadenas devueltas por la biblioteca (por ejemplo de
`sqlite_core_execute_reader_to_json`) se reservan con el asignador de SQLite
y deben liberarse con `sqlite_core_string_free`.

## Tests

```bash
# Windows
ctest --test-dir build -C Release --output-on-failure
# Linux / macOS
ctest --test-dir build --output-on-failure
```

| Suite | Qué verifica |
|-------|--------------|
| `regression` | Los bugs auditados ya corregidos: inversión de `is_readonly`, escapado/truncado del escritor JSON, inyección, robo de transacción, enteros de 64 bits, lectura de BLOB, conversión ANSI→UTF-8. |
| `api` | Paridad .NET: parámetros nombrados y bind por nombre, parámetros OUTPUT vía `RETURNING`, abort del callback del reader, conjuntos vacíos, rutas de error, clamping del busy timeout, round-trip de encoding. |
| `exports` | Todos los símbolos públicos de la API están realmente exportados por la biblioteca compartida (`sqlite.def` no puede desviarse de las cabeceras). |

Las mismas suites se ejecutan en cada push mediante GitHub Actions
(`.github/workflows/ci.yml`): matriz compartida en Windows Win32/x64, Linux
y macOS, más un job de build estático.

## Referencia de la API

Macros de versión: `SQLITE_CORE_VERSION_STR` (`"1.0.0"`, este wrapper) y
`SQLITE_CORE_SQLITE_VERSION_STR` (motor embebido, `"3.54.1"`).

### Tipos principales

```c
typedef enum { SQLITE_CORE_TYPE_NULL, SQLITE_CORE_TYPE_INT,
               SQLITE_CORE_TYPE_DOUBLE, SQLITE_CORE_TYPE_TEXT,
               SQLITE_CORE_TYPE_BLOB } sqlite_core_param_type_t;

typedef enum { SQLITE_CORE_DIRECTION_INPUT, SQLITE_CORE_DIRECTION_OUTPUT,
               SQLITE_CORE_DIRECTION_INPUTOUTPUT } sqlite_core_param_direction_t;

typedef struct {
    const char *name;
    int no_insensitive;                  /* 1 = coincidencia sensible a mayúsculas */
    sqlite_core_param_type_t type;
    sqlite_core_param_direction_t Direction;
    union {
        double dVal;
        sqlite3_int64 iVal;
        struct { char *ptr; int len; int max_len; } blob_or_text;
    } value;
} sqlite_core_param_t;

typedef int (*sqlite_core_row_callback_fn)(
    sqlite3_stmt *stmt, int columns, int hasrows, void *pUserContext);
```

Constantes: `SQLITE_CORE_BUFFER_TOO_SMALL` (`-2`, `sqlite_core_get_blob`).

### Diagnóstico de errores — `sqlite_core_error`

| Función | Descripción |
|---------|-------------|
| `int sqlite_core_error_code(sqlite3 *db)` | Último código de resultado (`SqlException.Number`). `0` si es `NULL`. |
| `int sqlite_core_error_code_extended(sqlite3 *db)` | Código de resultado extendido. |
| `const char *sqlite_core_error_message(sqlite3 *db)` | Último mensaje de error. |

### Cadenas y codificación — `sqlite_core_string`

| Función | Descripción |
|---------|-------------|
| `char *sqlite_core_string_format(const char *fmt, ...)` | Estilo `sprintf`, resultado en heap. |
| `char *sqlite_core_string_concat(const char *a, const char *b)` | Concatenación; tolera operandos `NULL`. |
| `void sqlite_core_string_free(const char *s)` | Libera cualquier cadena devuelta por la biblioteca; no-op con `NULL`. |
| `char *sqlite_core_string_utf8_to_ansi(const char *s)` | UTF-8 → ANSI (página de códigos activa en Windows). |
| `char *sqlite_core_string_ansi_to_utf8(const char *s)` | ANSI → UTF-8. |

Las conversiones UTF-8/ANSI solo son necesarias donde intervienen páginas de
códigos ANSI de Windows; el texto SQL se acepta en ambas codificaciones y se
convierte internamente.

### Conexiones — `sqlite_core_cnx`

| Función | Descripción |
|---------|-------------|
| `sqlite3 *sqlite_core_open(const char *path, int *out_rc)` | Abre un archivo existente; falla (`SQLITE_CANTOPEN`) si no existe. |
| `sqlite3 *sqlite_core_open_or_create(const char *path, int *out_rc)` | Crea el archivo si no existe. |
| `sqlite3 *sqlite_core_open_readonly(const char *path, int *out_rc)` | Solo lectura; falla si el archivo no existe. |
| `sqlite3 *sqlite_core_open_uri_readonly(const char *path, int *out_rc)` | Solo lectura con interpretación de URI, sin tocar la configuración global. |
| `sqlite3 *sqlite_core_open_ex(const char *path, int flags, int *out_rc)` | Control total con flags `SQLITE_OPEN_*` crudos. |
| `int sqlite_core_close(sqlite3 *db)` | Cierre idempotente; seguro con `NULL`; `1` en éxito. |
| `int sqlite_core_is_readonly(sqlite3 *db)` | `1` si es solo lectura, `0` si es escribible, `-1` en error. |
| `sqlite3_int64 sqlite_core_last_insert_rowid(sqlite3 *db)` | Rowid del último INSERT exitoso (`0` si no hubo o si `db` es NULL). |
| `int sqlite_core_set_busy_timeout(sqlite3 *db, int ms)` | Acepta 0..30000 ms, saturando valores fuera de rango. |

### Parámetros — `sqlite_core_parameter`

| Función | Descripción |
|---------|-------------|
| `const char *sqlite_core_param_clean_prefix(const char *name)` | Quita el sigilo `:` / `@` / `?` (`":id"` → `"id"`). |

El bind siempre es por nombre, nunca por ordinal: un parámetro ausente del
SQL simplemente no se enlaza, igual que en ADO.NET.

### Comandos — `sqlite_core_command`

| Función | Descripción |
|---------|-------------|
| `int sqlite_core_execute_command(sqlite3 *db, const char *sql, char *err_buf, int err_buf_max)` | Ejecuta una o más sentencias (DDL/batch). |
| `int sqlite_core_execute_non_query(sqlite3 *db, const char *sql, sqlite_core_param_t *params, int count, char *err_buf, int max)` | `ExecuteNonQuery`; una sola sentencia; devuelve filas afectadas. |
| `int sqlite_core_execute_scalar(sqlite3 *db, const char *sql, sqlite_core_param_t *params, int count, sqlite_core_param_t *out, char *err_buf, int max)` | Primera columna de la primera fila; `SQLITE_CORE_TYPE_NULL` si está vacío. |
| `int sqlite_core_execute_reader(sqlite3 *db, const char *sql, sqlite_core_param_t *params, int count, sqlite_core_row_callback_fn cb, void *ctx, char *err_buf, int max)` | `ExecuteReader`; un callback por fila; el callback aborta devolviendo distinto de cero. |
| `int sqlite_core_execute_transaction(sqlite3 *db, const char *sql, int txexpected, char *err_buf, int max)` | Ejecuta una sentencia esperando estar dentro de una transacción. |

### Transacciones — `sqlite_core_transaction`

| Función | Descripción |
|---------|-------------|
| `int sqlite_core_begin_transaction(sqlite3 *db, char *err_buf, int max)` | `BEGIN IMMEDIATE`; los begins anidados fallan. |
| `int sqlite_core_commit_transaction(sqlite3 *db, char *err_buf, int max)` | Commit; falla sin transacción activa. |
| `int sqlite_core_rollback_transaction(sqlite3 *db, char *err_buf, int max)` | Rollback; falla sin transacción activa. |

### JSON — `sqlite_core_builder`

| Función | Descripción |
|---------|-------------|
| `int sqlite_core_execute_reader_to_json(sqlite3 *db, const char *sql, sqlite_core_param_t *params, int count, char **out_json, char *err_buf, int max)` | Conjunto de resultados como array JSON de objetos; `[]` si está vacío; liberar con `sqlite_core_string_free`. |
| `int sqlite_core_insert_json(sqlite3 *db, const char *table, const char *json, char *err_buf, int max)` | Inserción masiva de un array JSON de objetos (parseado por JSON1); devuelve filas insertadas o un código SQLite negativo. |
| `int sqlite_core_json_writer_callback(sqlite3_stmt *stmt, int columns, int hasrows, void *ctx)` | `sqlite_core_row_callback_fn` listo para usar que acumula JSON en un `sqlite_core_json_builder_t`. |

### Campos — `sqlite_core_field`

| Función | Descripción |
|---------|-------------|
| `int sqlite_core_get_blob(sqlite3 *db, const char *sql, void *dest, int max_len, int *copied)` | Copia un BLOB al buffer del llamante; `SQLITE_CORE_BUFFER_TOO_SMALL` (-2) si no cabe. |
| `int sqlite_core_insert_blob(sqlite3 *db, const char *table, const char *column, sqlite3_int64 rowid, const char *file_path, int chunk_size, int *written, char *err, int max)` | Escribe un **fichero** en una columna BLOB por chunks (64 KiB por defecto). `rowid == 0` inserta; `rowid > 0` hace upsert de ese rowid. Devuelve los bytes escritos, o `SQLITE_CORE_BLOB_TOO_SMALL` (-3) si el blob destino es más pequeño que el fichero. |
| `int sqlite_core_update_blob(sqlite3 *db, const char *table, const char *column, sqlite3_int64 rowid, const char *file_path, int chunk_size, int *written, char *err, int max)` | El mismo streaming por chunks sobre una fila existente; nunca agranda la columna en silencio. |

### Versión

| Función | Descripción |
|---------|-------------|
| `const char *sqlite_core_version(void)` | Versión del wrapper, `"1.0.0"`. |
| `const char *sqlite_core_sqlite_version(void)` | Versión del motor embebido, `"3.54.1"`. |

### JSON de un vistazo

```c
char *json = NULL;
if (sqlite_core_execute_reader_to_json(
        db, "SELECT id, name FROM person ORDER BY id",
        NULL, 0, &json, NULL, 0) >= 0 && json != NULL)
{
    /* json contiene:
     * [
     *   {"id": 1, "name": "Ada"},
     *   {"id": 42, "name": "Grace"}
     * ]
     */
    sqlite_core_string_free(json);
}
```

`sqlite_core_insert_json` es la operación inversa: le pasas ese mismo
documento y el nombre de la tabla, y las filas se insertan con sentencias
parseadas y enlazadas por parámetros (sin concatenar cadenas, sin inyección).

## Documentación

| Documento | Contenido |
|-----------|-----------|
| [docs/behavior.es.md](docs/behavior.es.md) | Contrato de comportamiento con .NET: correspondencia por función, códigos de retorno, JSON, codificación (el contrato de aceptación) |
| [docs/behavior.md](docs/behavior.md) | El mismo contrato, en inglés |
| [docs/pragmas.md](docs/pragmas.md) | PRAGMAs de conexion (WAL, synchronous, busy_timeout, foreign_keys, temp_store): que hace cada uno, su alcance y como aplicarlos al abrir |
| [README.md](README.md) | Este README en inglés |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Qué queda pendiente: Android/Flutter, iOS, WASM, FLL de FoxPro, release |
| [CHANGELOG.md](CHANGELOG.md) | Historial de versiones y cambios incompatibles |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Flujo de build/tests, reglas para ampliar la API congelada |
| [SECURITY.md](SECURITY.md) | Cómo reportar vulnerabilidades |

## Bindings para otros lenguajes

La API congelada está envuelta para otros lenguajes, con un ejemplo
canónico en cada uno (el mismo flujo en todos los lenguajes para poder
comparar resultados):

| Lenguaje | Carpeta |
|----------|---------|
| C (nativo) | [`src/sqlite/examples`](src/sqlite/examples) |
| Python | [`bindings/python`](bindings/python) |
| C# / .NET | [`bindings/csharp`](bindings/csharp) |
| C++ | [`bindings/cpp`](bindings/cpp) |
| Swift | [`bindings/swift`](bindings/swift) |
| Go | [`bindings/go`](bindings/go) |
| Rust | [`bindings/rust`](bindings/rust) |
| Node.js | [`bindings/nodejs`](bindings/nodejs) |
| Java | [`bindings/java`](bindings/java) |
| R | [`bindings/r`](bindings/r) |
| Ruby | [`bindings/ruby`](bindings/ruby) |
| Julia | [`bindings/julia`](bindings/julia) |
| Fortran | [`bindings/fortran`](bindings/fortran) |
| Kotlin | [`bindings/kotlin`](bindings/kotlin) |
| Visual FoxPro (legacy) | [`bindings/foxpro`](bindings/foxpro) |
| Clipper (legacy) | [`bindings/clipper`](bindings/clipper) |

Tabla completa, reglas (propiedad del alocador, callbacks, struct de
parámetros) y los lenguajes restantes:
**[bindings/README.md](bindings/README.md)**.

## Ejemplos

Hay cuatro programas validados en
[`src/sqlite/examples/`](src/sqlite/examples/). Se compilan por defecto y
se ejecutan como smoke tests de CTest (`ctest -R example`), así que la CI
los verifica en todas las plataformas:

| Archivo | Qué muestra |
|---------|-------------|
| [basic.c](src/sqlite/examples/basic.c) | Apertura, DDL, insert, callback del reader, filas impresas como JSON |
| [parameters.c](src/sqlite/examples/parameters.c) | Parámetros nombrados, `clean_prefix`, escalares, parámetros OUTPUT vía `RETURNING` |
| [transaction.c](src/sqlite/examples/transaction.c) | `begin` / `commit` / `rollback` y sus rutas de fallo |
| [json.c](src/sqlite/examples/json.c) | `execute_reader_to_json` e `insert_json` en ambos sentidos |

> Los builds compartidos de Windows exportan exactamente los 34 símbolos
> `sqlite_core_*` (`sqlite.def`): las llamadas directas al motor
> (`sqlite3_column_*`, ...) requieren enlace **estático**, y por eso los
> ejemplos se quedan en la API del wrapper.

## Compilar en un contenedor (Docker)

Build local sin instalar toolchains en el host
([`docker/Dockerfile`](docker/Dockerfile)):

```bash
# Construye la imagen y corre toda la suite CTest (tests + ejemplos).
docker build -f docker/Dockerfile -t libsqlite:local .

# Extrae los binarios de Linux (shared + static) cuando pase.
docker run --rm -v "$(pwd)/bin-local:/bin-local" \
    libsqlite:local bash -c "cp -r bin/* /bin-local/"
```

(PowerShell: `docker run --rm -v "${PWD}/bin-local:/bin-local" ...`.)

### Contenedores de los bindings

Cada binding verificado también corre en su propio contenedor Linux: la
imagen construye la biblioteca, corre la suite CTest completa y después
compila y ejecuta el ejemplo del lenguaje contra el `.so` real; la imagen
se elimina al final (`docker system df` no crece):

```bat
docker\build_all.cmd            :: python csharp cpp swift ruby julia kotlin fortran go rust nodejs java r
docker\build.cmd python         :: un solo lenguaje
```

FoxPro y Clipper quedan fuera: cargan el DLL de Windows desde un runtime
legacy de 16/32 bits y no tienen contenedor Linux. Matriz completa en
[`docker/readme.md`](docker/readme.md), reglas por lenguaje en
[`bindings/README.md`](bindings/README.md).

## Contribuir y seguridad

Los reportes de bugs y las peticiones de función usan las plantillas de
issues de GitHub (piden versión, plataforma, tipo de enlace y una
reproducción mínima), y los pull requests siguen la checklist de
[.github/PULL_REQUEST_TEMPLATE.md](.github/PULL_REQUEST_TEMPLATE.md).
Flujo de build/tests y reglas para ampliar la API:
[CONTRIBUTING.md](CONTRIBUTING.md). Las vulnerabilidades se reportan de
forma privada según [SECURITY.md](SECURITY.md).

## Contrato de comportamiento

El comportamiento observable de cada función está documentado como contrato
de aceptación en **[docs/behavior.es.md](docs/behavior.es.md)**: tabla de
códigos de retorno, correspondencia con .NET por función, reglas de JSON y
codificación. Las suites `regression` y `api` implementan cada regla de ese
documento.

## Estructura del repositorio

```
libsqlite/
├── src/sqlite/
│   ├── CMakeLists.txt           Opciones, versión, instalación/export
│   ├── cmake/                   Plantilla del paquete CMake
│   ├── core/
│   │   ├── sqlite_core.h        Cabecera paraguas pública — incluir esta
│   │   ├── sqlite_core_*.{c,h}  Módulos de la API (error, string, cnx,
│   │   │                        parameter, command, transaction, builder, field)
│   │   ├── compat/              Capa de portabilidad absorbida (SQLITE_CORE_API,
│   │   │                        detección de codificación, comparaciones)
│   │   ├── engine/              Amalgamación SQLite embebida (sin modificar)
│   │   └── sqlite.def           Lista de exportación congelada (Windows)
│   ├── tests/                   Suites CTest: regression, api, exports
│   └── examples/                Ejemplos de uso (planificados)
├── bin/<arch>-<os>/{shared,static}/<config>/   Salidas de build (ignoradas por git)
├── docs/behavior.es.md          Contrato de paridad .NET (Español)
├── .github/workflows/ci.yml     CI: matriz de build + ctest
├── LICENSE                      MIT
└── cmp_dll.bat                  Script de conveniencia: build compartido Win32
```

## Componentes de terceros

- **Motor SQLite** (`src/sqlite/core/engine/sqlite3.c/.h`): la amalgamación
  oficial, sin modificaciones, en **dominio público**
  (<https://sqlite.org/copyright.html>).
- **Capa de portabilidad** (`src/sqlite/core/compat/`): solo las piezas
  realmente usadas de las antiguas dependencias externas (core-c-sdk /
  libstring): la macro de exportación, la familia de detección de
  codificación y los comparadores insensibles a mayúsculas. Todo es C99/C11
  estándar más los shims de plataforma mínimos posibles, así que no queda
  ninguna dependencia en runtime.

## Licencia

MIT — ver [LICENSE](LICENSE). © 2026 Christian Gutman.

El motor SQLite embebido es de dominio público, como se indicó arriba.

---

**libsqlite** · v1.0.0 · [English](README.md) · [Español](README.es.md)
