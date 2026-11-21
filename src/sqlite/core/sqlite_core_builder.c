#include "sqlite_core_builder.h"

/* SIZE_MAX lives in <stdint.h>; MSVC pulls it in transitively through other
 * headers, but glibc and macOS do not. */
#include <stdint.h>

/*
 * JSON output builder and JSON input, built on the SQLite allocator so that
 * every returned buffer can be released with sqlite_core_string_free().
 */

/* ------------------------------------------------------------------ */
/* Dynamic buffer helpers (sqlite_core_json_builder_t)                       */
/* ------------------------------------------------------------------ */

/* Ensures the buffer can hold `extra` more bytes plus the NUL terminator. */
static int JsonCtx_Ensure(sqlite_core_json_builder_t *ctx, size_t extra)
{
    if (ctx->has_error)
        return 0;

    if (extra > SIZE_MAX - ctx->current_len - 1)
    {
        ctx->has_error = 1;
        return 0;
    }

    size_t needed = ctx->current_len + extra;
    if (needed + 1 <= ctx->allocated_size)
        return 1;

    size_t new_size = (ctx->allocated_size > 0) ? ctx->allocated_size : 1024;
    while (new_size < needed + 1)
    {
        if (new_size > (SIZE_MAX / 2))
        {
            ctx->has_error = 1;
            return 0;
        }
        new_size *= 2;
    }

    char *ptr = (char *)sqlite3_realloc(ctx->string_ptr, new_size);
    if (ptr == NULL)
    {
        ctx->has_error = 1;
        return 0;
    }
    ctx->string_ptr = ptr;
    ctx->allocated_size = new_size;
    return 1;
}

/* Appends raw bytes. Sets has_error on out-of-memory. */
static void JsonCtx_Append(sqlite_core_json_builder_t *ctx, const char *data, size_t len)
{
    if (len == 0)
        return;
    if (!JsonCtx_Ensure(ctx, len))
        return;

    memcpy(ctx->string_ptr + ctx->current_len, data, len);
    ctx->current_len += len;
    ctx->string_ptr[ctx->current_len] = '\0';
}

/* Appends a string literal (compile-time constant). */
static void JsonCtx_AppendLit(sqlite_core_json_builder_t *ctx, const char *lit)
{
    JsonCtx_Append(ctx, lit, strlen(lit));
}

/* Appends a JSON string literal: RFC 8259 escaping, UTF-8 passthrough. */
static void JsonCtx_AppendJsonString(sqlite_core_json_builder_t *ctx, const char *text, size_t len)
{
    JsonCtx_AppendLit(ctx, "\"");

    for (size_t i = 0; i < len && !ctx->has_error; i++)
    {
        unsigned char ch = (unsigned char)text[i];
        switch (ch)
        {
        case '"':
            JsonCtx_AppendLit(ctx, "\\\"");
            break;
        case '\\':
            JsonCtx_AppendLit(ctx, "\\\\");
            break;
        case '\b':
            JsonCtx_AppendLit(ctx, "\\b");
            break;
        case '\f':
            JsonCtx_AppendLit(ctx, "\\f");
            break;
        case '\n':
            JsonCtx_AppendLit(ctx, "\\n");
            break;
        case '\r':
            JsonCtx_AppendLit(ctx, "\\r");
            break;
        case '\t':
            JsonCtx_AppendLit(ctx, "\\t");
            break;
        default:
            if (ch < 0x20)
            {
                char escape[8];
                snprintf(escape, sizeof(escape), "\\u%04x", ch);
                JsonCtx_AppendLit(ctx, escape);
            }
            else
            {
                JsonCtx_Append(ctx, (const char *)&ch, 1);
            }
            break;
        }
    }

    JsonCtx_AppendLit(ctx, "\"");
}

/* ------------------------------------------------------------------ */
/* JSON writer callback                                                */
/* ------------------------------------------------------------------ */

