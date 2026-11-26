#ifndef SQLITE_CORE_COMMAND_H
#define SQLITE_CORE_COMMAND_H

#include "sqlite_core.h" 

/**
 * @name Guards and the return-value-first rule
 *
 * `execute_non_query`, `execute_scalar` and `execute_reader` reject their
 * text before any statement is prepared when it is multi-statement or
 * carries plain-text transaction control (`BEGIN` / `COMMIT` / `ROLLBACK` /
 * `END`). Those rejections:
 *
 *   - return `SQLITE_MISUSE`, and
 *   - write the reason into `err_buf`,
 *
 * but the engine is never reached - so `sqlite_core_error_code()` still
 * reports `SQLITE_OK` and `sqlite_core_error_message()` returns
 * "not an error" at that moment.
 *
 * That combination is the trap this block exists to prevent: the failing
 * call and `err_buf` both carry a real error, while the two engine
 * accessors say nothing happened. See the precedence rule in
 * `sqlite_core_error.h` and the full table there.
 * @{
 */

int sqlite_core_is_forbidden_transaction_text(const char *sql);

/** Raw execution of DDL/DML text. One or more statements are allowed here,
 * unlike the single-statement functions below - which is why this one has no
 * such guard and no such asymmetry. */
SQLITE_CORE_API int sqlite_core_execute_command(
    sqlite3 *db,
    const char *sql_command,
    char *err_buf,
    int err_buf_max);

/** Runs a statement while a transaction is expected (or not). */
SQLITE_CORE_API int sqlite_core_execute_transaction(
    sqlite3 *db,
    const char *sql_command,
    int txexpected,
    char *err_buf,
    int err_buf_max);

/** Single statement only: multi-statement text is rejected with
 * `SQLITE_MISUSE` before the engine is reached (see the block above). */
SQLITE_CORE_API int sqlite_core_execute_non_query(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    char *err_buf,
    int err_buf_max);

/** Single statement only, same guard as `execute_non_query`.
 *
 * The caller must pre-allocate `out_scalar_result` with
 * `value.blob_or_text.ptr` and `max_len` for TEXT results; larger values are
 * truncated silently, and without a buffer only the type is reported. */
SQLITE_CORE_API int sqlite_core_execute_scalar(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    sqlite_core_param_t *out_scalar_result,
    char *err_buf,
    int err_buf_max);

/** Single statement only, same guard as `execute_non_query`.
 *
 * The callback is invoked once per row, and exactly once with
 * `hasrows == 0` when the result set is empty. Returning non-zero from the
 * callback aborts the iteration. */
SQLITE_CORE_API int sqlite_core_execute_reader(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    sqlite_core_row_callback_fn reader_callback,
    void *pUserContext,
    char *err_buf, int err_buf_max);

/** @} */

#endif // SQLITE_CORE_COMMAND_H