#include "sqlite_core_command.h"

/* .NET parity: one statement per command (ADO.NET CommandText semantics).
 * Tracks single-quote state so semicolons inside string literals are ignored. */
static int sqlite_core_is_single_statement(const char *sql)
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

/* .NET parity: transaction control goes through BeginTransaction/Commit/
 * Rollback, never through plain command text. */
int sqlite_core_is_forbidden_transaction_text(const char *sql)
{
    if (sql == NULL)
        return 0;

    while (*sql == ' ' || *sql == '\t' || *sql == '\r' || *sql == '\n')
    {
        sql++;
    }

    if (sqlite_core_str_n_compare(sql, "BEGIN", 5) == 0)
        return 1;
    if (sqlite_core_str_n_compare(sql, "COMMIT", 6) == 0)
        return 1;
    if (sqlite_core_str_n_compare(sql, "ROLLBACK", 8) == 0)
        return 1;
    if (sqlite_core_str_n_compare(sql, "END", 3) == 0)
    {
        char next = *(sql + 3);
        if (next == '\0' || next == ' ' || next == '\t' || next == '\r' || next == '\n' || next == ';')
        {
            return 1;
        }
    }

    return 0;
}

/* Prepares the statement converting ANSI SQL to UTF-8 first (returns the
 * prepared statement; *must_free tells the caller whether to release it). */
static int PrepareUtf8Sql(
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

/* Copies an SQLite error message into the caller buffer (bounded). */
static void CopyErrorMessage(char *err_buf, int err_buf_max, const char *zErrMsg)
{
    if (zErrMsg == NULL || err_buf == NULL || err_buf_max <= 0)
        return;

    size_t len = strlen(zErrMsg);
    if ((int)len > err_buf_max - 1)
        len = (size_t)err_buf_max - 1;
    memcpy(err_buf, zErrMsg, len);
    err_buf[len] = '\0';
}

/* Raw execution of DDL/DML text (one or more statements allowed). */
int sqlite_core_execute_command(
    sqlite3 *db,
    const char *sql_command,
    char *err_buf,
    int err_buf_max)
{
    if (db == NULL || sql_command == NULL)
    {
        return SQLITE_MISUSE;
    }

    if (err_buf && err_buf_max > 0)
    {
        err_buf[0] = '\0';
    }

    char *zErrMsg = NULL;
    int rc = sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);

    if (rc != SQLITE_OK)
    {
        CopyErrorMessage(err_buf, err_buf_max, zErrMsg);
        if (zErrMsg != NULL)
        {
            sqlite3_free(zErrMsg);
        }
    }
    return rc;
}

/* Guarded transaction command execution. txexpected is the autocommit
 * state required before the command runs (1 = begin, 0 = commit/rollback). */
SQLITE_CORE_API int sqlite_core_execute_transaction(
    sqlite3 *db,
    const char *sql_command,
    int txexpected,
    char *err_buf,
    int err_buf_max)
{
    if (db == NULL || sql_command == NULL)
    {
        return SQLITE_MISUSE;
    }

    if (err_buf && err_buf_max > 0)
    {
        err_buf[0] = '\0';
    }

    int current_autocommit = sqlite3_get_autocommit(db);
    if (current_autocommit != txexpected)
    {
        if (err_buf && err_buf_max > 0)
        {
            if (txexpected == 1)
            {
                snprintf(err_buf, err_buf_max, "ERROR: A transaction is already active on this handle.");
            }
            else
            {
                snprintf(err_buf, err_buf_max, "ERROR: There is no active transaction to close.");
            }
        }
        return -1;
    }

    int rc = sqlite_core_execute_command(db, sql_command, err_buf, err_buf_max);
    return (rc == SQLITE_OK ? 1 : 0);
}

/* .NET ExecuteNonQuery parity: returns the number of affected rows, or the
 * SQLite error code on failure. */
