#ifndef SQLITE_CORE_INTERNAL_H
#define SQLITE_CORE_INTERNAL_H

/*
 * Internal helpers shared by the implementation modules.
 *
 * NOT part of the public API: these symbols are absent from sqlite.def,
 * so they are invisible to consumers of the shared library. They exist
 * so several modules can share the exact same preprocessing rules
 * (ANSI -> UTF-8 SQL conversion, one-statement guard) without
 * duplicating the logic.
 */

#include "sqlite_core.h"
#include "sqlite_core_string.h"

#ifdef __cplusplus
extern "C" {
#endif

/* .NET parity: a command runs exactly one statement. Tracks quoting so
 * semicolons inside string literals are ignored. */
int sqlite_core_is_single_statement(const char *sql);

/* Prepares a statement, converting ANSI SQL to UTF-8 first. When the
 * converted text differs, *must_free_sql is 1 and the caller releases
 * the buffer with sqlite_core_string_free(). Never touches err buffers;
 * the SQLite return code is passed through. */
int PrepareUtf8Sql(
    sqlite3 *db,
    const char *sql,
    sqlite3_stmt **out_stmt,
    int *must_free_sql);

/* Quotes a value as a SQL identifier: "name" with internal quotes
 * doubled. Returns a SQLite-allocated string (release with
 * sqlite3_free), or NULL on bad input or out of memory. */
char *QuoteIdentifierSqlite(const char *name);

/* Copies an engine error message into a caller buffer (bounded). */
void CopyErrorMessage(char *err_buf, int err_buf_max, const char *zErrMsg);

#ifdef __cplusplus
}
#endif

#endif /* SQLITE_CORE_INTERNAL_H */