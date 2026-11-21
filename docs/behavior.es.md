# Contrato de Comportamiento — Paridad .NET

libsqlite expone una API C con semántica ADO.NET. **El comportamiento
observable de cada función debe coincidir con su equivalente en .NET.** Este
documento es el contrato de aceptación para las correcciones y los tests.

## Conexiones

| .NET (ADO.NET)               | libsqlite                          | Comportamiento exigido |
|------------------------------|-----------------------------------|------------------------|
| `SqlConnection.Open`         | `sqlite_core_open`               | Falla (`SQLITE_CANTOPEN`) si el archivo no existe. |
| `Open` + semántica de crear  | `sqlite_core_open_or_create`     | Crea el archivo cuando no existe. |
| Cadena con `ReadOnly=True`   | `sqlite_core_open_readonly`      | Falla si el archivo no existe; las escrituras fallan. |
| `DbConnection.Close`         | `sqlite_core_close`              | Idempotente, seguro con `NULL`, devuelve 1 en éxito. |
| Comprobación de estado       | `sqlite_core_is_readonly`         | **`1` si la base es solo lectura, `0` si es escribible, `-1` en error.** |
| `CommandTimeout`             | `sqlite_core_set_busy_timeout`     | Acepta 0..30000 ms, saturando valores fuera de rango. |

## Comandos

| .NET (ADO.NET)               | libsqlite                          | Comportamiento exigido |
|------------------------------|-----------------------------------|------------------------|
| `ExecuteReader`              | `sqlite_core_execute_reader`      | Invoca el callback una vez por fila; cuando el conjunto de resultados está vacío el callback se invoca exactamente una vez con `hasrows == 0` (equivalente a `HasRows == false`). Devuelve el número de filas procesadas; abortar desde el callback detiene la iteración. |
| `ExecuteNonQuery`            | `sqlite_core_execute_non_query`    | Devuelve las filas afectadas; en error rellena `err_buf` y devuelve el código de error de SQLite. |
| `ExecuteScalar`              | `sqlite_core_execute_scalar`      | Primera columna de la primera fila; **`SQLITE_CORE_TYPE_NULL` cuando no hay filas** (equivale al `null` de .NET). |
| Una sentencia por comando    | `sqlite_core_is_single_statement`  | `ExecuteNonQuery`/`ExecuteScalar`/`ExecuteReader` rechazan texto multi-sentencia con `SQLITE_MISUSE`. |
| Control de transacciones     | `sqlite_core_begin_transaction` / `Commit` / `Rollback` | `Begin` usa `BEGIN IMMEDIATE`; los `Begin` anidados fallan; `Commit`/`Rollback` sin transacción activa fallan. |
| Sin transacciones en el texto | `sqlite_core_is_forbidden_transaction_text` | `ExecuteNonQuery`/`ExecuteScalar`/`ExecuteReader` rechazan `BEGIN`/`COMMIT`/`ROLLBACK`/`END` en el texto del comando. |

## Parámetros

- Los nombres siguen el patrón .NET (`:name`, `@name`, `?name`);
  `sqlite_core_param_clean_prefix` quita el sigilo para poder enlazar por
  nombre simple.
- `Direction` refleja `ParameterDirection`: `Input`, `Output`,
  `InputOutput`. Los parámetros de salida se rellenan desde las columnas del
  resultado cuyo nombre coincide con el del parámetro (SQLite no tiene
  parámetros de salida nativos; esta es la emulación documentada).
- El enlace es por nombre, nunca por ordinal: un parámetro ausente del SQL
  simplemente no se enlaza, igual que en .NET.
- Los parámetros de texto se convierten a UTF-8 antes de enlazar (la entrada
  ANSI se detecta, no se asume); el valor guardado en la estructura del
  parámetro nunca es modificado por la biblioteca.
- Los enteros son de 64 bits de punta a punta (`value.iVal` es
  `sqlite3_int64`), equivalente al `long` de .NET.
- **Resultados TEXT en `ExecuteScalar`**: quien llama debe proporcionar
  `value.blob_or_text.ptr` y `max_len`; el valor se copia allí (valores
  mayores que `max_len - 1` se truncan de forma silenciosa). Sin buffer, el
  valor TEXT se descarta y solo se reporta el tipo.

## JSON

- `sqlite_core_execute_reader_to_json` devuelve un array JSON de objetos en
  UTF-8 (escapado RFC 8259) y `[]` para un conjunto vacío; la cadena se
  reserva con el asignador de SQLite y se libera con
  `sqlite_core_string_free`.
