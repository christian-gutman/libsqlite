/* R shim for libsqlite3.
 *
 * Why this exists (also explained in bindings/r/README.md): the frozen
 * 34-symbol API cannot be reached from pure R:
 *
 *   - sqlite_core_open*() returns the sqlite3* handle, and R's .C()
 *     discards the C return value, so the handle would be unreachable;
 *   - sqlite_core_param_t (sqlite_core_execute_non_query / _scalar /
 *     _reader) is a struct with a union that contains *native pointers*
 *     (name, blob_or_text.ptr) - R cannot allocate or read those;
 *   - sqlite3_int64 (rowid, last_insert_rowid) has no R representation
 *     in .C();
 *   - sqlite_core_execute_reader needs a C function pointer, and an R
 *     closure cannot be converted into one without compiled code.
 *
 * The shim therefore wraps every call with R-friendly marshalling and
 * calls the frozen API itself: no behaviour is re-implemented, and the
 * 34 symbols stay the single surface being tested.
 *
 * Build:  R CMD SHLIB -o libsqlite_shim.so src/libsqlite_shim.c
 *         (PKG_CPPFLAGS=-I<repo>/src/sqlite/core
 *          PKG_LIBS="-L<libdir> -lsqlite3 -Wl,-rpath,<libdir>")
 */

/* The header marks its own prototypes with SQLITE_CORE_API (dllexport on
 * Windows): pre-defining it empty makes this a plain consumer. */
#define SQLITE_CORE_API

#include "sqlite_core.h"

#include <R.h>
#include <Rinternals.h>
#include <R_ext/Rdynload.h>

#include <string.h>
#include <stdlib.h>

#define R_ERR_MAX 512

/* ------------------------------------------------------------------ */
/* helpers                                                             */
/* ------------------------------------------------------------------ */

static void *handle_of(SEXP db)
{
    if (TYPEOF(db) != EXTPTRSXP) {
        Rf_error("libsqlite3: expected a connection handle (external pointer)");
    }
    void *addr = R_ExternalPtrAddr(db);
    if (addr == NULL) {
        Rf_error("libsqlite3: the connection handle is closed");
    }
    return addr;
}

static const char *text_of(SEXP x)
{
    if (TYPEOF(x) != STRSXP || LENGTH(x) < 1) {
        Rf_error("libsqlite3: expected a character string");
    }
    return CHAR(STRING_ELT(x, 0));
}

static int int_of(SEXP x)
{
    return (int)asInteger(x);
}

static double real_of(SEXP x)
{
    return asReal(x);
}

/* Builds a result list from (name, value) pairs; caller must UNPROTECT. */
static SEXP result_list(int n, const char **names, SEXP *values)
{
    SEXP out = PROTECT(allocVector(VECSXP, n));
    SEXP nm = PROTECT(allocVector(STRSXP, n));
    for (int i = 0; i < n; i++) {
        SET_VECTOR_ELT(out, i, values[i]);
        SET_STRING_ELT(nm, i, mkChar(names[i]));
    }
    setAttrib(out, R_NamesSymbol, nm);
    UNPROTECT(1);
    return out;
}

static SEXP err_string(const char *buf)
{
    size_t n = 0;
    while (buf != NULL && buf[n] != '\0' && n < R_ERR_MAX) {
        n++;
    }
    if (n == 0) {
        return mkString("");
    }
    char *copy = (char *)R_alloc(n + 1, 1);
    memcpy(copy, buf, n);
    copy[n] = '\0';
    return mkChar(copy) == NA_STRING ? mkString("") : mkString(copy);
}

static SEXP scalar_int(int v)
{
    return ScalarInteger(v);
}

/* ------------------------------------------------------------------ */
/* connection lifetime                                                 */
/* ------------------------------------------------------------------ */

static void db_finalizer(SEXP ptr)
{
    if (TYPEOF(ptr) != EXTPTRSXP) {
        return;
    }
    sqlite3 *db = (sqlite3 *)R_ExternalPtrAddr(ptr);
    if (db != NULL) {
        sqlite_core_close(db);
        R_SetExternalPtrAddr(ptr, NULL);
    }
}

/* sqlite_r_open(path, flags, mode)
 * mode: 0 open, 1 open_ex(flags), 2 readonly, 3 open_or_create, 4 uri_readonly */
