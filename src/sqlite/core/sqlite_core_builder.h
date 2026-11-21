#ifndef SQLITE_CORE_BUILDER_H
#define SQLITE_CORE_BUILDER_H

#include "sqlite_core.h"

/**
 * @struct sqlite_core_json_builder_t
 * @brief Opaque memory context for the dynamic JSON string builder.
 *
 * Holds the heap buffer that accumulates the JSON output. It travels through
 * the generic 'pUserContext' parameter required by the
 * sqlite_core_row_callback_fn signature.
 */
typedef struct {
    char *string_ptr;         /**< Base pointer to the accumulated JSON buffer (heap). */
    size_t allocated_size;    /**< Total capacity in bytes currently allocated. */
    size_t current_len;       /**< Current string length, excluding the NUL terminator. */
    int rows_processed;       /**< Number of rows successfully processed. */
    int has_error;            /**< Error flag: set to 1 on out-of-memory. */
} sqlite_core_json_builder_t;


/**
 * @brief Specialized callback that builds a JSON array from a result set.
 *        Matches the sqlite_core_row_callback_fn signature.
 *
 * Values are emitted as UTF-8 and properly escaped (RFC 8259): quotes,
 * backslashes and control characters are escaped. NULL becomes JSON null.
 * Integers use full 64-bit precision; doubles use %.17g (round-trip safe).
 *
 * @param[in] stmt          Active SQLite statement positioned on a row.
 * @param[in] columns       Number of columns in the result set.
 * @param[in] hasrows       1 when a valid row is current, 0 for an empty set.
 * @param[in] pUserContext  Pointer to the caller's sqlite_core_json_builder_t.
 *
 * @return 0 to continue with the next row, 1 to abort (out of memory).
 */
SQLITE_CORE_API int sqlite_core_json_writer_callback(
    sqlite3_stmt *stmt,
    int columns,
    int hasrows,
    void *pUserContext);


/**
 * @brief Executes a query and returns its result set as a JSON array.
 *
 * The produced document is a JSON array of objects:
 *   [\n  {"col1": 1, "col2": "text"},\n  {"col1": 2, "col2": null}\n]
 * An empty result set produces "[]". Column names come from the result set
 * metadata; values are UTF-8 and RFC 8259 escaped.
 *
 * @param[in]  db            Open database handle. Must not be NULL.
 * @param[in]  sql           Query text (single statement, ANSI or UTF-8).
 * @param[in]  params        Optional named parameters (may be NULL).
 * @param[in]  paramCount    Number of entries in params.
 * @param[out] out_json      Receives the heap-allocated JSON string
 *                           (release with sqlite_core_string_free).
 *                           Set to NULL on failure.
 * @param[out] err_buf       Optional error message buffer.
 * @param[in]  err_buf_max   Capacity of err_buf.
 *
 * @return Number of rows (>= 0) on success, or a SQLite error code.
 */
SQLITE_CORE_API int sqlite_core_execute_reader_to_json(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    char **out_json,
    char *err_buf,
    int err_buf_max);


/**
 * @brief Parses a JSON array of objects and inserts them into a table.
 *
 * The JSON array is parsed by the embedded SQLite JSON1 engine, so nested
 * structures, escaped strings and numeric formats are handled natively:
 *   [{"col1": "value", "col2": 42}, ...]
 *
 * Column names are taken from the first object. Values are bound through
 * prepared-statement placeholders (no string concatenation of values).
 * Identifiers (table and column names) are quoted as SQLite identifiers;
 * they never alter the statement structure.
 *
 * When no transaction is active the whole batch runs inside one
 * BEGIN IMMEDIATE ... COMMIT; any failure rolls the batch back. When a
 * transaction is already active the batch joins it and the caller keeps
 * control of commit/rollback.
 *
 * @param[in]  db           Open database handle. Must not be NULL.
 * @param[in]  table_name   Destination table (quoted as an identifier).
 * @param[in]  json_str     JSON array of objects.
 * @param[out] err_buf      Optional error message buffer.
 * @param[in]  err_buf_max  Capacity of err_buf.
 *
 * @return Number of inserted rows (>= 0), or a negative SQLite error code
 *         (-SQLITE_MISUSE for invalid input, -1 when the transaction
 *         cannot be started or committed).
 */
SQLITE_CORE_API int sqlite_core_insert_json(
    sqlite3 *db,
    const char *table_name,
    const char *json_str,
    char *err_buf,
    int err_buf_max);


#endif // SQLITE_CORE_BUILDER_H