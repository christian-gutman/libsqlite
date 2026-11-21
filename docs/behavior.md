# Behavioral Contract — .NET Parity

libsqlite exposes an ADO.NET-style C API. **The observable behavior of every
function must match its .NET counterpart.** This document is the acceptance
contract for bug fixes and tests.

## Connection

| .NET (ADO.NET)               | libsqlite                          | Required behavior |
|------------------------------|-----------------------------------|-------------------|
| `SqlConnection.Open`         | `sqlite_core_open`               | Fails (`SQLITE_CANTOPEN`) if the file does not exist. |
| `Open` + create semantics    | `sqlite_core_open_or_create`     | Creates the file when missing. |
| `ReadOnly=True` conn. string | `sqlite_core_open_readonly`      | Fails if the file does not exist; writes fail. |
| `DbConnection.Close`         | `sqlite_core_close`              | Idempotent, safe on `NULL`, returns 1 on success. |
| connection state checks      | `sqlite_core_is_readonly`         | **`1` iff the database is read-only, `0` if writable, `-1` on error.** |
| `CommandTimeout`             | `sqlite_core_set_busy_timeout`     | Accepts 0..30000 ms, clamps out-of-range values. |

## Commands

| .NET (ADO.NET)               | libsqlite                          | Required behavior |
|------------------------------|-----------------------------------|-------------------|
| `ExecuteReader`              | `sqlite_core_execute_reader`      | Invokes the callback once per row; when the result set is empty the callback is invoked exactly once with `hasrows == 0` (equivalent to `HasRows == false`). Returns the number of rows processed; aborting from the callback stops iteration. |
| `ExecuteNonQuery`            | `sqlite_core_execute_non_query`    | Returns rows affected; on error fills `err_buf` and returns the SQLite error code. |
| `ExecuteScalar`              | `sqlite_core_execute_scalar`      | First column of the first row; **`SQLITE_CORE_TYPE_NULL` when there are no rows** (matches .NET `null`). |
| one statement per command    | `sqlite_core_is_single_statement`  | `ExecuteNonQuery`/`ExecuteScalar`/`ExecuteReader` reject multi-statement text with `SQLITE_MISUSE`. |
| transaction control          | `sqlite_core_begin_transaction` / `Commit` / `Rollback` | `Begin` uses `BEGIN IMMEDIATE`; nested `Begin` fails; `Commit`/`Rollback` without an active transaction fail. |
| no transactions in text      | `sqlite_core_is_forbidden_transaction_text` | `ExecuteNonQuery`/`ExecuteScalar`/`ExecuteReader` reject `BEGIN`/`COMMIT`/`ROLLBACK`/`END` in the command text. |

## Parameters

- Naming follows the .NET pattern (`:name`, `@name`, `?name`); `sqlite_core_param_clean_prefix`
  strips the sigil so callers may bind by bare name.
- `Direction` mirrors `ParameterDirection`: `Input`, `Output`,
  `InputOutput`. Output parameters are populated from result columns whose
  name matches the parameter name (SQLite has no native output parameters;
  this is the documented emulation).
- Binding is by name, never by ordinal: a parameter missing from the SQL is
  simply not bound, matching .NET behavior.
- Text parameters are converted to UTF-8 before binding (ANSI input is
  detected, not assumed); the value stored in the parameter struct is never
  mutated by the library.
- Integer values are 64-bit end to end (`value.iVal` is `sqlite3_int64`),
  matching .NET `long`.
- **TEXT results in `ExecuteScalar`**: the caller must provide
  `value.blob_or_text.ptr` and `max_len`; the value is copied there (values
  larger than `max_len - 1` are silently truncated). Without a buffer the
  TEXT value is dropped and only the type is reported.

## JSON

- `sqlite_core_execute_reader_to_json` returns a UTF-8 JSON array of objects (RFC 8259
  escaped) and `[]` for an empty result set; the string is heap-allocated by
  the SQLite allocator and released with `sqlite_core_string_free`.
- `sqlite_core_insert_json` parses server-side via SQLite JSON1 (nested documents,
  escaped strings and numeric formats handled natively), inserts inside a
  transaction it owns only when no transaction is active, and returns the
  number of inserted rows or a **negative** SQLite error code.
- Table and column identifiers are quoted as SQLite identifiers; extraction
  paths are bound parameters, so key names can never alter the statement.

## Encoding detection

- `sqlite_core_detect_encoding` returns `ENCODING_UNKNOWN` for empty or NULL input,
  ASCII/UTF-8/ANSI otherwise. Conversion helpers accept that input safely.

## Errors

Unified contract for every public function:

| Return value | Meaning |
|--------------|---------|
| `>= 0` | Success. For value functions (`execute_non_query`, `execute_scalar`, `execute_reader`, `insert_json`) it is the affected/returned row count; for flag functions (`close`, `begin_transaction`, ...) it is `1` for success. |
| `0` | Boolean "false" result (`is_readonly` on a writable database, `set_busy_timeout` failure). |
| `-1` | Generic error (transaction could not be started/committed). Details in `err_buf`. |
| `-2` | `SQLITE_CORE_BUFFER_TOO_SMALL`: destination buffer too small (`get_blob`). |
| negative SQLite code | Engine failure (e.g. `-SQLITE_CONSTRAINT`); mirrors `SqlException.Number` negated. |
| positive SQLite code | `execute_*` functions keep the native SQLite result codes (`SQLITE_MISUSE`, `SQLITE_CANTOPEN`, ...) for compatibility with engine conventions. |

- Every function that accepts `err_buf` clears it on entry and fills it with
  the SQLite message on failure.
- `error_code`, `error_code_extended` and `error_message` mirror
  `SqlException.Number` / `Message`.

## Naming

The public API is `sqlite_core_*` (snake_case), types are
`sqlite_core_*_t`. Every function declared with `SQLITE_CORE_API` in the
public headers must appear in `sqlite.def`; the `exports` CTest verifies
this at runtime against the shared library.

## Formatting and encoding

- JSON output is UTF-8 and escapes `"`, `\` and control characters
  (RFC 8259). Numeric formatting is culture-invariant.
- SQL text passed to the API is UTF-8; ANSI input is converted transparently
  on every `Execute*` entry point (Windows only; Linux/macOS are UTF-8
  native and the string is passed through).

## Status of this contract

Every rule above becomes a test in the test suite (milestone 4). Known
violations are listed in the audit report and are fixed before the 1.0 tag.