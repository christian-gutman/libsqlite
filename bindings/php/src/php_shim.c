/* PHP shim for libsqlite3.
 *
 * Why this exists (also explained in bindings/php/README.md): the frozen
 * 34-symbol API is fully reachable from PHP through FFI, with exactly one
 * gap — PHP FFI has no callback mechanism at all (the complete method list
 * is cdef, load, scope, new, free, cast, type, typeof, arrayType, addr,
 * sizeof, alignof, memcpy, memcmp, memset, string, isNull; there is no
 * FFI::callback). Two frozen symbols need one:
 *
 *   - sqlite_core_execute_reader, whose reader_callback is a C function
 *     pointer, so a PHP closure cannot be passed;
 *   - sqlite_core_json_writer_callback, which is only useful *as* such a
 *     pointer, and its address cannot be taken through an FFI binding.
 *
 * This shim supplies both with a few dozen lines of plain C and nothing
 * else: it re-implements no behaviour and calls the frozen API itself,
 * exactly like the R shim (bindings/r/src/libsqlite_shim.c) and the Swift
 * trampoline. The JSON path mirrors what
 * sqlite_core_execute_reader_to_json produces, so the example can compare
 * both documents byte for byte.
 *
 * Build (from bindings/php):
 *   cc -shared -fPIC -I ../../src/sqlite/core src/php_shim.c \
 *      -L <libdir> -lsqlite3 -Wl,-rpath,<libdir> -o libsqlite_shim.so
 */

/* The header marks its own prototypes with SQLITE_CORE_API (dllexport on
 * Windows): pre-defining it empty makes this a plain consumer. */
#define SQLITE_CORE_API

#include "sqlite_core.h"

#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* 1. execute_reader: rows are counted through a C trampoline          */
/* ------------------------------------------------------------------ */

typedef struct {
    int rows;
    int columns;
} php_reader_ctx;

static int php_row_trampoline(sqlite3_stmt *stmt, int columns, int hasrows,
                              void *user)
{
    (void)stmt; /* the frozen surface has no engine column API to read */
    php_reader_ctx *ctx = (php_reader_ctx *)user;
    if (hasrows == 1) {
        ctx->rows++;
        ctx->columns = columns;
    }
    return 0; /* non-zero would abort the iteration */
}

/* sqlite_php_reader_rows(db, sql, &rows, &columns, err, err_max) -> rc
 * rc is the sqlite_core_execute_reader return code (>= 0) or a negative
 * SQLite code. rows/columns receive the trampoline's counts; on an empty
 * result set the callback is invoked once with hasrows == 0, so both stay
 * at 0 while rc is still a success. */
int sqlite_php_reader_rows(sqlite3 *db, const char *sql, int *out_rows,
                           int *out_columns, char *err_buf, int err_buf_max)
{
    php_reader_ctx ctx;
    ctx.rows = 0;
    ctx.columns = 0;

    int rc = sqlite_core_execute_reader(db, sql, NULL, 0, php_row_trampoline,
                                        &ctx, err_buf, err_buf_max);

    if (out_rows != NULL) {
        *out_rows = ctx.rows;
    }
    if (out_columns != NULL) {
        *out_columns = ctx.columns;
    }
    return rc;
}

/* ------------------------------------------------------------------ */
/* 2. reader driven by the exported JSON writer callback                */
/* ------------------------------------------------------------------ */

/* sqlite_php_reader_json(db, sql, &json, &rows, err, err_max) -> rc
 *
 * Drives sqlite_core_execute_reader with the frozen
 * sqlite_core_json_writer_callback plus a sqlite_core_json_builder_t
 * context, then wraps the accumulated rows with the same "[\n" ... "\n]"
 * (or "[]") protocol that sqlite_core_execute_reader_to_json uses. The
 * document arrives SQLite-allocated in *out_json: release it from PHP with
 * sqlite_core_string_free, never with the PHP allocator.
 *
 * The wrapping only uses frozen symbols (string_format / string_concat /
 * string_free): the engine's mprintf/realloc are not part of the export
 * list, so they would not link against the Windows DLL. */
int sqlite_php_reader_json(sqlite3 *db, const char *sql, char **out_json,
                           int *out_rows, char *err_buf, int err_buf_max)
{
    sqlite_core_json_builder_t builder;
    memset(&builder, 0, sizeof(builder));

    int inner = sqlite_core_execute_reader(db, sql, NULL, 0,
                                           sqlite_core_json_writer_callback,
                                           &builder, err_buf, err_buf_max);

    char *doc = NULL;
    int rows = 0;

    if (inner >= 0 && builder.has_error == 0) {
        if (builder.rows_processed == 0) {
            doc = sqlite_core_string_format("[]");
            rows = 0;
        } else {
            /* "[\n" + <rows> + "\n]" */
            char *with_close = sqlite_core_string_concat(
                builder.string_ptr == NULL ? "" : builder.string_ptr, "\n]");
            char *with_open = sqlite_core_string_concat("[\n", with_close);
            if (with_close != NULL) {
                sqlite_core_string_free(with_close);
            }
            if (builder.string_ptr != NULL) {
                sqlite_core_string_free(builder.string_ptr);
            }
            doc = with_open;
            rows = builder.rows_processed;
        }
    } else {
        if (builder.string_ptr != NULL) {
            sqlite_core_string_free(builder.string_ptr);
        }
        rows = inner < 0 ? inner : SQLITE_NOMEM;
    }

    if (out_json != NULL) {
        *out_json = doc;
    }
    if (out_rows != NULL) {
        *out_rows = rows;
    }
    return rows >= 0 ? SQLITE_OK : rows;
}

/* ------------------------------------------------------------------ */
/* 3. the address of the frozen JSON writer callback                    */
/* ------------------------------------------------------------------ */

/* Returns the address of sqlite_core_json_writer_callback for callers that
 * want to pass it themselves (a Zend extension, or a host with its own FFI
 * machinery). Kept for completeness: the two functions above already cover
 * the PHP case without any cross-scope function-pointer juggling.
 * On Windows the symbol comes from the import library, so this is the
 * address of the thunk that reaches libsqlite3.dll. */
void *sqlite_php_json_writer_callback(void)
{
    return (void *)(uintptr_t)sqlite_core_json_writer_callback;
}