int sqlite_core_json_writer_callback(
    sqlite3_stmt *stmt,
    int columns,
    int hasrows,
    void *pUserContext)
{
    sqlite_core_json_builder_t *ctx = (sqlite_core_json_builder_t *)pUserContext;
    if (ctx == NULL || ctx->has_error)
    {
        return 1;
    }

    if (hasrows == 0)
    {
        return 0;
    }

    if (!JsonCtx_Ensure(ctx, 64))
    {
        ctx->has_error = 1;
        return 1;
    }

    if (ctx->rows_processed > 0)
    {
        JsonCtx_AppendLit(ctx, ",\n");
    }
    JsonCtx_AppendLit(ctx, "  {\n");

    for (int i = 0; i < columns && !ctx->has_error; i++)
    {
        if (i > 0)
        {
            JsonCtx_AppendLit(ctx, ",\n");
        }

        const char *col_name = sqlite3_column_name(stmt, i);
        size_t name_len = col_name ? strlen(col_name) : 0;
        JsonCtx_AppendJsonString(ctx, col_name ? col_name : "", name_len);
        JsonCtx_AppendLit(ctx, ": ");

        int col_type = sqlite3_column_type(stmt, i);
        switch (col_type)
        {
        case SQLITE_NULL:
            JsonCtx_AppendLit(ctx, "null");
            break;

        case SQLITE_INTEGER:
        {
            char num[32];
            snprintf(num, sizeof(num), "%lld", sqlite3_column_int64(stmt, i));
            JsonCtx_AppendLit(ctx, num);
            break;
        }

        case SQLITE_FLOAT:
        {
            /* %.17g keeps the value round-trippable */
            char num[64];
            snprintf(num, sizeof(num), "%.17g", sqlite3_column_double(stmt, i));
            JsonCtx_AppendLit(ctx, num);
            break;
        }

        default:
        {
            const char *val = (const char *)sqlite3_column_text(stmt, i);
            if (val == NULL)
            {
                JsonCtx_AppendLit(ctx, "null");
            }
            else
            {
                /* JSON is UTF-8 by definition: no ANSI conversion here */
                JsonCtx_AppendJsonString(ctx, val, strlen(val));
            }
            break;
        }
        }
    }

    if (!ctx->has_error)
    {
        JsonCtx_AppendLit(ctx, "\n  }");
        ctx->rows_processed++;
    }

    return ctx->has_error ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Reader -> JSON array driver                                         */
/* ------------------------------------------------------------------ */

int sqlite_core_execute_reader_to_json(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    char **out_json,
    char *err_buf,
    int err_buf_max)
{
    if (db == NULL || sql == NULL || out_json == NULL)
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "ERROR: db/sql/out_json pointers are NULL.");
        return SQLITE_MISUSE;
    }

    if (err_buf && err_buf_max > 0)
        err_buf[0] = '\0';
    *out_json = NULL;

    sqlite_core_json_builder_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    if (!JsonCtx_Ensure(&ctx, 2))
    {
        sqlite3_free(ctx.string_ptr);
        return SQLITE_NOMEM;
    }
    JsonCtx_AppendLit(&ctx, "[\n");

    int rows = sqlite_core_execute_reader(
        db, sql, params, paramCount,
        sqlite_core_json_writer_callback, &ctx,
        err_buf, err_buf_max);

    if (rows < 0 || ctx.has_error)
    {
        sqlite3_free(ctx.string_ptr);
        *out_json = NULL;
        return (rows < 0) ? rows : SQLITE_NOMEM;
    }

    if (ctx.rows_processed == 0)
    {
        sqlite3_free(ctx.string_ptr);
        *out_json = sqlite3_mprintf("[]");
        return (*out_json != NULL) ? 0 : SQLITE_NOMEM;
    }

    JsonCtx_AppendLit(&ctx, "\n]");
    if (ctx.has_error)
    {
        sqlite3_free(ctx.string_ptr);
        *out_json = NULL;
        return SQLITE_NOMEM;
    }

    *out_json = ctx.string_ptr; /* ownership transferred to the caller */
    return ctx.rows_processed;
}

/* ------------------------------------------------------------------ */
/* JSON array -> INSERT (SQLite JSON1 engine)                          */
/* ------------------------------------------------------------------ */

/* Simple growable list of SQLite-allocated strings. */
typedef struct
{
    char **items;
    int count;
    int cap;
} StrList;

static int StrList_Add(StrList *list, char *value)
{
    if (list->count >= list->cap)
    {
        int new_cap = (list->cap > 0) ? list->cap * 2 : 8;
        char **items = (char **)sqlite3_realloc(list->items, (size_t)new_cap * sizeof(char *));
        if (items == NULL)
        {
            sqlite3_free(value);
            return 0;
        }
        list->items = items;
        list->cap = new_cap;
    }
    list->items[list->count++] = value;
    return 1;
}

