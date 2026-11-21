#include "sqlite_core.h"
#include "sqlite_core_internal.h"

/*
 * Shared internals: the pieces several modules reuse so behavior cannot
 * drift between entry points (see sqlite_core_internal.h). They are not
 * exported: they do not appear in sqlite.def.
 */

/* .NET parity: one statement per command (ADO.NET CommandText semantics).
 * Tracks single-quote state so semicolons inside string literals are ignored. */
int sqlite_core_is_single_statement(const char *sql)
{
    if (sql == NULL)
        return 0;

    int inside_quote = 0;
    const char *p = sql;

    while (*p)
    {
        if (*p == '\'')
        {
            inside_quote = !inside_quote;
            p++;
            continue;
        }

        if (!inside_quote && *p == ';')
        {
            const char *next = p + 1;
            while (*next && isspace((unsigned char)*next))
            {
                next++;
            }

            if (*next != '\0')
            {
                return 0;
            }
        }
        p++;
    }
    return 1;
}

/* Prepares the statement converting ANSI SQL to UTF-8 first (returns the
 * prepared statement; *must_free tells the caller whether to release it). */
int PrepareUtf8Sql(
    sqlite3 *db,
    const char *sql,
    sqlite3_stmt **out_stmt,
    int *must_free_sql)
{
    *must_free_sql = 0;
    *out_stmt = NULL;

    const char *final_sql = sql;
    char *converted_sql = sqlite_core_string_ansi_to_utf8(sql);
    if (converted_sql != NULL)
    {
        final_sql = converted_sql;
        *must_free_sql = 1;
    }

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, final_sql, -1, &stmt, NULL);

    if (*must_free_sql)
    {
        sqlite_core_string_free((void *)converted_sql);
    }

    *out_stmt = stmt;
    return rc;
}

/* Quotes a value as a SQL identifier: "name" with internal quotes doubled.
 * Returns a SQLite-allocated string (release with sqlite3_free). */
char *QuoteIdentifierSqlite(const char *name)
{
    if (name == NULL)
        return NULL;

    size_t len = strlen(name);
    char *out = (char *)sqlite3_malloc(len * 2 + 3);
    if (out == NULL)
        return NULL;

    char *p = out;
    *p++ = '"';
    for (size_t i = 0; i < len; i++)
    {
        if (name[i] == '"')
        {
            *p++ = '"';
            *p++ = '"';
        }
        else
        {
            *p++ = name[i];
        }
    }
    *p++ = '"';
    *p = '\0';
    return out;
}

/* Copies an engine error message into the caller buffer (bounded). */
void CopyErrorMessage(char *err_buf, int err_buf_max, const char *zErrMsg)
{
    if (zErrMsg == NULL || err_buf == NULL || err_buf_max <= 0)
        return;

    size_t len = strlen(zErrMsg);
    if ((int)len > err_buf_max - 1)
        len = (size_t)err_buf_max - 1;
    memcpy(err_buf, zErrMsg, len);
    err_buf[len] = '\0';
}

// Returns the library version (e.g. "1.0.0").
const char *sqlite_core_version()
{
    return SQLITE_CORE_VERSION_STR;
}

// Returns the version of the embedded SQLite engine (e.g. "3.54.1").
const char *sqlite_core_sqlite_version()
{
    return SQLITE_CORE_SQLITE_VERSION_STR;
}

   