SEXP sqlite_r_open(SEXP path, SEXP flags, SEXP mode)
{
    const char *p = text_of(path);
    int m = int_of(mode);
    int fl = int_of(flags);
    int rc = 0;
    sqlite3 *db = NULL;

    switch (m) {
    case 1: db = sqlite_core_open_ex(p, fl, &rc); break;
    case 2: db = sqlite_core_open_readonly(p, &rc); break;
    case 3: db = sqlite_core_open_or_create(p, &rc); break;
    case 4: db = sqlite_core_open_uri_readonly(p, &rc); break;
    default: db = sqlite_core_open(p, &rc); break;
    }

    const char *names[2] = {"db", "rc"};
    SEXP values[2];
    values[1] = PROTECT(scalar_int(rc));
    if (db != NULL) {
        SEXP h = PROTECT(R_MakeExternalPtr((void *)db, install("libsqlite3"),
                                           R_NilValue));
        R_RegisterCFinalizerEx(h, db_finalizer, TRUE);
        values[0] = h;
        UNPROTECT(1);
    } else {
        values[0] = PROTECT(R_NilValue);
        UNPROTECT(1);
    }
    SEXP out = PROTECT(result_list(2, names, values));
    UNPROTECT(2);
    return out;
}

/* sqlite_r_close(db) -> integer (1 closed, 0 nothing to do) */
SEXP sqlite_r_close(SEXP db)
{
    sqlite3 *h = (sqlite3 *)R_ExternalPtrAddr(db);
    int rc = 0;
    if (h != NULL) {
        rc = sqlite_core_close(h);
        R_SetExternalPtrAddr(db, NULL);
    }
    return scalar_int(rc);
}

/* sqlite_r_connection(db, op, arg) -> integer/real
 * op: 0 is_readonly, 1 last_insert_rowid, 2 set_busy_timeout(arg ms),
 *     3 error_code, 4 error_code_extended, 5 error message */
SEXP sqlite_r_connection(SEXP db, SEXP op, SEXP arg)
{
    void *h = handle_of(db);
    int o = int_of(op);
    switch (o) {
    case 0: return scalar_int(sqlite_core_is_readonly(h));
    case 1: return ScalarReal((double)sqlite_core_last_insert_rowid(h));
    case 2: return scalar_int(sqlite_core_set_busy_timeout(h, int_of(arg)));
    case 3: return scalar_int(sqlite_core_error_code(h));
    case 4: return scalar_int(sqlite_core_error_code_extended(h));
    default: {
        const char *msg = sqlite_core_error_message(h);
        return msg == NULL ? mkString("") : mkString(msg);
    }
    }
}

/* sqlite_r_message(which) -> character
 * which: 0 library version, 1 sqlite engine version (static memory) */
SEXP sqlite_r_message(SEXP which)
{
    const char *m = int_of(which) == 0 ? sqlite_core_version()
                                       : sqlite_core_sqlite_version();
    return m == NULL ? mkString("") : mkString(m);
}

/* sqlite_r_string(op, a, b) -> list(free=TRUE handled, value)
 * op: 0 string_format(fmt = a, arg = b), 1 concat, 2 ansi_to_utf8,
 *     3 utf8_to_ansi. Every result is SQLite-allocated and freed here. */
SEXP sqlite_r_string(SEXP op, SEXP a, SEXP b)
{
    int o = int_of(op);
    const char *va = text_of(a);   /* format (op 0) or left operand */
    const char *vb = text_of(b);   /* vararg (op 0) or right operand */
    char *out = NULL;

    switch (o) {
    case 0: out = sqlite_core_string_format(va, vb); break;
    case 1: out = sqlite_core_string_concat(va, vb); break;
    case 2: out = sqlite_core_string_ansi_to_utf8(va); break;
    default: out = sqlite_core_string_utf8_to_ansi(va); break;
    }

    const char *names[1] = {"value"};
    SEXP values[1];
    if (out != NULL) {
        values[0] = PROTECT(mkString(out));
        sqlite_core_string_free(out);   /* SQLite allocator, not the JVM */
    } else {
        values[0] = PROTECT(R_NilValue);
    }
    SEXP res = PROTECT(result_list(1, names, values));
    UNPROTECT(2);
    return res;
}

/* sqlite_r_param_clean_prefix(name) -> character (static memory) */
SEXP sqlite_r_param_clean_prefix(SEXP name)
{
    const char *m = sqlite_core_param_clean_prefix(text_of(name));
    return m == NULL ? mkString("") : mkString(m);
}

/* ------------------------------------------------------------------ */
/* parameters                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    sqlite_core_param_t *items;
    int count;
    void **buffers;     /* malloc'd text buffers to release after the call */
} param_array;

static void params_free(param_array *p)
{
    if (p->buffers != NULL) {
        for (int i = 0; i < p->count; i++) {
            if (p->buffers[i] != NULL) {
                free(p->buffers[i]);
            }
        }
        free(p->buffers);
    }
    free(p->items);
    p->items = NULL;
    p->buffers = NULL;
    p->count = 0;
}