int sqlite_core_execute_non_query(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    char *err_buf,
    int err_buf_max)
{
    if (db == NULL || sql == NULL)
    {
        return SQLITE_MISUSE;
    }

    if (err_buf && err_buf_max > 0)
    {
        err_buf[0] = '\0';
    }

    if (!sqlite_core_is_single_statement(sql))
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "ERROR CORE: Only one statement per execution is allowed.");
        return SQLITE_MISUSE;
    }

    if (sqlite_core_is_forbidden_transaction_text(sql))
    {
        if (err_buf && err_buf_max > 0)
        {
            snprintf(err_buf, err_buf_max, "ERROR CORE: Plain-text transaction statements are forbidden. Use sqlite_core_begin_transaction/Commit/Rollback.");
        }
        return SQLITE_MISUSE;
    }

    sqlite3_stmt *stmt = NULL;
    int must_free_sql = 0;
    int rc = PrepareUtf8Sql(db, sql, &stmt, &must_free_sql);
    if (rc != SQLITE_OK)
    {
        if (err_buf && err_buf_max > 0)
        {
            snprintf(err_buf, err_buf_max, "%s", sqlite3_errmsg(db));
        }
        return rc;
    }

    if (params && paramCount > 0)
    {
        sqlite_core_param_bind(stmt, params, paramCount);
    }

    rc = sqlite3_step(stmt);

    int rows_affected = -1;

    if (rc == SQLITE_DONE || rc == SQLITE_ROW)
    {
        if (params && paramCount > 0)
        {
            sqlite_core_param_process_outputs(stmt, params, paramCount);
        }
        rows_affected = sqlite3_changes(db);
    }
    else
    {
        if (err_buf && err_buf_max > 0)
        {
            snprintf(err_buf, err_buf_max, "%s", sqlite3_errmsg(db));
        }
    }
    sqlite3_finalize(stmt);
    return rows_affected;
}

/* .NET ExecuteScalar parity: first column of the first row, NULL when the
 * result set is empty. */
int sqlite_core_execute_scalar(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    sqlite_core_param_t *out_scalar_result,
    char *err_buf,
    int err_buf_max)
{
    if (db == NULL || sql == NULL)
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "ERROR: db/sql pointers are NULL.");
        return SQLITE_MISUSE;
    }

    if (err_buf && err_buf_max > 0)
        err_buf[0] = '\0';

    sqlite_core_param_t dummy_local;
    char buffer_descarte[256] = {0};
    if (out_scalar_result == NULL)
    {
        memset(&dummy_local, 0, sizeof(sqlite_core_param_t));
        out_scalar_result = &dummy_local;
        out_scalar_result->value.blob_or_text.ptr = buffer_descarte;
        out_scalar_result->value.blob_or_text.max_len = sizeof(buffer_descarte);
    }

    if (!sqlite_core_is_single_statement(sql))
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "ERROR CORE: Only one statement per execution is allowed.");
        return SQLITE_MISUSE;
    }

    if (sqlite_core_is_forbidden_transaction_text(sql))
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "ERROR CORE: Plain-text transaction statements are forbidden.");
        return SQLITE_MISUSE;
    }

    sqlite3_stmt *stmt = NULL;
    int must_free_sql = 0;
    int rc = PrepareUtf8Sql(db, sql, &stmt, &must_free_sql);
    if (rc != SQLITE_OK)
    {
        if (err_buf && err_buf_max > 0)
        {
            snprintf(err_buf, err_buf_max, "%s", sqlite3_errmsg(db));
        }
        return rc;
    }

    if (params && paramCount > 0)
    {
        sqlite_core_param_bind(stmt, params, paramCount);
    }

    rc = sqlite3_step(stmt);
    int final_rc = rc;

    if (rc == SQLITE_ROW)
    {
        int type = sqlite3_column_type(stmt, 0);
        switch (type)
        {
        case SQLITE_INTEGER:
            out_scalar_result->type = SQLITE_CORE_TYPE_INT;
            out_scalar_result->value.iVal = sqlite3_column_int64(stmt, 0);
            break;

        case SQLITE_FLOAT:
            out_scalar_result->type = SQLITE_CORE_TYPE_DOUBLE;
            out_scalar_result->value.dVal = sqlite3_column_double(stmt, 0);
            break;

        case SQLITE_TEXT:
        {
            const char *txt = (const char *)sqlite3_column_text(stmt, 0);
            int len = sqlite3_column_bytes(stmt, 0);
            out_scalar_result->type = SQLITE_CORE_TYPE_TEXT;

            if (txt != NULL)
            {
                const char *pSrcText = txt;
                int must_free_text = 0;

                if (sqlite_core_detect_encoding(txt) == ENCODING_UTF8)
                {
                    char *converted_ansi = sqlite_core_string_utf8_to_ansi(txt);
                    if (converted_ansi != NULL)
                    {
                        pSrcText = converted_ansi;
                        len = (int)strlen(converted_ansi);
                        must_free_text = 1;
                    }
                }

                if (out_scalar_result->value.blob_or_text.ptr != NULL &&
                    out_scalar_result->value.blob_or_text.max_len > len)
                {
                    memcpy(out_scalar_result->value.blob_or_text.ptr, pSrcText, len);
                    out_scalar_result->value.blob_or_text.ptr[len] = '\0';
                    out_scalar_result->value.blob_or_text.len = len;
                }

                if (must_free_text)
                {
                    sqlite_core_string_free((void *)pSrcText);
                }
            }
            break;
        }
        case SQLITE_NULL:
            out_scalar_result->type = SQLITE_CORE_TYPE_NULL;
            break;
        }
        final_rc = SQLITE_OK;
    }
    else if (rc == SQLITE_DONE)
    {
        /* Empty result set: .NET ExecuteScalar returns null */
        out_scalar_result->type = SQLITE_CORE_TYPE_NULL;
        final_rc = SQLITE_OK;
    }
    else
    {
        if (err_buf && err_buf_max > 0)
        {
            snprintf(err_buf, err_buf_max, "%s", sqlite3_errmsg(db));
        }
    }

    sqlite3_finalize(stmt);
    return final_rc;
}

