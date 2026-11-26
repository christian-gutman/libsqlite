#ifndef SQLITE_CORE_ERROR_H
#define SQLITE_CORE_ERROR_H

#include "sqlite_core.h"

/**
 * @name Error diagnostics (engine-level)
 *
 * All three accessors describe **the engine's last statement**, not the
 * library's last call. They wrap `sqlite3_errcode` / `sqlite3_errmsg`
 * directly, so they can only see failures the engine actually produced.
 *
 * A library guard can reject before any statement is prepared:
 * multi-statement text, forbidden transaction text, or NULL arguments.
 * Those paths return `SQLITE_MISUSE` and write their reason to `err_buf`,
 * but they never reach the engine - so at that exact moment:
 *
 * | Channel                       | Value                                            |
 * |-------------------------------|--------------------------------------------------|
 * | return value of the call      | `SQLITE_MISUSE` (21) - that is a failure         |
 * | `err_buf`                     | "ERROR CORE: Only one statement per execution..." |
 * | `sqlite_core_error_code()`    | `SQLITE_OK` (0) - the engine saw nothing         |
 * | `sqlite_core_error_message()` | "not an error" - **a non-empty string**          |
 *
 * That last row is a trap: because "not an error" is non-empty, testing
 * `strlen(error_message) > 0` is NOT a way to detect a failure. It passes
 * even when nothing went wrong on the engine.
 *
 * @par The precedence rule
 * Check the **return value first** - it alone decides success or failure,
 * for every function in this API. The two accessors add engine detail only
 * after a failure the engine actually produced. Never decide "did it fail?"
 * from `error_code` alone: a guard failure returns non-zero with
 * `error_code == SQLITE_OK`.
 *
 * `err_buf` is the channel that carries library-level reasons; the engine
 * accessors cannot see them. Three bindings (C#, R, Node.js) asserted
 * `error_code != 0` after a multi-statement rejection and failed until each
 * provoked a real engine error first - see `bindings/CONTRACT.md`.
 * @{
 */

/** Last result code on the connection (`SqlException.Number`).
 * Mirrors the engine: `SQLITE_OK` when the engine's last statement
 * succeeded, or when the last call never reached the engine at all. */
SQLITE_CORE_API int sqlite_core_error_code(sqlite3 *db);

/** Last extended result code (the `SQLITE_IOERR_*` / `*_READONLY` variants
 * of `sqlite_core_error_code`). Same engine-level scope as that function. */
SQLITE_CORE_API int sqlite_core_error_code_extended(sqlite3 *db);

/** Last error message on the connection (`SqlException.Message`).
 *
 * Returns the engine's message. After a successful statement - or after a
 * call that never reached the engine - it returns the literal string
 * "not an error", which is non-empty: do not treat "non-empty" as
 * "something failed". The message is owned by the library's static data:
 * copy it, never free it. */
SQLITE_CORE_API const char* sqlite_core_error_message(sqlite3 *db);

/** @} */

#endif // SQLITE_CORE_ERROR_H