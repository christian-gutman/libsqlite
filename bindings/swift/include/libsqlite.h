/* C module that Swift imports (bindings/swift/module.modulemap).
 *
 * It declares the frozen 34-symbol surface of libsqlite3
 * (src/sqlite/core/sqlite.def) so the binding is self-contained: Swift
 * gets the prototypes, sqlite_core_param_t (with its union) and the row
 * callback type, and libsqlite3 is linked with -l:libsqlite3.so.
 *
 * Nothing here is implemented: the definitions live in the library.
 */

#ifndef LIBSQLITE_SWIFT_MODULE_H
#define LIBSQLITE_SWIFT_MODULE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
typedef long long sqlite3_int64;

#define SQLITE_OK 0
#define SQLITE_ERROR 1
#define SQLITE_MISUSE 21
#define SQLITE_CANTOPEN 14

#define SQLITE_CORE_BUFFER_TOO_SMALL (-2)
#define SQLITE_CORE_BLOB_TOO_SMALL (-3)

#define SQLITE_CORE_TYPE_NULL 0
#define SQLITE_CORE_TYPE_INT 2
#define SQLITE_CORE_TYPE_DOUBLE 4
#define SQLITE_CORE_TYPE_TEXT 6
#define SQLITE_CORE_TYPE_BLOB 8

#define SQLITE_CORE_DIRECTION_INPUT 1
#define SQLITE_CORE_DIRECTION_OUTPUT 2
#define SQLITE_CORE_DIRECTION_INPUTOUTPUT 3

typedef union sqlite_core_param_value {
    double dVal;
    sqlite3_int64 iVal;
    struct {
        char *ptr;
        int len;
        int max_len;
    } blob_or_text;
} sqlite_core_param_value;

typedef struct sqlite_core_param_t {
    const char *name;
    int no_insensitive;
    int type;
    int Direction;
    sqlite_core_param_value value;
} sqlite_core_param_t;

typedef int (*sqlite_core_row_callback_fn)(
    sqlite3_stmt *stmt,
    int columns,
    int hasrows,
    void *pUserContext);

/* --- the 34 exported symbols (src/sqlite/core/sqlite.def) ------------- */

const char *sqlite_core_version(void);

const char *sqlite_core_sqlite_version(void);

int sqlite_core_error_code(sqlite3 *db);

int sqlite_core_error_code_extended(sqlite3 *db);

const char *sqlite_core_error_message(sqlite3 *db);

char *sqlite_core_string_format(const char *format, ...);

char *sqlite_core_string_concat(const char *str1, const char *str2);

void sqlite_core_string_free(const char *input);

char *sqlite_core_string_utf8_to_ansi(const char *utf8_str);

char *sqlite_core_string_ansi_to_utf8(const char *ansi_str);

sqlite3 *sqlite_core_open_ex(const char *uripath, int flags, int *out_rc);

sqlite3 *sqlite_core_open_readonly(const char *uripath, int *out_rc);

sqlite3 *sqlite_core_open_or_create(const char *uripath, int *out_rc);

sqlite3 *sqlite_core_open(const char *uripath, int *out_rc);

sqlite3 *sqlite_core_open_uri_readonly(const char *uripath, int *out_rc);

int sqlite_core_close(sqlite3 *db);

int sqlite_core_is_readonly(sqlite3 *db);

sqlite3_int64 sqlite_core_last_insert_rowid(sqlite3 *db);

int sqlite_core_set_busy_timeout(sqlite3 *db, int ms);

const char *sqlite_core_param_clean_prefix(const char *name);

int sqlite_core_execute_command(
    sqlite3 *db,
    const char *sql_command,
    char *err_buf,
    int err_buf_max);

int sqlite_core_execute_transaction(
    sqlite3 *db,
    const char *sql_command,
    int txexpected,
    char *err_buf,
    int err_buf_max);

int sqlite_core_execute_non_query(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    char *err_buf,
    int err_buf_max);

int sqlite_core_execute_scalar(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    sqlite_core_param_t *out_scalar_result,
    char *err_buf,
    int err_buf_max);

int sqlite_core_execute_reader(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    sqlite_core_row_callback_fn reader_callback,
    void *pUserContext,
    char *err_buf,
    int err_buf_max);

int sqlite_core_begin_transaction(sqlite3 *db, char *err_buf, int err_buf_max);

int sqlite_core_commit_transaction(sqlite3 *db, char *err_buf, int err_buf_max);

int sqlite_core_rollback_transaction(sqlite3 *db, char *err_buf, int err_buf_max);

int sqlite_core_json_writer_callback(
    sqlite3_stmt *stmt,
    int columns,
    int hasrows,
    void *pUserContext);

int sqlite_core_execute_reader_to_json(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    char **out_json,
    char *err_buf,
    int err_buf_max);

int sqlite_core_insert_json(
    sqlite3 *db,
    const char *table_name,
    const char *json_str,
    char *err_buf,
    int err_buf_max);

int sqlite_core_get_blob(
    sqlite3 *db,
    const char *sql,
    void *pDestBuffer,
    int maxLen,
    int *pOutCopied);

int sqlite_core_insert_blob(
    sqlite3 *db,
    const char *table,
    const char *column,
    sqlite3_int64 rowid,
    const char *file_path,
    int chunk_size,
    int *p_out_written,
    char *err_buf,
    int err_buf_max);

int sqlite_core_update_blob(
    sqlite3 *db,
    const char *table,
    const char *column,
    sqlite3_int64 rowid,
    const char *file_path,
    int chunk_size,
    int *p_out_written,
    char *err_buf,
    int err_buf_max);

/* Swift cannot form a C function pointer from a closure that captures
 * context, so sqlite_core_execute_reader is driven from this counting
 * trampoline (the same pattern the R binding uses): it fills one struct with
 * the visited rows and the column count of the last one. Callers that need
 * per-row data use sqlite_core_execute_reader_to_json instead. */
typedef struct libsqlite3_row_stats {
    int rows;
    int columns;
} libsqlite3_row_stats;

static inline int libsqlite3_row_counter(sqlite3_stmt *stmt, int columns,
                                         int hasrows, void *ctx)
{
    libsqlite3_row_stats *stats = (libsqlite3_row_stats *)ctx;
    (void)stmt;
    if (stats != NULL && hasrows == 1) {
        stats->rows++;
        stats->columns = columns;
    }
    return 0;
}

/* Mirror of sqlite_core_json_builder_t (src/sqlite/core/sqlite_core_builder.h):
 * the context that sqlite_core_json_writer_callback fills, used by the Swift
 * readerJsonCallback() path. Its string_ptr is owned by the SQLite allocator
 * and must be released with sqlite_core_string_free. */
typedef struct libsqlite3_json_builder {
    char *string_ptr;
    size_t allocated_size;
    size_t current_len;
    int rows_processed;
    int has_error;
} libsqlite3_json_builder_t;

#ifdef __cplusplus
}
#endif

#endif /* LIBSQLITE_SWIFT_MODULE_H */