/* .NET ExecuteReader parity: the callback is invoked once per row, and
 * once with hasrows == 0 when the result set is empty. */
int sqlite_core_execute_reader(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    sqlite_core_row_callback_fn reader_callback,
    void *pUserContext,
    char *err_buf,
    int err_buf_max)
{
    if (db == NULL || sql == NULL || reader_callback == NULL)
    {
        return SQLITE_MISUSE;
    }

    if (err_buf && err_buf_max > 0)
    {
        err_buf[0] = '\0';
    }

    if (!sqlite_core_is_single_statement(sql))
    {
        if (err_buf && err_buf_max > 0)
        {
            snprintf(err_buf, err_buf_max, "ERROR CORE: Only one statement per execution is allowed.");
        }
        return SQLITE_MISUSE;
    }

    if (sqlite_core_is_forbidden_transaction_text(sql))
    {
        if (err_buf && err_buf_max > 0)
        {
            snprintf(err_buf, err_buf_max, "ERROR CORE: Plain-text transaction statements are forbidden. Use sqlite_core_begin_transaction/Commit/Rollback.");
        }
        return SQLITE_MISUSE;
    }

    sqlite3_stmt *stmt = NULL;
    int must_free_sql = 0;
    int rc = PrepareUtf8Sql(db, sql, &stmt, &must_free_sql);
    if (rc != SQLITE_OK)
    {
        if (err_buf && err_buf_max > 0)
        {
            snprintf(err_buf, err_buf_max, "%s", sqlite3_errmsg(db));
        }
        return rc;
    }

    if (params != NULL && paramCount > 0)
    {
        sqlite_core_param_bind(stmt, params, paramCount);
    }

    int cols = sqlite3_column_count(stmt);
    int rows = 0;
    int call_abort = 0;
    int step_rc = SQLITE_OK;

    while ((step_rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        rows++;
        call_abort = reader_callback(stmt, cols, 1, pUserContext);
        if (call_abort != 0)
        {
            break;
        }
    }

    if (step_rc != SQLITE_DONE && step_rc != SQLITE_ROW)
    {
        if (err_buf && err_buf_max > 0)
        {
            snprintf(err_buf, err_buf_max, "%s", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
        return step_rc;
    }

    if (rows == 0 && call_abort == 0)
    {
        reader_callback(stmt, cols, 0, pUserContext);
    }

    sqlite3_finalize(stmt);
    return rows;
}