/* params: NULL, or a list of list(name=, type=, value=) entries.
 * type is one of "null", "int", "double", "text". */
static void params_build(SEXP params, param_array *out)
{
    out->items = NULL;
    out->buffers = NULL;
    out->count = 0;
    if (params == R_NilValue) {
        return;
    }
    if (TYPEOF(params) != VECSXP) {
        Rf_error("libsqlite3: params must be a list of parameter definitions");
    }
    int n = LENGTH(params);
    if (n == 0) {
        return;
    }

    out->items = (sqlite_core_param_t *)calloc((size_t)n, sizeof(sqlite_core_param_t));
    out->buffers = (void **)calloc((size_t)n, sizeof(void *));
    if (out->items == NULL || out->buffers == NULL) {
        params_free(out);
        Rf_error("libsqlite3: out of memory building parameters");
    }
    out->count = n;

    for (int i = 0; i < n; i++) {
        SEXP spec = VECTOR_ELT(params, i);
        if (TYPEOF(spec) != VECSXP || LENGTH(spec) < 3) {
            params_free(out);
            Rf_error("libsqlite3: each parameter needs list(name, type, value)");
        }
        sqlite_core_param_t *p = &out->items[i];
        p->name = text_of(VECTOR_ELT(spec, 0));
        p->no_insensitive = 0;
        p->Direction = SQLITE_CORE_DIRECTION_INPUT;
        const char *t = text_of(VECTOR_ELT(spec, 1));
        SEXP v = VECTOR_ELT(spec, 2);

        if (strcmp(t, "int") == 0) {
            p->type = SQLITE_CORE_TYPE_INT;
            p->value.iVal = (sqlite3_int64)real_of(v);
        } else if (strcmp(t, "double") == 0) {
            p->type = SQLITE_CORE_TYPE_DOUBLE;
            p->value.dVal = real_of(v);
        } else if (strcmp(t, "text") == 0) {
            const char *s = text_of(v);
            size_t len = strlen(s);
            char *buf = (char *)malloc(len + 1);
            if (buf == NULL) {
                params_free(out);
                Rf_error("libsqlite3: out of memory copying a text parameter");
            }
            memcpy(buf, s, len + 1);
            out->buffers[i] = buf;
            p->type = SQLITE_CORE_TYPE_TEXT;
            p->value.blob_or_text.ptr = buf;
            p->value.blob_or_text.len = -1;      /* NUL-terminated utf-8 */
            p->value.blob_or_text.max_len = 0;
        } else if (strcmp(t, "null") == 0) {
            p->type = SQLITE_CORE_TYPE_NULL;
        } else {
            params_free(out);
            Rf_error("libsqlite3: unknown parameter type (null/int/double/text)");
        }
    }
}

/* ------------------------------------------------------------------ */
/* reader trampoline (rows are counted: R cannot hold a C callback)     */
/* ------------------------------------------------------------------ */

typedef struct {
    int rows;
    int columns;
} reader_ctx;

