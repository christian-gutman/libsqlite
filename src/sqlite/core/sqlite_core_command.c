#include "sqlite_core_command.h"

static inline int SqlLite_Core_IsSingleStatement(const char *sql)
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

static inline int SqlLite_Core_IsValidScalarCommand(const char *sql)
{
    if (!SqlLite_Core_IsSingleStatement(sql))
    {
        return 0;
    }

    if (str_n_compare(sql, "SELECT", 6) == 0 && (isspace((unsigned char)sql[6]) || sql[6] == '\0'))
    {
        return 1;
    }

    if (str_n_compare(sql, "INSERT", 6) == 0 && (isspace((unsigned char)sql[6])))
    {
        const char *p = sql + 6;
        int inside_quote = 0;

        while (*p)
        {
            if (*p == '\'')
            {
                inside_quote = !inside_quote;
                p++;
                continue;
            }

            if (!inside_quote)
            {
                if (str_n_compare(p, "RETURNING", 9) == 0)
                {
                    char prev = *(p - 1);
                    char next = *(p + 9);

                    if (isspace((unsigned char)prev) && (isspace((unsigned char)next) || next == ';' || next == '\0'))
                    {
                        return 1;
                    }
                }
            }
            p++;
        }
    }

    return 0;
}

int SqlLite_Core_IsForbiddenTransactionalText(const char *sql)
{
    if (sql == NULL)
        return 0;
    //
    while (*sql == ' ' || *sql == '\t' || *sql == '\r' || *sql == '\n')
    {
        sql++;
    }
    //
    if (str_n_compare(sql, "BEGIN", 5) == 0)
        return 1;
    if (str_n_compare(sql, "COMMIT", 6) == 0)
        return 1;
    if (str_n_compare(sql, "ROLLBACK", 8) == 0)
        return 1;
    if (str_n_compare(sql, "END", 3) == 0)
    {
        char next = *(sql + 3);
        if (next == '\0' || next == ' ' || next == '\t' || next == '\r' || next == '\n' || next == ';')
        {
            return 1;
        }
    }
    //
    return 0;
}

int SqlLite_Core_ExecuteCommand(
    sqlite3 *db,
    const char *sql_command,
    char *err_buf,
    int err_buf_max)
{
    //
    if (db == NULL || sql_command == NULL)
    {
        return SQLITE_MISUSE;
    }
    //
    if (err_buf && err_buf_max > 0)
    {
        err_buf[0] = '\0';
    }
    /*
    if (!SqlLite_Core_IsSingleStatement(sql_command))
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "ERROR CORE: Solo se permite una instruccion por ejecucion.");

        return SQLITE_MISUSE;
    }
        BEGIN EXCLUSIVE TRANSACTION;
        Cmd
        COMMIT;
    */
    //
    char *zErrMsg = NULL;
    //
    int rc = sqlite3_exec(db, sql_command, NULL, NULL, &zErrMsg);
    //
    if (rc != SQLITE_OK)
    {
        //
        if (zErrMsg != NULL && err_buf != NULL && err_buf_max > 0)
        {
            strncpy(err_buf, zErrMsg, err_buf_max - 1);
            err_buf[err_buf_max - 1] = '\0';
        }
        //
        if (zErrMsg != NULL)
        {
            sqlite3_free(zErrMsg);
        }
    }
    return rc;
}

CORE_API int SqlLite_Core_ExecuteTransaction(
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

    int current_autocommit = sqlite3_get_autocommit(db);
    if (current_autocommit != txexpected)
    {
        if (err_buf && err_buf_max > 0)
        {
            if (txexpected == 1)
            {
                snprintf(err_buf, err_buf_max, "ERROR: Ya existe una transacción activa en este handle.");
            }
            else
            {
                snprintf(err_buf, err_buf_max, "ERROR: No hay ninguna transacción activa que cerrar.");
            }
        }
        return -1;
    }
    int rc = SqlLite_Core_ExecuteCommand(db, sql_command, err_buf, err_buf_max);
    return (rc == SQLITE_OK ? 1 : 0);
}

