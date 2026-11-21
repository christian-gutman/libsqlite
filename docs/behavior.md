# Behavioral Contract — .NET Parity

libsqlite3 exposes an ADO.NET-style C API. **The observable behavior of every
function must match its .NET counterpart.** This document is the acceptance
contract for bug fixes and tests.

## Connection

| .NET (ADO.NET)               | libsqlite3                        | Required behavior |
|------------------------------|-----------------------------------|-------------------|
| `SqlConnection.Open`         | `SqlLite_Core_Open`               | Fails (`SQLITE_CANTOPEN`) if the file does not exist. |
| `Open` + create semantics    | `SqlLite_Core_Open_Or_Create`     | Creates the file when missing. |
| `ReadOnly=True` conn. string | `SqlLite_Core_Open_ReadOnly`      | Fails if the file does not exist; writes fail. |
| `DbConnection.Close`         | `SqlLite_Core_Close`              | Idempotent, safe on `NULL`, returns 1 on success. |
| connection state checks      | `SqlLite_Core_IsReadonly`         | **`1` iff the database is read-only, `0` if writable, `-1` on error.** |
| `CommandTimeout`             | `SqlLite_Core_SetBusyTimeout`     | Accepts 0..30000 ms, clamps out-of-range values. |

## Commands

| .NET (ADO.NET)               | libsqlite3                        | Required behavior |
|------------------------------|-----------------------------------|-------------------|
| `ExecuteReader`              | `SqlLite_Core_ExecuteReader`      | Invokes the callback once per row; when the result set is empty the callback is invoked exactly once with `hasrows == 0` (equivalent to `HasRows == false`). Returns the number of rows processed; aborting from the callback stops iteration. |
| `ExecuteNonQuery`            | `SqlLite_Core_ExecuteNonQuery`    | Returns rows affected; on error fills `err_buf` and returns the SQLite error code. |
| `ExecuteScalar`              | `SqlLite_Core_ExecuteScalar`      | First column of the first row; **`SQLITE_CORE_TYPE_NULL` when there are no rows** (matches .NET `null`). |
| one statement per command    | `SqlLite_Core_IsSingleStatement`  | `ExecuteNonQuery`/`ExecuteScalar`/`ExecuteReader` reject multi-statement text with `SQLITE_MISUSE`. |
| transaction control          | `SqlLite_Core_BeginTransaction` / `Commit` / `Rollback` | `Begin` uses `BEGIN IMMEDIATE`; nested `Begin` fails; `Commit`/`Rollback` without an active transaction fail. |
| no transactions in text      | `SqlLite_Core_IsForbiddenTransactionalText` | `ExecuteNonQuery`/`ExecuteScalar`/`ExecuteReader` reject `BEGIN`/`COMMIT`/`ROLLBACK`/`END` in the command text. |

## Parameters

- Naming follows the .NET pattern (`:name`, `@name`, `?name`); `CleanPrefix`
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

## Errors

- Every function that accepts `err_buf` clears it on entry and fills it with
  the SQLite message on failure.
- Return codes are the native SQLite result codes (`SQLITE_OK`,
  `SQLITE_MISUSE`, ...); `1`/`0` are reserved for boolean-ish functions.
- `GetLastErrorCode`, `GetLastErrorCodeExtended` and `GetLastErrorMsg`
  mirror `SqlException.Number` / `Message`.

## Formatting and encoding

- JSON output is UTF-8 and escapes `"`, `\` and control characters
  (RFC 8259). Numeric formatting is culture-invariant.
- SQL text passed to the API is UTF-8; ANSI input is converted transparently
  on every `Execute*` entry point (currently inconsistent — see milestone 2).

## Status of this contract

Every rule above becomes a test in the test suite (milestone 4). Known
violations are listed in the audit report and are fixed before the 1.0 tag.