static int row_trampoline(sqlite3_stmt *stmt, int columns, int hasrows,
                          void *user)
{
    (void)stmt;
    reader_ctx *ctx = (reader_ctx *)user;
    if (hasrows == 1) {
        ctx->rows++;
        ctx->columns = columns;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* execute_*                                                           */
/* ------------------------------------------------------------------ */

/* sqlite_r_execute(db, sql, kind, params, txexpected) -> list
 * kind: 0 command, 1 transaction, 2 non_query, 3 scalar, 4 reader.
 * The list always carries rc/rows/value/type/err:
 *   - scalar: value = the first column (numeric, character or NULL on
 *     NULL), type = SQLITE_CORE_TYPE_*;
 *   - reader: rows = number of visited rows;
 *   - command/non_query/transaction: rows = the function's return code
 *     (rows affected, or 1/0 for the transaction paths). */
SEXP sqlite_r_execute(SEXP db, SEXP sql, SEXP kind, SEXP params, SEXP txexpected)
{
    void *h = handle_of(db);
    const char *s = text_of(sql);
    int k = int_of(kind);
    char err[R_ERR_MAX];
    memset(err, 0, sizeof(err));
    int rc = 0;
    int type_code = -1;

    param_array pa;
    params_build(params, &pa);

    double number = 0;
    char text[R_ERR_MAX];
    memset(text, 0, sizeof(text));

    if (k == 3) {
        sqlite_core_param_t out;
        memset(&out, 0, sizeof(out));
        out.type = SQLITE_CORE_TYPE_INT;
        out.value.blob_or_text.ptr = text;
        out.value.blob_or_text.max_len = (int)sizeof(text);

        rc = sqlite_core_execute_scalar(h, s, pa.items, pa.count, &out,
                                        err, (int)sizeof(err));
        if (rc == SQLITE_OK) {
            type_code = out.type;
            if (out.type == SQLITE_CORE_TYPE_INT) {
                number = (double)out.value.iVal;
            } else if (out.type == SQLITE_CORE_TYPE_DOUBLE) {
                number = out.value.dVal;
            } else if (out.type == SQLITE_CORE_TYPE_TEXT) {
                int len = out.value.blob_or_text.len;
                if (len < 0) {
                    len = (int)strlen(out.value.blob_or_text.ptr);
                }
                if (len >= (int)sizeof(text)) {
                    len = (int)sizeof(text) - 1;
                }
                memcpy(text, out.value.blob_or_text.ptr, (size_t)len);
                text[len] = '\0';
            }
        }
    } else if (k == 4) {
        reader_ctx ctx;
        ctx.rows = 0;
        ctx.columns = 0;
        rc = sqlite_core_execute_reader(h, s, pa.items, pa.count,
                                        row_trampoline, &ctx, err,
                                        (int)sizeof(err));
        number = (double)ctx.rows;
    } else {
        switch (k) {
        case 1:
            rc = sqlite_core_execute_transaction(h, s, int_of(txexpected),
                                                 err, (int)sizeof(err));
            break;
        case 2:
            rc = sqlite_core_execute_non_query(h, s, pa.items, pa.count,
                                               err, (int)sizeof(err));
            break;
        default:
            rc = sqlite_core_execute_command(h, s, err, (int)sizeof(err));
            break;
        }
        number = (double)rc;
    }

    params_free(&pa);

    const char *names[5] = {"rc", "rows", "value", "type", "err"};
    SEXP values[5];
    values[0] = PROTECT(scalar_int(rc));
    values[1] = PROTECT(ScalarReal(number));
    if (k == 3 && rc == SQLITE_OK) {
        if (type_code == SQLITE_CORE_TYPE_INT || type_code == SQLITE_CORE_TYPE_DOUBLE) {
            values[2] = ScalarReal(number);
        } else if (type_code == SQLITE_CORE_TYPE_TEXT) {
            values[2] = mkString(text);
        } else {
            values[2] = R_NilValue;
        }
    } else {
        values[2] = R_NilValue;
    }
    values[3] = PROTECT(scalar_int(type_code));
    values[4] = PROTECT(err_string(err));
    SEXP res = PROTECT(result_list(5, names, values));
    UNPROTECT(5);
    return res;
}

/* ------------------------------------------------------------------ */
/* blob fields                                                         */
/* ------------------------------------------------------------------ */

/* sqlite_r_get_blob(db, sql) -> list(rc, data, copied) */
SEXP sqlite_r_get_blob(SEXP db, SEXP sql)
{
    (void)0;
    void *h = handle_of(db);
    const char *s = text_of(sql);
    int copied = 0;
    int cap = 4096;
    void *buf = malloc((size_t)cap);
    if (buf == NULL) {
        Rf_error("libsqlite3: out of memory reading the blob");
    }
    int rc = SQLITE_OK;
    for (;;) {
        copied = 0;
        rc = sqlite_core_get_blob(h, s, buf, cap, &copied);
        if (rc == SQLITE_CORE_BUFFER_TOO_SMALL && cap < (64 * 1024 * 1024)) {
            cap *= 2;
            void *grown = realloc(buf, (size_t)cap);
            if (grown == NULL) {
                free(buf);
                Rf_error("libsqlite3: out of memory reading the blob");
            }
            buf = grown;
            continue;
        }
        break;
    }

    const char *names[3] = {"rc", "data", "copied"};
    SEXP values[3];
    values[0] = PROTECT(scalar_int(rc));
    values[2] = PROTECT(scalar_int(copied));
    if (rc == SQLITE_OK && copied > 0) {
        values[1] = PROTECT(allocVector(RAWSXP, copied));
        memcpy(RAW(values[1]), buf, (size_t)copied);
    } else {
        values[1] = PROTECT(R_NilValue);
    }
    UNPROTECT(1);
    SEXP res = PROTECT(result_list(3, names, values));
    UNPROTECT(3);
    free(buf);
    return res;
}

/* sqlite_r_write_blob(db, kind, table, column, rowid, file, chunk)
 * kind: 0 insert_blob, 1 update_blob -> list(rc, written, err) */
SEXP sqlite_r_write_blob(SEXP db, SEXP kind, SEXP table, SEXP column,
                         SEXP rowid, SEXP file, SEXP chunk)
{
    void *h = handle_of(db);
    char err[R_ERR_MAX];
    memset(err, 0, sizeof(err));
    int written = 0;
    int rc;
    sqlite3_int64 id = (sqlite3_int64)real_of(rowid);

    if (int_of(kind) == 1) {
        rc = sqlite_core_update_blob(h, text_of(table), text_of(column), id,
                                     text_of(file), int_of(chunk), &written,
                                     err, (int)sizeof(err));
    } else {
        rc = sqlite_core_insert_blob(h, text_of(table), text_of(column), id,
                                     text_of(file), int_of(chunk), &written,
                                     err, (int)sizeof(err));
    }

    const char *names[3] = {"rc", "written", "err"};
    SEXP values[3];
    values[0] = PROTECT(scalar_int(rc));
    values[1] = PROTECT(scalar_int(written));
    values[2] = PROTECT(err_string(err));
    SEXP res = PROTECT(result_list(3, names, values));
    UNPROTECT(3);
    return res;
}

/* ------------------------------------------------------------------ */
/* json                                                               */
/* ------------------------------------------------------------------ */

/* Builds the (rows, json, err) result; the SQLite-allocated json is freed
 * here, like the other bindings do with their take()/takeAndFree(). */
static SEXP json_result(int rows, const char *err, char *json)
{
    const char *names[3] = {"rows", "json", "err"};
    SEXP values[3];
    values[0] = PROTECT(scalar_int(rows));
    if (json != NULL) {
        values[1] = PROTECT(mkString(json));
        sqlite_core_string_free(json);
    } else {
        values[1] = PROTECT(R_NilValue);
    }
    values[2] = PROTECT(err_string(err));
    SEXP res = PROTECT(result_list(3, names, values));
    UNPROTECT(3);
    return res;
}

/* sqlite_r_json(db, what, a, b, params)
 * what 0: execute_reader_to_json(sql = a, params = b) -> list(rows, json, err)
 * what 1: insert_json(table = a, json = b)              -> list(rows, err)
 * what 2: execute_reader driven by sqlite_core_json_writer_callback, wrapped
 *         with the same "[\n" ... "\n]" (or "[]") protocol that
 *         execute_reader_to_json uses -> list(rows, json, err) */
SEXP sqlite_r_json(SEXP db, SEXP what, SEXP a, SEXP b, SEXP params)
{
    void *h = handle_of(db);
    int w = int_of(what);
    char err[R_ERR_MAX];
    memset(err, 0, sizeof(err));
    int rows = 0;
    char *json = NULL;

    param_array pa;
    params_build(params, &pa);

    if (w == 1) {
        rows = sqlite_core_insert_json(h, text_of(a), text_of(b), err,
                                       (int)sizeof(err));
        params_free(&pa);
        return json_result(rows, err, json);
    }

    if (w == 2) {
        sqlite_core_json_builder_t builder;
        memset(&builder, 0, sizeof(builder));
        builder.string_ptr = sqlite3_mprintf("[\n");
        if (builder.string_ptr == NULL) {
            params_free(&pa);
            Rf_error("libsqlite3: out of memory building the JSON array");
        }
        builder.allocated_size = (size_t)sqlite3_msize(builder.string_ptr);
        builder.current_len = strlen(builder.string_ptr);

        int inner_rows = sqlite_core_execute_reader(
            h, text_of(a), pa.items, pa.count,
            sqlite_core_json_writer_callback, &builder, err, (int)sizeof(err));
        if (inner_rows >= 0 && !builder.has_error) {
            if (builder.rows_processed == 0) {
                sqlite3_free(builder.string_ptr);
                json = sqlite3_mprintf("[]");
                rows = 0;
            } else {
                size_t len = builder.current_len + 2;
                char *closed = (char *)sqlite3_realloc(builder.string_ptr,
                                                       (int)len + 1);
                if (closed != NULL) {
                    memcpy(closed + builder.current_len, "\n]", 3);
                    closed[len] = '\0';
                    json = closed;
                    rows = builder.rows_processed;
                } else {
                    sqlite3_free(builder.string_ptr);
                    rows = SQLITE_NOMEM;
                }
            }
        } else {
            sqlite3_free(builder.string_ptr);
            rows = inner_rows < 0 ? inner_rows : SQLITE_NOMEM;
        }
        params_free(&pa);
        return json_result(rows, err, json);
    }

    rows = sqlite_core_execute_reader_to_json(h, text_of(a), pa.items,
                                              pa.count, &json, err,
                                              (int)sizeof(err));
    params_free(&pa);
    return json_result(rows, err, json);
}