int SqlLite_Core_ExecuteNonQuery(
    sqlite3 *db,
    const char *sql,
    SqlLiteCoreParam *params,
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

    if (!SqlLite_Core_IsSingleStatement(sql))
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "ERROR CORE: Solo se permite una instruccion por ejecucion.");

        return SQLITE_MISUSE;
    }

    if (SqlLite_Core_IsForbiddenTransactionalText(sql))
    {
        if (err_buf && err_buf_max > 0)
        {
            snprintf(err_buf, err_buf_max, "ERROR CORE: Operaciones transaccionales por texto plano estan prohibidas. Use SqlLite_Core_BeginTransaction/Commit/Rollback.");
        }
        return SQLITE_MISUSE;
    }

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
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
        SqlLite_Core_BindParameters(stmt, params, paramCount);
    }

    rc = sqlite3_step(stmt);

    int rows_affected = -1;

    if (rc == SQLITE_DONE || rc == SQLITE_ROW)
    {
        if (params && paramCount > 0)
        {
            SqlLite_Core_ProcessOutputParameters(stmt, params, paramCount);
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

int SqlLite_Core_ExecuteScalar(
    sqlite3 *db,
    const char *sql,
    SqlLiteCoreParam *params,
    int paramCount,
    SqlLiteCoreParam *out_scalar_result,
    char *err_buf,
    int err_buf_max)
{
    if (db == NULL || sql == NULL)
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "ERROR: db/sql Punteros nulos.");
        return SQLITE_MISUSE;
    }

    if (err_buf && err_buf_max > 0)
        err_buf[0] = '\0';

    SqlLiteCoreParam dummy_local;
    char buffer_descarte[256] = {0};
    if (out_scalar_result == NULL)
    {
        memset(&dummy_local, 0, sizeof(SqlLiteCoreParam));
        out_scalar_result = &dummy_local;
        out_scalar_result->value.blob_or_text.ptr = buffer_descarte;
        out_scalar_result->value.blob_or_text.max_len = sizeof(buffer_descarte);
    }

    if (!SqlLite_Core_IsSingleStatement(sql))
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "ERROR CORE: Solo se permite una instruccion por ejecucion.");

        return SQLITE_MISUSE;
    }

    if (SqlLite_Core_IsForbiddenTransactionalText(sql))
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "ERROR CORE: Operaciones transaccionales por texto plano estan prohibidas.");
        return SQLITE_MISUSE;
    }

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "%s", sqlite3_errmsg(db));
        return rc;
    }

    if (params && paramCount > 0)
    {
        SqlLite_Core_BindParameters(stmt, params, paramCount);
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
            out_scalar_result->value.iVal = sqlite3_column_int(stmt, 0);
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

                if (StrDetectEncoding(txt) == ENCODING_UTF8)
                {
                    char *converted_ansi = SqlLite_Core_UTF8ToAnsi(txt);
                    if (converted_ansi != NULL)
                    {
                        pSrcText = converted_ansi;
                        len = (int)strlen(converted_ansi);
                        must_free_text = 1;
                    }
                }

                if (out_scalar_result->value.blob_or_text.ptr &&
                    out_scalar_result->value.blob_or_text.max_len > len)
                {
                    memcpy(out_scalar_result->value.blob_or_text.ptr, pSrcText, len);
                    out_scalar_result->value.blob_or_text.ptr[len] = '\0';
                    out_scalar_result->value.blob_or_text.len = len;
                }

                if (must_free_text)
                {
                    SqlLite_Core_FreeString((void *)pSrcText);
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
        out_scalar_result->type = SQLITE_CORE_TYPE_NULL;
        final_rc = SQLITE_OK;
    }
    else
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "%s", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return final_rc;
}

int SqlLite_Core_ExecuteReader(
    sqlite3 *db,
    const char *sql,
    SqlLiteCoreParam *params,
    int paramCount,
    SqlLiteCoreRowCallback reader_callback,
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

    if (!SqlLite_Core_IsSingleStatement(sql))
    {
        if (err_buf && err_buf_max > 0)
        {
            snprintf(err_buf, err_buf_max, "ERROR CORE: Solo se permite una instruccion por ejecucion.");
        }
        return SQLITE_MISUSE;
    }

    if (SqlLite_Core_IsForbiddenTransactionalText(sql))
    {
        if (err_buf && err_buf_max > 0)
        {
            snprintf(err_buf, err_buf_max, "ERROR CORE: Operaciones transaccionales por texto plano estan prohibidas. Use SqlLite_Core_BeginTransaction/Commit/Rollback.");
        }
        return SQLITE_MISUSE;
    }

    const char *pFinalSql = sql;
    int must_free_sql = 0;

    char *converted_sql = SqlLite_Core_AnsiToUTF8(sql);
    if (converted_sql != NULL)
    {
        pFinalSql = converted_sql;
        must_free_sql = 1;
    }

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, pFinalSql, -1, &stmt, NULL);

    if (must_free_sql)
    {
        SqlLite_Core_FreeString((void *)converted_sql);
    }

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
        SqlLite_Core_BindParameters(stmt, params, paramCount);
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