static void StrList_Free(StrList *list)
{
    for (int i = 0; i < list->count; i++)
        sqlite3_free(list->items[i]);
    sqlite3_free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

/* Quotes a value as a SQL identifier: "name" with internal quotes doubled.
 * Returns a SQLite-allocated string (release with sqlite3_free). */
static char *QuoteIdentifierSqlite(const char *name)
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

/* Quotes a key as a JSON path selector: $."key" with JSON-string escapes.
 * Returns a SQLite-allocated string (release with sqlite3_free). */
static char *QuoteKeyPath(const char *key)
{
    if (key == NULL)
        return NULL;

    size_t len = strlen(key);
    char *out = (char *)sqlite3_malloc(len * 2 + 6); /* worst case */
    if (out == NULL)
        return NULL;

    char *p = out;
    *p++ = '$';
    *p++ = '.';
    *p++ = '"';
    for (size_t i = 0; i < len; i++)
    {
        if (key[i] == '"' || key[i] == '\\')
        {
            *p++ = '\\';
        }
        *p++ = key[i];
    }
    *p++ = '"';
    *p = '\0';
    return out;
}

/* Validates that json_str is a JSON array and collects the column names of
 * its first object.
 * Returns SQLITE_OK (columns listed in *out_keys, may be empty for []),
 * SQLITE_MISUSE (not a valid JSON array) or a SQLite error code. */
static int CollectJsonKeys(
    sqlite3 *db,
    const char *json_str,
    StrList *out_keys,
    char *err_buf,
    int err_buf_max)
{
    sqlite3_stmt *chk = NULL;
    int rc = sqlite3_prepare_v2(db,
        "SELECT CASE WHEN json_valid(?1) AND json_type(?1) = 'array' THEN 1 ELSE 0 END",
        -1, &chk, NULL);
    if (rc != SQLITE_OK)
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "%s", sqlite3_errmsg(db));
        return rc;
    }
    sqlite3_bind_text(chk, 1, json_str, -1, SQLITE_TRANSIENT);
    int is_array = 0;
    if (sqlite3_step(chk) == SQLITE_ROW)
        is_array = sqlite3_column_int(chk, 0);
    sqlite3_finalize(chk);

    if (!is_array)
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "ERROR: json_str is not a valid JSON array.");
        return SQLITE_MISUSE;
    }

    sqlite3_stmt *keys = NULL;
    rc = sqlite3_prepare_v2(db, "SELECT key FROM json_each(?1, '$[0]')", -1, &keys, NULL);
    if (rc != SQLITE_OK)
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "%s", sqlite3_errmsg(db));
        return rc;
    }
    sqlite3_bind_text(keys, 1, json_str, -1, SQLITE_TRANSIENT);

    int step_rc;
    while ((step_rc = sqlite3_step(keys)) == SQLITE_ROW)
    {
        const char *key = (const char *)sqlite3_column_text(keys, 0);
        if (key == NULL)
            continue;
        char *copy = sqlite3_mprintf("%s", key);
        if (copy == NULL || !StrList_Add(out_keys, copy))
        {
            sqlite3_finalize(keys);
            return SQLITE_NOMEM;
        }
    }
    sqlite3_finalize(keys);

    if (step_rc != SQLITE_DONE)
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "%s", sqlite3_errmsg(db));
        return rc;
    }
    return SQLITE_OK;
}

/* Builds:
 *   INSERT INTO "table" ("c1","c2") SELECT json_extract(je.value,?2),
 *   json_extract(je.value,?3) FROM json_each(?1)
 * Returns a SQLite-allocated SQL string (release with sqlite3_free),
 * or NULL with rc set to SQLITE_NOMEM.
 *
 * The vendored engine header has no sqlite3_str family, so the statement is
 * assembled into an over-allocated buffer and copied piece by piece. */