- `sqlite_core_insert_json` parsea del lado del motor mediante SQLite JSON1
  (documentos anidados, cadenas escapadas y formatos numéricos se manejan de
  forma nativa), inserta dentro de una transacción de su propiedad solo cuando
  no hay transacción activa, y devuelve el número de filas insertadas o un
  **código de error de SQLite negativo**.
- Los identificadores de tabla y columna se entrecomillan como identificadores
  de SQLite; las rutas de extracción se enlazan como parámetros, así que los
  nombres de clave nunca pueden alterar la sentencia.

## Escritura de BLOBs

- `sqlite_core_insert_blob` / `sqlite_core_update_blob` escriben un
  **fichero** en una columna BLOB con la API incremental de blobs, por
  chunks (64 KiB por defecto, configurable; memoria constante sin
  importar el tamaño del fichero).
- `insert_blob` con `rowid == 0` inserta una fila nueva dimensionada con
  `zeroblob()`; con un `rowid` distinto de cero es un upsert (actualiza la
  fila si existe, la crea con ese `rowid` si no).
- `update_blob` requiere una fila existente y nunca agranda la columna en
  silencio: un fichero mayor que el blob guardado falla con
  `SQLITE_CORE_BLOB_TOO_SMALL` (-3).
- Valor devuelto: número de bytes escritos (`>= 0`); `SQLITE_MISUSE` con
  argumentos inválidos; `SQLITE_CANTOPEN` si el fichero no se puede abrir
  o leer; en otro caso, el código del motor con el mensaje en `err_buf`.

## Detección de codificación

- `sqlite_core_detect_encoding` devuelve `ENCODING_UNKNOWN` para entrada
  vacía o `NULL`; ASCII/UTF-8/ANSI en caso contrario. Los helpers de
  conversión aceptan esa entrada de forma segura.

## Errores

Contrato unificado para todas las funciones públicas:

| Valor devuelto | Significado |
|----------------|-------------|
| `>= 0` | Éxito. En funciones con valor (`execute_non_query`, `execute_scalar`, `execute_reader`, `insert_json`) es el número de filas afectadas/devueltas; en funciones de bandera (`close`, `begin_transaction`, ...) es `1` en éxito. |
| `0` | Resultado booleano "falso" (`is_readonly` sobre una base escribible, fallo de `set_busy_timeout`). |
| `-1` | Error genérico (no se pudo iniciar/confirmar la transacción). Detalles en `err_buf`. |
| `-2` | `SQLITE_CORE_BUFFER_TOO_SMALL`: buffer de destino demasiado pequeño (`get_blob`). |
| `-3` | `SQLITE_CORE_BLOB_TOO_SMALL`: el blob existente de la columna es más pequeño que el fichero que se está escribiendo (`update_blob` y el upsert de `insert_blob`). |
| Código SQLite negativo | Fallo del motor (p. ej. `-SQLITE_CONSTRAINT`); refleja `SqlException.Number` negado. |
| Código SQLite positivo | Las funciones `execute_*` conservan los códigos nativos de resultado de SQLite (`SQLITE_MISUSE`, `SQLITE_CANTOPEN`, ...) por convención del motor. |

- Toda función que acepte `err_buf` lo limpia al entrar y lo rellena con el
  mensaje de SQLite en caso de fallo.
- `error_code`, `error_code_extended` y `error_message` reflejan
  `SqlException.Number` / `Message`.

## Nomenclatura

La API pública es `sqlite_core_*` (snake_case), los tipos son
`sqlite_core_*_t`. Toda función declarada con `SQLITE_CORE_API` en las
cabeceras públicas debe aparecer en `sqlite.def`; el CTest `exports` lo
verifica en runtime contra la biblioteca compartida.

## Formato y codificación

- La salida JSON es UTF-8 y escapa `"`, `\` y los caracteres de control
  (RFC 8259). El formato numérico es invariante de cultura.
- El texto SQL que se pasa a la API es UTF-8; la entrada ANSI se convierte de
  forma transparente en todos los puntos de entrada `Execute*` (solo en
  Windows; Linux/macOS son UTF-8 nativos y la cadena se pasa sin cambios).

## Estado de este contrato

Cada regla anterior se convierte en un test de la suite de pruebas (hito 4).
Las infracciones conocidas se listan en el informe de auditoría y
se corrigen antes del tag 1.0.