static char *BuildJsonInsertSql(sqlite3 *db, const char *table_name, const StrList *keys, int *rc)
{
    (void)db; /* the statement is assembled without engine string builders */

    char *quoted_table = QuoteIdentifierSqlite(table_name);
    if (quoted_table == NULL)
    {
        *rc = SQLITE_NOMEM;
        return NULL;
    }

    /* Over-estimate the size: every part is bounded by these constants.
     * Per column: quoted key (key + 2, up to 2x with doubled quotes),
     * separator comma, and "json_extract(je.value, ?NNNN)" (<= 31). */
    size_t total = 61 + strlen(table_name) * 2 + 2;
    for (int i = 0; i < keys->count; i++)
        total += strlen(keys->items[i]) + 40;

    char *buf = (char *)sqlite3_malloc(total);
    if (buf == NULL)
    {
        sqlite3_free(quoted_table);
        *rc = SQLITE_NOMEM;
        return NULL;
    }

    int off = snprintf(buf, total, "INSERT INTO %s (", quoted_table);
    sqlite3_free(quoted_table);

    for (int i = 0; i < keys->count; i++)
    {
        char *quoted = QuoteIdentifierSqlite(keys->items[i]);
        if (quoted == NULL)
        {
            sqlite3_free(buf);
            *rc = SQLITE_NOMEM;
            return NULL;
        }
        if (i > 0)
            buf[off++] = ',';
        size_t qlen = strlen(quoted);
        memcpy(buf + off, quoted, qlen);
        off += (int)qlen;
        sqlite3_free(quoted);
    }

    off += snprintf(buf + off, total - (size_t)off, ") SELECT ");

    for (int i = 0; i < keys->count; i++)
    {
        if (i > 0)
            buf[off++] = ',';
        off += snprintf(buf + off, total - (size_t)off, "json_extract(je.value, ?%d)", i + 2);
    }

    off += snprintf(buf + off, total - (size_t)off, " FROM json_each(?1) AS je");
    buf[off] = '\0';

    *rc = SQLITE_OK;
    return buf;
}

int sqlite_core_insert_json(
    sqlite3 *db,
    const char *table_name,
    const char *json_str,
    char *err_buf,
    int err_buf_max)
{
    if (db == NULL || table_name == NULL || json_str == NULL)
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "ERROR: db/table_name/json_str pointers are NULL.");
        return SQLITE_MISUSE;
    }

    if (err_buf && err_buf_max > 0)
        err_buf[0] = '\0';

    StrList keys;
    memset(&keys, 0, sizeof(keys));

    int rc = CollectJsonKeys(db, json_str, &keys, err_buf, err_buf_max);
    if (rc != SQLITE_OK)
    {
        StrList_Free(&keys);
        return (rc > 0) ? -rc : rc; /* errors are negative by convention */
    }

    /* Empty array: zero rows inserted, nothing else to do */
    if (keys.count == 0)
    {
        StrList_Free(&keys);
        return 0;
    }

    /* Own the transaction only when none is active (.NET parity: joining an
     * ambient transaction leaves commit/rollback to the caller). */
    int started = 0;
    if (sqlite3_get_autocommit(db) == 1)
    {
        int btrc = sqlite_core_begin_transaction(db, err_buf, err_buf_max);
        if (btrc != 1)
        {
            StrList_Free(&keys);
            return -1; /* could not start the transaction */
        }
        started = 1;
    }

    int build_rc = SQLITE_OK;
    char *sql = BuildJsonInsertSql(db, table_name, &keys, &build_rc);
    if (sql == NULL)
    {
        StrList_Free(&keys);
        if (started)
            sqlite_core_rollback_transaction(db, NULL, 0);
        return (build_rc > 0) ? -build_rc : build_rc;
    }

    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_free(sql);

    if (rc != SQLITE_OK)
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "%s", sqlite3_errmsg(db));
        StrList_Free(&keys);
        if (started)
            sqlite_core_rollback_transaction(db, NULL, 0);
        return -rc;
    }

    sqlite3_bind_text(stmt, 1, json_str, -1, SQLITE_TRANSIENT);
    for (int i = 0; i < keys.count; i++)
    {
        char *path = QuoteKeyPath(keys.items[i]);
        if (path == NULL)
            continue;
        sqlite3_bind_text(stmt, i + 2, path, -1, SQLITE_TRANSIENT);
        sqlite3_free(path);
    }

    rc = sqlite3_step(stmt);
    int rows = (rc == SQLITE_DONE) ? sqlite3_changes(db) : -1;
    if (rc != SQLITE_DONE)
    {
        if (err_buf && err_buf_max > 0)
            snprintf(err_buf, err_buf_max, "%s", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    StrList_Free(&keys);

    if (rows < 0)
    {
        if (started)
            sqlite_core_rollback_transaction(db, NULL, 0);
        return (rc > 0) ? -rc : rc;
    }

    if (started)
    {
        int crc = sqlite_core_commit_transaction(db, err_buf, err_buf_max);
        if (crc != 1)
        {
            sqlite_core_rollback_transaction(db, NULL, 0);
            return -1;
        }
    }

    return rows;
}