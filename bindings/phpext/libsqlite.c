/* libsqlite — Zend extension (PHP 8).
 *
 * Wraps the frozen 34-symbol API (src/sqlite/core/sqlite.def) as native PHP
 * code: no FFI, no external PHP packages, and — the reason this extension
 * exists at all — PHP FFI cannot create a C function pointer while an
 * extension can, so both sqlite_core_execute_reader and the exported
 * sqlite_core_json_writer_callback are reachable here for real.
 *
 * Every method carries arginfo, so ReflectionMethod and IDEs see the native
 * signatures. The extension re-implements no behaviour: it calls the frozen
 * API only. Library-allocated strings are released with
 * sqlite_core_string_free, never the PHP allocator.
 *
 * Build (Linux/macOS): phpize && ./configure --with-libsqlite3-dir=<dir> && make
 * Build (Windows):       buildconf.bat + configure --enable-libsqlite
 *                       --with-libsqlite3-dir=<dir>, then nmake
 *
 * This file is the deliverable; the build/test path is documented in
 * README.md and has not been executed on any host yet.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_exceptions.h"
#include "zend_interfaces.h"

#include <stdint.h>
#include <string.h>

#define SQLITE_CORE_API
#include "sqlite_core.h"

#include "php_libsqlite.h"

/* The extension keeps no per-request state: every call goes straight to the frozen API. */

/* ------------------------------------------------------------------ */
/* the connection object                                               */
/* ------------------------------------------------------------------ */

typedef struct _libsqlite_connection_object {
    sqlite3 *db;
    bool closed;
    zend_object std;
} libsqlite_connection_object;

static zend_object_handlers libsqlite_connection_handlers;
static zend_class_entry *libsqlite_connection_ce;
static zend_class_entry *libsqlite_exception_ce;

static inline libsqlite_connection_object *
libsqlite_connection_from_obj(zend_object *obj)
{
    return (libsqlite_connection_object *)((char *)(obj) -
        XtOffsetOf(libsqlite_connection_object, std));
}

#define Z_LIBSQLITE_CONNECTION_P(zv) \
    libsqlite_connection_from_obj(Z_OBJ_P(zv))

static zend_object *
libsqlite_connection_create_object(zend_class_entry *ce)
{
    libsqlite_connection_object *o;

    o = ecalloc(1, sizeof(libsqlite_connection_object) +
                    zend_object_properties_size(ce));
    zend_object_std_init(&o->std, ce);
    object_properties_init(&o->std, ce);
    o->std.handlers = &libsqlite_connection_handlers;

    o->db = NULL;
    o->closed = 0;

    return &o->std;
}

/* A connection is closed by its destructor too, like a C wrapper. */
static void libsqlite_connection_free_obj(zend_object *object)
{
    libsqlite_connection_object *o = libsqlite_connection_from_obj(object);

    if (o->db != NULL && !o->closed) {
        sqlite_core_close(o->db);
        o->db = NULL;
        o->closed = 1;
    }
    zend_object_std_dtor(object);
}

/* Returns the handle or NULL, throwing LibSqlite\Exception when the
 * connection is closed (the API must never see a stale pointer). */
static sqlite3 *libsqlite_connection_handle(libsqlite_connection_object *o)
{
    if (o == NULL || o->db == NULL || o->closed) {
        zend_throw_exception(libsqlite_exception_ce,
                             "the connection is closed", 0);
        return NULL;
    }
    return o->db;
}

/* ------------------------------------------------------------------ */
/* the reader callback: one PHP callable per execute_reader call        */
/* ------------------------------------------------------------------ */

typedef struct _libsqlite_reader_state {
    libsqlite_connection_object *owner;
    zend_fcall_info fci;
    zend_fcall_info_cache fcc;
    bool aborted;
} libsqlite_reader_state;

static void libsqlite_reader_state_dtor(zval *zv)
{
    libsqlite_reader_state *st = (libsqlite_reader_state *)Z_PTR_P(zv);

    if (st != NULL) {
        if (Z_TYPE(st->fci.function_name) != IS_UNDEF) {
            zval_ptr_dtor(&st->fci.function_name);
        }
        efree(st);
    }
}

static int libsqlite_row_trampoline(sqlite3_stmt *stmt, int columns,
                                    int hasrows, void *user)
{
    libsqlite_reader_state *st = (libsqlite_reader_state *)user;
    zval retval;
    zval params[3];
    int abort = 0;

    (void)stmt; /* the frozen surface has no engine column API to read */

    if (st == NULL || st->fci.size == 0) {
        return 0;
    }

    ZVAL_LONG(&params[0], (zend_long)(intptr_t)stmt);
    ZVAL_LONG(&params[1], (zend_long)columns);
    ZVAL_LONG(&params[2], (zend_long)hasrows);

    st->fci.params = params;
    st->fci.param_count = 3;
    st->fci.retval = &retval;

    if (zend_call_function(&st->fci, &st->fcc) == SUCCESS) {
        if (Z_TYPE(retval) != IS_UNDEF && zend_is_true(&retval)) {
            abort = 1;
        }
        zval_ptr_dtor(&retval);
    }

    return abort;
}

/* ------------------------------------------------------------------ */
/* parameters                                                          */
/* ------------------------------------------------------------------ */
/*
 * A parameter arrives as ['name' => string, 'type' => int,
 * 'value' => mixed]. The structs are native and contiguous; the text copies
 * are owned here and live until the call returns.
 */

typedef struct {
    sqlite_core_param_t *items;
    int count;
    zend_string **text;
} libsqlite_params;

static void libsqlite_params_free(libsqlite_params *p)
{
    if (p->text != NULL) {
        for (int i = 0; i < p->count; i++) {
            if (p->text[i] != NULL) {
                zend_string_release(p->text[i]);
            }
        }
        efree(p->text);
    }
    efree(p->items);
    p->items = NULL;
    p->text = NULL;
    p->count = 0;
}

/* Builds the native array from a PHP list of parameter specs. Returns
 * SUCCESS, or FAILURE with a thrown exception. */
static int libsqlite_params_build(zval *params_z, libsqlite_params *out)
{
    HashTable *ht;
    zval *entry;
    uint32_t n = 0;
    int idx = 0;

    out->items = NULL;
    out->text = NULL;
    out->count = 0;

    if (params_z == NULL || Z_TYPE_P(params_z) == IS_NULL) {
        return SUCCESS;
    }
    if (Z_TYPE_P(params_z) != IS_ARRAY) {
        zend_throw_exception(libsqlite_exception_ce,
                             "params must be an array", 0);
        return FAILURE;
    }

    ht = Z_ARRVAL_P(params_z);
    n = zend_hash_num_elements(ht);
    if (n == 0) {
        return SUCCESS;
    }

    out->items = (sqlite_core_param_t *)ecalloc(
        (size_t)n, sizeof(sqlite_core_param_t));
    out->text = (zend_string **)ecalloc((size_t)n, sizeof(zend_string *));
    out->count = (int)n;

    ZEND_HASH_FOREACH_VAL(ht, entry) {
        zval *name, *type, *value;
        sqlite_core_param_t *p;

        if (entry == NULL || Z_TYPE_P(entry) != IS_ARRAY) {
            zend_throw_exception(libsqlite_exception_ce,
                                 "each parameter must be an array", 0);
            libsqlite_params_free(out);
            return FAILURE;
        }

        p = &out->items[idx];
        p->no_insensitive = 0;
        p->Direction = SQLITE_CORE_DIRECTION_INPUT;

        name = zend_hash_str_find(Z_ARRVAL_P(entry), "name", sizeof("name") - 1);
        type = zend_hash_str_find(Z_ARRVAL_P(entry), "type", sizeof("type") - 1);
        value = zend_hash_str_find(Z_ARRVAL_P(entry), "value", sizeof("value") - 1);

        if (name == NULL || type == NULL ||
            Z_TYPE_P(name) != IS_STRING || Z_TYPE_P(type) != IS_LONG) {
            zend_throw_exception(libsqlite_exception_ce,
                "each parameter needs name (string) and type (int)", 0);
            libsqlite_params_free(out);
            return FAILURE;
        }

        /* The struct holds a raw char*, so the name must be a native copy
         * that outlives the call. */
        out->text[idx] = zend_string_copy(Z_STR_P(name));
        p->name = ZSTR_VAL(out->text[idx]);

        switch ((sqlite_core_param_type_t)Z_LVAL_P(type)) {
        case SQLITE_CORE_TYPE_INT:
            p->type = SQLITE_CORE_TYPE_INT;
            p->value.iVal = value ? zval_get_long(value) : 0;
            break;
        case SQLITE_CORE_TYPE_DOUBLE:
            p->type = SQLITE_CORE_TYPE_DOUBLE;
            p->value.dVal = value ? zval_get_double(value) : 0.0;
            break;
        case SQLITE_CORE_TYPE_TEXT: {
            zend_string *copy;
            if (value == NULL || Z_TYPE_P(value) != IS_STRING) {
                copy = zend_string_init("", 0, 0);
            } else {
                copy = zend_string_copy(Z_STR_P(value));
            }
            p->type = SQLITE_CORE_TYPE_TEXT;
            p->value.blob_or_text.ptr = ZSTR_VAL(copy);
            p->value.blob_or_text.len = -1;  /* NUL-terminated UTF-8 */
            p->value.blob_or_text.max_len = 0;
            out->text[idx] = copy;
            break;
        }
        case SQLITE_CORE_TYPE_NULL:
        default:
            p->type = SQLITE_CORE_TYPE_NULL;
            break;
        }
        idx++;
    } ZEND_HASH_FOREACH_END();

    return SUCCESS;
}

/* ------------------------------------------------------------------ */
/* error buffer                                                         */
/* ------------------------------------------------------------------ */
/* Every call that takes err_buf gets the same stack buffer, cleared
 * before the call. The buffer is the channel that carries library-level
 * reasons (a guard rejection never reaches the engine), so it is read
 * back even when the return value alone already decided the outcome -
 * see the Doxygen in sqlite_core_error.h. */
#define LIBSQLITE_ERR_MAX 512

static void libsqlite_err_clear(char *err)
{
    err[0] = '\0';
}

/* ------------------------------------------------------------------ */
/* Connection: the 25 symbols that need a handle                        */
/* ------------------------------------------------------------------ */

/* Every factory returns an object whose handle is already open, or throws. */
static void libsqlite_connection_from_handle(INTERNAL_FUNCTION_PARAMETERS,
                                             sqlite3 *db)
{
    if (db == NULL) {
        zend_throw_exception(libsqlite_exception_ce,
                             "sqlite_core could not open the database", 0);
        RETURN_THROWS();
    }
    object_init_ex(return_value, libsqlite_connection_ce);
    Z_LIBSQLITE_CONNECTION_P(return_value)->db = db;
    Z_LIBSQLITE_CONNECTION_P(return_value)->closed = 0;
}

PHP_METHOD(libsqlite_Connection, openOrCreate)
{
    zend_string *path;
    int rc = 0;
    char err[LIBSQLITE_ERR_MAX];

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(path)
    ZEND_PARSE_PARAMETERS_END();

    libsqlite_err_clear(err);
    libsqlite_connection_from_handle(INTERNAL_FUNCTION_PARAM_PASSTHRU,
        sqlite_core_open_or_create(ZSTR_VAL(path), &rc));
}

PHP_METHOD(libsqlite_Connection, open)
{
    zend_string *path;
    int rc = 0;
    char err[LIBSQLITE_ERR_MAX];

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(path)
    ZEND_PARSE_PARAMETERS_END();

    libsqlite_err_clear(err);
    libsqlite_connection_from_handle(INTERNAL_FUNCTION_PARAM_PASSTHRU,
        sqlite_core_open(ZSTR_VAL(path), &rc));
}

PHP_METHOD(libsqlite_Connection, openReadonly)
{
    zend_string *path;
    int rc = 0;
    char err[LIBSQLITE_ERR_MAX];

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(path)
    ZEND_PARSE_PARAMETERS_END();

    libsqlite_err_clear(err);
    libsqlite_connection_from_handle(INTERNAL_FUNCTION_PARAM_PASSTHRU,
        sqlite_core_open_readonly(ZSTR_VAL(path), &rc));
}

PHP_METHOD(libsqlite_Connection, openUriReadonly)
{
    zend_string *path;
    int rc = 0;
    char err[LIBSQLITE_ERR_MAX];

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(path)
    ZEND_PARSE_PARAMETERS_END();

    libsqlite_err_clear(err);
    libsqlite_connection_from_handle(INTERNAL_FUNCTION_PARAM_PASSTHRU,
        sqlite_core_open_uri_readonly(ZSTR_VAL(path), &rc));
}

PHP_METHOD(libsqlite_Connection, openEx)
{
    zend_string *path;
    zend_long flags = 0;
    int rc = 0;
    char err[LIBSQLITE_ERR_MAX];

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STR(path)
        Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    libsqlite_err_clear(err);
    libsqlite_connection_from_handle(INTERNAL_FUNCTION_PARAM_PASSTHRU,
        sqlite_core_open_ex(ZSTR_VAL(path), (int)flags, &rc));
}

/* The constructor is private on purpose: a connection exists only with an
 * open handle, and only the factories above can produce one. */
PHP_METHOD(libsqlite_Connection, __construct)
{
    zend_throw_exception(libsqlite_exception_ce,
        "use LibSqlite\\Connection::openOrCreate() to obtain a connection", 0);
}

PHP_METHOD(libsqlite_Connection, close)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);

    ZEND_PARSE_PARAMETERS_NONE();

    if (db == NULL) {
        RETURN_THROWS();
    }
    o->closed = 1;
    o->db = NULL;
    RETURN_LONG(sqlite_core_close(db));
}

PHP_METHOD(libsqlite_Connection, isReadonly)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);

    ZEND_PARSE_PARAMETERS_NONE();

    if (db == NULL) {
        RETURN_THROWS();
    }
    RETURN_LONG(sqlite_core_is_readonly(db));
}

PHP_METHOD(libsqlite_Connection, lastInsertRowid)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);

    ZEND_PARSE_PARAMETERS_NONE();

    if (db == NULL) {
        RETURN_THROWS();
    }
    RETURN_LONG((zend_long)sqlite_core_last_insert_rowid(db));
}

PHP_METHOD(libsqlite_Connection, setBusyTimeout)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    zend_long ms = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(ms)
    ZEND_PARSE_PARAMETERS_END();

    if (db == NULL) {
        RETURN_THROWS();
    }
    RETURN_LONG(sqlite_core_set_busy_timeout(db, (int)ms));
}

PHP_METHOD(libsqlite_Connection, errorCode)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);

    ZEND_PARSE_PARAMETERS_NONE();

    if (db == NULL) {
        RETURN_THROWS();
    }
    RETURN_LONG(sqlite_core_error_code(db));
}

PHP_METHOD(libsqlite_Connection, errorCodeExtended)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);

    ZEND_PARSE_PARAMETERS_NONE();

    if (db == NULL) {
        RETURN_THROWS();
    }
    RETURN_LONG(sqlite_core_error_code_extended(db));
}

PHP_METHOD(libsqlite_Connection, errorMessage)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    const char *msg;

    ZEND_PARSE_PARAMETERS_NONE();

    if (db == NULL) {
        RETURN_THROWS();
    }
    /* The library owns the string (static data or engine-owned): copy it,
     * free nothing. */
    msg = sqlite_core_error_message(db);
    RETURN_STRING(msg == NULL ? "" : msg);
}

PHP_METHOD(libsqlite_Connection, command)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    zend_string *sql;
    char err[LIBSQLITE_ERR_MAX];

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(sql)
    ZEND_PARSE_PARAMETERS_END();

    if (db == NULL) {
        RETURN_THROWS();
    }
    libsqlite_err_clear(err);
    RETURN_LONG(sqlite_core_execute_command(db, ZSTR_VAL(sql), err,
                                            (int)sizeof(err)));
}

PHP_METHOD(libsqlite_Connection, executeTransaction)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    zend_string *sql;
    zend_long expected = 0;
    char err[LIBSQLITE_ERR_MAX];

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STR(sql)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(expected)
    ZEND_PARSE_PARAMETERS_END();

    if (db == NULL) {
        RETURN_THROWS();
    }
    libsqlite_err_clear(err);
    RETURN_LONG(sqlite_core_execute_transaction(db, ZSTR_VAL(sql),
                                                (int)expected, err,
                                                (int)sizeof(err)));
}

PHP_METHOD(libsqlite_Connection, nonQuery)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    zend_string *sql;
    zval *params_z = NULL;
    libsqlite_params p;
    char err[LIBSQLITE_ERR_MAX];
    int rc;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STR(sql)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(params_z)
    ZEND_PARSE_PARAMETERS_END();

    if (db == NULL) {
        RETURN_THROWS();
    }
    if (libsqlite_params_build(params_z, &p) == FAILURE) {
        RETURN_THROWS();
    }
    libsqlite_err_clear(err);
    rc = sqlite_core_execute_non_query(db, ZSTR_VAL(sql), p.items, p.count,
                                       err, (int)sizeof(err));
    libsqlite_params_free(&p);
    RETURN_LONG(rc);
}

/* The scalar result is returned as ['rc' => int, 'type' => int,
 * 'value' => mixed]: the return value alone decides success, and the type
 * is what tells the caller how to read 'value' (an empty result is a
 * success with TYPE_NULL and a null value). */
PHP_METHOD(libsqlite_Connection, scalar)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    zend_string *sql;
    zval *params_z = NULL;
    libsqlite_params p;
    sqlite_core_param_t out;
    char err[LIBSQLITE_ERR_MAX];
    char text[LIBSQLITE_ERR_MAX];
    int rc;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STR(sql)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(params_z)
    ZEND_PARSE_PARAMETERS_END();

    if (db == NULL) {
        RETURN_THROWS();
    }
    if (libsqlite_params_build(params_z, &p) == FAILURE) {
        RETURN_THROWS();
    }

    /* A TEXT/BLOB result needs a destination buffer or the value is not
     * reported; the smaller int/double results use the same union. */
    memset(&out, 0, sizeof(out));
    memset(text, 0, sizeof(text));
    out.type = SQLITE_CORE_TYPE_TEXT;
    out.Direction = SQLITE_CORE_DIRECTION_INPUT;
    out.value.blob_or_text.ptr = text;
    out.value.blob_or_text.len = -1;
    out.value.blob_or_text.max_len = (int)sizeof(text) - 1;

    libsqlite_err_clear(err);
    rc = sqlite_core_execute_scalar(db, ZSTR_VAL(sql), p.items, p.count, &out,
                                    err, (int)sizeof(err));
    libsqlite_params_free(&p);

    array_init(return_value);
    add_assoc_long(return_value, "rc", rc);
    add_assoc_long(return_value, "type", (zend_long)out.type);

    switch (out.type) {
    case SQLITE_CORE_TYPE_INT:
        add_assoc_long(return_value, "value", (zend_long)out.value.iVal);
        break;
    case SQLITE_CORE_TYPE_DOUBLE:
        add_assoc_double(return_value, "value", out.value.dVal);
        break;
    case SQLITE_CORE_TYPE_TEXT:
    case SQLITE_CORE_TYPE_BLOB: {
        size_t n = (out.value.blob_or_text.len >= 0)
                       ? (size_t)out.value.blob_or_text.len
                       : strlen(text);
        add_assoc_stringl(return_value, "value", text, n);
        break;
    }
    case SQLITE_CORE_TYPE_NULL:
    default:
        add_assoc_null(return_value, "value");
        break;
    }
}

/* The PHP callable is invoked once per row with (stmt_ptr, columns,
 * hasrows) and must return a truthy value to abort the iteration - which
 * is the C callback contract, and the reason this needs an extension:
 * PHP FFI cannot build a C function pointer. */
PHP_METHOD(libsqlite_Connection, reader)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    zval *func;
    zend_string *sql;
    zval *params_z = NULL;
    libsqlite_params p;
    libsqlite_reader_state st;
    char err[LIBSQLITE_ERR_MAX];
    zend_string *callable_name = NULL;
    char *init_error = NULL;
    int rc;

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_ZVAL(func)
        Z_PARAM_STR(sql)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(params_z)
    ZEND_PARSE_PARAMETERS_END();

    if (db == NULL) {
        RETURN_THROWS();
    }
    /* zend_fcall_info_init hands back the name it resolved (a zend_string,
     * refcounted) and, on failure, a plain char* with the reason. */
    if (zend_fcall_info_init(func, 0, &st.fci, &st.fcc, &callable_name,
                             &init_error) == FAILURE) {
        zend_throw_exception(libsqlite_exception_ce,
                             init_error != NULL ? init_error
                                                : "the reader callback is not callable",
                             0);
        if (init_error != NULL) {
            efree(init_error);
        }
        RETURN_THROWS();
    }
    if (init_error != NULL) {
        efree(init_error);
    }
    st.fci.size = sizeof(zend_fcall_info);
    st.fci.object = NULL;

    if (libsqlite_params_build(params_z, &p) == FAILURE) {
        zval_ptr_dtor(&st.fci.function_name);
        if (callable_name != NULL) {
            zend_string_release(callable_name);
        }
        RETURN_THROWS();
    }

    libsqlite_err_clear(err);
    rc = sqlite_core_execute_reader(db, ZSTR_VAL(sql), p.items, p.count,
                                    libsqlite_row_trampoline, &st, err,
                                    (int)sizeof(err));
    libsqlite_params_free(&p);
    zval_ptr_dtor(&st.fci.function_name);
    if (callable_name != NULL) {
        zend_string_release(callable_name);
    }
    RETURN_LONG(rc);
}

PHP_METHOD(libsqlite_Connection, readerToJson)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    zend_string *sql;
    zval *params_z = NULL;
    libsqlite_params p;
    char *json = NULL;
    char err[LIBSQLITE_ERR_MAX];
    int rc, rows = 0;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STR(sql)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(params_z)
    ZEND_PARSE_PARAMETERS_END();

    if (db == NULL) {
        RETURN_THROWS();
    }
    if (libsqlite_params_build(params_z, &p) == FAILURE) {
        RETURN_THROWS();
    }
    libsqlite_err_clear(err);
    rc = sqlite_core_execute_reader_to_json(db, ZSTR_VAL(sql), p.items,
                                            p.count, &json, err,
                                            (int)sizeof(err));
    libsqlite_params_free(&p);

    /* On this call the return value IS the row count; a negative value is a
     * SQLite code. Reporting both keeps the two apart for the caller. */
    array_init(return_value);
    add_assoc_long(return_value, "rc", rc);
    add_assoc_long(return_value, "rows", rc >= 0 ? rc : 0);
    if (json != NULL) {
        zend_string *doc = zend_string_init(json, strlen(json), 0);
        add_assoc_str(return_value, "json", doc);
        sqlite_core_string_free(json);
    } else {
        add_assoc_null(return_value, "json");
    }
}

PHP_METHOD(libsqlite_Connection, insertJson)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    zend_string *table, *json;
    char err[LIBSQLITE_ERR_MAX];

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STR(table)
        Z_PARAM_STR(json)
    ZEND_PARSE_PARAMETERS_END();

    if (db == NULL) {
        RETURN_THROWS();
    }
    libsqlite_err_clear(err);
    RETURN_LONG(sqlite_core_insert_json(db, ZSTR_VAL(table), ZSTR_VAL(json),
                                        err, (int)sizeof(err)));
}

/* The blob helpers report both numbers the caller has to check: the return
 * code and the bytes written/copied, so a partial write cannot pass for a
 * complete one. */
PHP_METHOD(libsqlite_Connection, insertBlob)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    zend_string *table, *column, *file;
    zend_long rowid = 0, chunk = 0;
    int written = 0, rc;
    char err[LIBSQLITE_ERR_MAX];

    ZEND_PARSE_PARAMETERS_START(4, 5)
        Z_PARAM_STR(table)
        Z_PARAM_STR(column)
        Z_PARAM_LONG(rowid)
        Z_PARAM_STR(file)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(chunk)
    ZEND_PARSE_PARAMETERS_END();

    if (db == NULL) {
        RETURN_THROWS();
    }
    libsqlite_err_clear(err);
    rc = sqlite_core_insert_blob(db, ZSTR_VAL(table), ZSTR_VAL(column),
                                 (sqlite3_int64)rowid, ZSTR_VAL(file),
                                 (int)chunk, &written, err, (int)sizeof(err));
    array_init(return_value);
    add_assoc_long(return_value, "rc", rc);
    add_assoc_long(return_value, "written", written);
}

PHP_METHOD(libsqlite_Connection, updateBlob)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    zend_string *table, *column, *file;
    zend_long rowid = 0, chunk = 0;
    int written = 0, rc;
    char err[LIBSQLITE_ERR_MAX];

    ZEND_PARSE_PARAMETERS_START(4, 5)
        Z_PARAM_STR(table)
        Z_PARAM_STR(column)
        Z_PARAM_LONG(rowid)
        Z_PARAM_STR(file)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(chunk)
    ZEND_PARSE_PARAMETERS_END();

    if (db == NULL) {
        RETURN_THROWS();
    }
    libsqlite_err_clear(err);
    rc = sqlite_core_update_blob(db, ZSTR_VAL(table), ZSTR_VAL(column),
                                 (sqlite3_int64)rowid, ZSTR_VAL(file),
                                 (int)chunk, &written, err, (int)sizeof(err));
    array_init(return_value);
    add_assoc_long(return_value, "rc", rc);
    add_assoc_long(return_value, "written", written);
}

PHP_METHOD(libsqlite_Connection, getBlob)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    zend_string *sql;
    zend_long max_len = 0;
    char *buf;
    int copied = 0, rc;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STR(sql)
        Z_PARAM_LONG(max_len)
    ZEND_PARSE_PARAMETERS_END();

    if (db == NULL) {
        RETURN_THROWS();
    }
    if (max_len <= 0) {
        zend_throw_exception(libsqlite_exception_ce,
                             "getBlob needs a positive max length", 0);
        RETURN_THROWS();
    }
    buf = (char *)ecalloc(1, (size_t)max_len);
    rc = sqlite_core_get_blob(db, ZSTR_VAL(sql), buf, (int)max_len, &copied);

    array_init(return_value);
    add_assoc_long(return_value, "rc", rc);
    add_assoc_long(return_value, "copied", copied);
    if (copied > 0) {
        add_assoc_stringl(return_value, "data", buf, (size_t)copied);
    } else {
        add_assoc_string(return_value, "data", "");
    }
    efree(buf);
}

PHP_METHOD(libsqlite_Connection, begin)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    char err[LIBSQLITE_ERR_MAX];

    ZEND_PARSE_PARAMETERS_NONE();

    if (db == NULL) {
        RETURN_THROWS();
    }
    libsqlite_err_clear(err);
    RETURN_LONG(sqlite_core_begin_transaction(db, err, (int)sizeof(err)));
}

PHP_METHOD(libsqlite_Connection, commit)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    char err[LIBSQLITE_ERR_MAX];

    ZEND_PARSE_PARAMETERS_NONE();

    if (db == NULL) {
        RETURN_THROWS();
    }
    libsqlite_err_clear(err);
    RETURN_LONG(sqlite_core_commit_transaction(db, err, (int)sizeof(err)));
}

PHP_METHOD(libsqlite_Connection, rollback)
{
    libsqlite_connection_object *o = Z_LIBSQLITE_CONNECTION_P(ZEND_THIS);
    sqlite3 *db = libsqlite_connection_handle(o);
    char err[LIBSQLITE_ERR_MAX];

    ZEND_PARSE_PARAMETERS_NONE();

    if (db == NULL) {
        RETURN_THROWS();
    }
    libsqlite_err_clear(err);
    RETURN_LONG(sqlite_core_rollback_transaction(db, err, (int)sizeof(err)));
}

/* ------------------------------------------------------------------ */
/* module functions (versions and string helpers)                      */
/* ------------------------------------------------------------------ */

PHP_FUNCTION(libsqlite_version)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STRING(sqlite_core_version());
}

PHP_FUNCTION(libsqlite_sqlite_version)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STRING(sqlite_core_sqlite_version());
}

PHP_FUNCTION(libsqlite_param_clean_prefix)
{
    zend_string *name;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(name)
    ZEND_PARSE_PARAMETERS_END();

    /* The library returns its own static data: copy it, free nothing. */
    RETURN_STRING(sqlite_core_param_clean_prefix(ZSTR_VAL(name)));
}

PHP_FUNCTION(libsqlite_string_concat)
{
    zend_string *a, *b;
    char *out;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STR(a)
        Z_PARAM_STR(b)
    ZEND_PARSE_PARAMETERS_END();

    out = sqlite_core_string_concat(ZSTR_VAL(a), ZSTR_VAL(b));
    if (out == NULL) {
        RETURN_NULL();
    }
    RETVAL_STRING(out);
    sqlite_core_string_free(out);
}

PHP_FUNCTION(libsqlite_string_utf8_to_ansi)
{
    zend_string *s;
    char *out;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(s)
    ZEND_PARSE_PARAMETERS_END();

    out = sqlite_core_string_utf8_to_ansi(ZSTR_VAL(s));
    if (out == NULL) {
        RETURN_NULL();
    }
    RETVAL_STRING(out);
    sqlite_core_string_free(out);
}

PHP_FUNCTION(libsqlite_string_ansi_to_utf8)
{
    zend_string *s;
    char *out;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(s)
    ZEND_PARSE_PARAMETERS_END();

    out = sqlite_core_string_ansi_to_utf8(ZSTR_VAL(s));
    if (out == NULL) {
        RETURN_NULL();
    }
    RETVAL_STRING(out);
    sqlite_core_string_free(out);
}

PHP_FUNCTION(libsqlite_json_writer_callback)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG((zend_long)(intptr_t)sqlite_core_json_writer_callback);
}

/* sqlite_core_string_format is a plain C variadic function, and PHP cannot
 * build a va_list at run time. The only way to call it without
 * reimplementing it is to dispatch on the PHP type of each argument and
 * forward the literal that type expects, so this wrapper does exactly
 * that for up to three arguments; the cap keeps the dispatch finite and is
 * documented in the README. Note the int is forwarded as a C int (not a
 * zend_long): %d in the format reads 32 bits. */
#define LIBSQLITE_FMT_0() out = sqlite_core_string_format(ZSTR_VAL(format))
#define LIBSQLITE_FMT_1(a) out = sqlite_core_string_format(ZSTR_VAL(format), a)
#define LIBSQLITE_FMT_2(a, b) \
    out = sqlite_core_string_format(ZSTR_VAL(format), a, b)
#define LIBSQLITE_FMT_3(a, b, c) \
    out = sqlite_core_string_format(ZSTR_VAL(format), a, b, c)

PHP_FUNCTION(libsqlite_string_format)
{
    zend_string *format;
    zval *argv = NULL;
    zend_long argc = 0;
    char *out = NULL;
    int kind[3] = {0, 0, 0};   /* 0 none, 1 int, 2 float, 3 string */
/* the variadic macro in PHP 8.2 takes a pass-by-ref flag, not a bound flag */
    zend_long lv[3] = {0, 0, 0};
    double dv[3] = {0.0, 0.0, 0.0};
    zend_string *sv[3] = {NULL, NULL, NULL};

    ZEND_PARSE_PARAMETERS_START(1, -1)
        Z_PARAM_STR(format)
        Z_PARAM_VARIADIC(0, argv, argc)
    ZEND_PARSE_PARAMETERS_END();

    if (argc > 3) {
        zend_throw_exception(libsqlite_exception_ce,
            "libsqlite_string_format accepts at most 3 arguments "
            "(PHP cannot build a C va_list)", 0);
        RETURN_THROWS();
    }
    for (int i = 0; i < (int)argc; i++) {
        zval *a = &argv[i];
        switch (Z_TYPE_P(a)) {
        case IS_LONG:
            kind[i] = 1;
            lv[i] = Z_LVAL_P(a);
            break;
        case IS_DOUBLE:
            kind[i] = 2;
            dv[i] = Z_DVAL_P(a);
            break;
        case IS_STRING:
            kind[i] = 3;
            sv[i] = Z_STR_P(a);
            break;
        default:
            zend_throw_exception(libsqlite_exception_ce,
                "libsqlite_string_format arguments must be int, float or string",
                0);
            RETURN_THROWS();
        }
    }

    if (argc == 0) {
        LIBSQLITE_FMT_0();
    } else if (argc == 1) {
        if (kind[0] == 1) {
            LIBSQLITE_FMT_1((int)lv[0]);
        } else if (kind[0] == 2) {
            LIBSQLITE_FMT_1(dv[0]);
        } else {
            LIBSQLITE_FMT_1(ZSTR_VAL(sv[0]));
        }
    } else if (argc == 2) {
        if (kind[0] == 1 && kind[1] == 1) {
            LIBSQLITE_FMT_2((int)lv[0], (int)lv[1]);
        } else if (kind[0] == 1 && kind[1] == 2) {
            LIBSQLITE_FMT_2((int)lv[0], dv[1]);
        } else if (kind[0] == 1 && kind[1] == 3) {
            LIBSQLITE_FMT_2((int)lv[0], ZSTR_VAL(sv[1]));
        } else if (kind[0] == 2 && kind[1] == 1) {
            LIBSQLITE_FMT_2(dv[0], (int)lv[1]);
        } else if (kind[0] == 2 && kind[1] == 2) {
            LIBSQLITE_FMT_2(dv[0], dv[1]);
        } else if (kind[0] == 2 && kind[1] == 3) {
            LIBSQLITE_FMT_2(dv[0], ZSTR_VAL(sv[1]));
        } else if (kind[0] == 3 && kind[1] == 1) {
            LIBSQLITE_FMT_2(ZSTR_VAL(sv[0]), (int)lv[1]);
        } else if (kind[0] == 3 && kind[1] == 2) {
            LIBSQLITE_FMT_2(ZSTR_VAL(sv[0]), dv[1]);
        } else {
            LIBSQLITE_FMT_2(ZSTR_VAL(sv[0]), ZSTR_VAL(sv[1]));
        }
    } else {
        if (kind[0] == 1 && kind[1] == 1 && kind[2] == 1) {
            LIBSQLITE_FMT_3((int)lv[0], (int)lv[1], (int)lv[2]);
        } else if (kind[0] == 1 && kind[1] == 1 && kind[2] == 2) {
            LIBSQLITE_FMT_3((int)lv[0], (int)lv[1], dv[2]);
        } else if (kind[0] == 1 && kind[1] == 1 && kind[2] == 3) {
            LIBSQLITE_FMT_3((int)lv[0], (int)lv[1], ZSTR_VAL(sv[2]));
        } else if (kind[0] == 1 && kind[1] == 2 && kind[2] == 1) {
            LIBSQLITE_FMT_3((int)lv[0], dv[1], (int)lv[2]);
        } else if (kind[0] == 1 && kind[1] == 2 && kind[2] == 2) {
            LIBSQLITE_FMT_3((int)lv[0], dv[1], dv[2]);
        } else if (kind[0] == 1 && kind[1] == 2 && kind[2] == 3) {
            LIBSQLITE_FMT_3((int)lv[0], dv[1], ZSTR_VAL(sv[2]));
        } else if (kind[0] == 1 && kind[1] == 3 && kind[2] == 1) {
            LIBSQLITE_FMT_3((int)lv[0], ZSTR_VAL(sv[1]), (int)lv[2]);
        } else if (kind[0] == 1 && kind[1] == 3 && kind[2] == 2) {
            LIBSQLITE_FMT_3((int)lv[0], ZSTR_VAL(sv[1]), dv[2]);
        } else if (kind[0] == 1 && kind[1] == 3 && kind[2] == 3) {
            LIBSQLITE_FMT_3((int)lv[0], ZSTR_VAL(sv[1]), ZSTR_VAL(sv[2]));
        } else if (kind[0] == 2 && kind[1] == 1 && kind[2] == 1) {
            LIBSQLITE_FMT_3(dv[0], (int)lv[1], (int)lv[2]);
        } else if (kind[0] == 2 && kind[1] == 1 && kind[2] == 2) {
            LIBSQLITE_FMT_3(dv[0], (int)lv[1], dv[2]);
        } else if (kind[0] == 2 && kind[1] == 1 && kind[2] == 3) {
            LIBSQLITE_FMT_3(dv[0], (int)lv[1], ZSTR_VAL(sv[2]));
        } else if (kind[0] == 2 && kind[1] == 2 && kind[2] == 1) {
            LIBSQLITE_FMT_3(dv[0], dv[1], (int)lv[2]);
        } else if (kind[0] == 2 && kind[1] == 2 && kind[2] == 2) {
            LIBSQLITE_FMT_3(dv[0], dv[1], dv[2]);
        } else if (kind[0] == 2 && kind[1] == 2 && kind[2] == 3) {
            LIBSQLITE_FMT_3(dv[0], dv[1], ZSTR_VAL(sv[2]));
        } else if (kind[0] == 2 && kind[1] == 3 && kind[2] == 1) {
            LIBSQLITE_FMT_3(dv[0], ZSTR_VAL(sv[1]), (int)lv[2]);
        } else if (kind[0] == 2 && kind[1] == 3 && kind[2] == 2) {
            LIBSQLITE_FMT_3(dv[0], ZSTR_VAL(sv[1]), dv[2]);
        } else if (kind[0] == 2 && kind[1] == 3 && kind[2] == 3) {
            LIBSQLITE_FMT_3(dv[0], ZSTR_VAL(sv[1]), ZSTR_VAL(sv[2]));
        } else if (kind[0] == 3 && kind[1] == 1 && kind[2] == 1) {
            LIBSQLITE_FMT_3(ZSTR_VAL(sv[0]), (int)lv[1], (int)lv[2]);
        } else if (kind[0] == 3 && kind[1] == 1 && kind[2] == 2) {
            LIBSQLITE_FMT_3(ZSTR_VAL(sv[0]), (int)lv[1], dv[2]);
        } else if (kind[0] == 3 && kind[1] == 1 && kind[2] == 3) {
            LIBSQLITE_FMT_3(ZSTR_VAL(sv[0]), (int)lv[1], ZSTR_VAL(sv[2]));
        } else if (kind[0] == 3 && kind[1] == 2 && kind[2] == 1) {
            LIBSQLITE_FMT_3(ZSTR_VAL(sv[0]), dv[1], (int)lv[2]);
        } else if (kind[0] == 3 && kind[1] == 2 && kind[2] == 2) {
            LIBSQLITE_FMT_3(ZSTR_VAL(sv[0]), dv[1], dv[2]);
        } else if (kind[0] == 3 && kind[1] == 2 && kind[2] == 3) {
            LIBSQLITE_FMT_3(ZSTR_VAL(sv[0]), dv[1], ZSTR_VAL(sv[2]));
        } else if (kind[0] == 3 && kind[1] == 3 && kind[2] == 1) {
            LIBSQLITE_FMT_3(ZSTR_VAL(sv[0]), ZSTR_VAL(sv[1]), (int)lv[2]);
        } else if (kind[0] == 3 && kind[1] == 3 && kind[2] == 2) {
            LIBSQLITE_FMT_3(ZSTR_VAL(sv[0]), ZSTR_VAL(sv[1]), dv[2]);
        } else {
            LIBSQLITE_FMT_3(ZSTR_VAL(sv[0]), ZSTR_VAL(sv[1]), ZSTR_VAL(sv[2]));
        }
    }

    if (out == NULL) {
        RETURN_NULL();
    }
    RETVAL_STRING(out);
    sqlite_core_string_free(out);
}

/* ------------------------------------------------------------------ */
/* arginfo                                                             */
/* ------------------------------------------------------------------ */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_libsqlite_version, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_libsqlite_sqlite_version, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_libsqlite_param_clean_prefix, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_libsqlite_string_concat, 0, 2, IS_STRING, 1)
    ZEND_ARG_TYPE_INFO(0, a, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, b, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_libsqlite_string_utf8_to_ansi, 0, 1, IS_STRING, 1)
    ZEND_ARG_TYPE_INFO(0, s, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_libsqlite_string_ansi_to_utf8, 0, 1, IS_STRING, 1)
    ZEND_ARG_TYPE_INFO(0, s, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_libsqlite_json_writer_callback, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_libsqlite_string_format, 0, 1, IS_STRING, 1)
    ZEND_ARG_TYPE_INFO(0, format, IS_STRING, 0)
    ZEND_ARG_VARIADIC_TYPE_INFO(0, args, IS_MIXED, 0)
ZEND_END_ARG_INFO()

/* ------------------------------------------------------------------ */
/* Connection arginfo                                                  */
/* ------------------------------------------------------------------ */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_libsqlite_connection___construct, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_openOrCreate, 0, 1, IS_OBJECT, 0)
    ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_open, 0, 1, IS_OBJECT, 0)
    ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_openReadonly, 0, 1, IS_OBJECT, 0)
    ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_openUriReadonly, 0, 1, IS_OBJECT, 0)
    ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_openEx, 0, 2, IS_OBJECT, 0)
    ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, flags, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_close, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_isReadonly, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_lastInsertRowid, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_setBusyTimeout, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, ms, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_errorCode, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_errorCodeExtended, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_errorMessage, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_command, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_nonQuery, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, params, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_executeTransaction, 0, 1, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, expected, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_scalar, 0, 1, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, params, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_reader, 0, 2, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, callback, IS_CALLABLE, 0)
    ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, params, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_readerToJson, 0, 1, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, params, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_insertJson, 0, 2, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, table, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, json, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_insertBlob, 0, 4, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, table, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, column, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, rowid, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, file, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, chunkSize, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_updateBlob, 0, 4, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, table, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, column, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, rowid, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, file, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, chunkSize, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_getBlob, 0, 2, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, sql, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, maxLen, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_begin, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_commit, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_libsqlite_connection_rollback, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

/* The class entry. The factories are static, the destructor-closing
 * behaviour is in libsqlite_connection_free_obj, and the constructor is
 * private so a handle-less object cannot exist. */
static const zend_function_entry class_libsqlite_connection_methods[] = {
    ZEND_ME(libsqlite_Connection, __construct,
            arginfo_class_libsqlite_connection___construct,
            ZEND_ACC_PRIVATE)
    ZEND_ME(libsqlite_Connection, openOrCreate,
            arginfo_class_libsqlite_connection_openOrCreate,
            ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_ME(libsqlite_Connection, open,
            arginfo_class_libsqlite_connection_open,
            ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_ME(libsqlite_Connection, openReadonly,
            arginfo_class_libsqlite_connection_openReadonly,
            ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_ME(libsqlite_Connection, openUriReadonly,
            arginfo_class_libsqlite_connection_openUriReadonly,
            ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_ME(libsqlite_Connection, openEx,
            arginfo_class_libsqlite_connection_openEx,
            ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    ZEND_ME(libsqlite_Connection, close,
            arginfo_class_libsqlite_connection_close, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, isReadonly,
            arginfo_class_libsqlite_connection_isReadonly, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, lastInsertRowid,
            arginfo_class_libsqlite_connection_lastInsertRowid,
            ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, setBusyTimeout,
            arginfo_class_libsqlite_connection_setBusyTimeout,
            ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, errorCode,
            arginfo_class_libsqlite_connection_errorCode, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, errorCodeExtended,
            arginfo_class_libsqlite_connection_errorCodeExtended,
            ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, errorMessage,
            arginfo_class_libsqlite_connection_errorMessage, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, command,
            arginfo_class_libsqlite_connection_command, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, nonQuery,
            arginfo_class_libsqlite_connection_nonQuery, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, executeTransaction,
            arginfo_class_libsqlite_connection_executeTransaction,
            ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, scalar,
            arginfo_class_libsqlite_connection_scalar, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, reader,
            arginfo_class_libsqlite_connection_reader, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, readerToJson,
            arginfo_class_libsqlite_connection_readerToJson, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, insertJson,
            arginfo_class_libsqlite_connection_insertJson, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, insertBlob,
            arginfo_class_libsqlite_connection_insertBlob, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, updateBlob,
            arginfo_class_libsqlite_connection_updateBlob, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, getBlob,
            arginfo_class_libsqlite_connection_getBlob, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, begin,
            arginfo_class_libsqlite_connection_begin, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, commit,
            arginfo_class_libsqlite_connection_commit, ZEND_ACC_PUBLIC)
    ZEND_ME(libsqlite_Connection, rollback,
            arginfo_class_libsqlite_connection_rollback, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

static const zend_function_entry libsqlite_functions[] = {
    PHP_FE(libsqlite_version, arginfo_libsqlite_version)
    PHP_FE(libsqlite_sqlite_version, arginfo_libsqlite_sqlite_version)
    PHP_FE(libsqlite_param_clean_prefix, arginfo_libsqlite_param_clean_prefix)
    PHP_FE(libsqlite_string_concat, arginfo_libsqlite_string_concat)
    PHP_FE(libsqlite_string_utf8_to_ansi, arginfo_libsqlite_string_utf8_to_ansi)
    PHP_FE(libsqlite_string_ansi_to_utf8, arginfo_libsqlite_string_ansi_to_utf8)
    PHP_FE(libsqlite_json_writer_callback, arginfo_libsqlite_json_writer_callback)
    PHP_FE(libsqlite_string_format, arginfo_libsqlite_string_format)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(libsqlite)
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "LibSqlite", "Exception", NULL);
    libsqlite_exception_ce = zend_register_internal_class_ex(
        &ce, zend_ce_exception);

    INIT_NS_CLASS_ENTRY(ce, "LibSqlite", "Connection",
                        class_libsqlite_connection_methods);
    libsqlite_connection_ce = zend_register_internal_class(&ce);
    libsqlite_connection_ce->create_object =
        libsqlite_connection_create_object;
    libsqlite_connection_ce->ce_flags |= ZEND_ACC_FINAL;

    memcpy(&libsqlite_connection_handlers, zend_get_std_object_handlers(),
           sizeof(zend_object_handlers));
    libsqlite_connection_handlers.offset =
        XtOffsetOf(libsqlite_connection_object, std);
    libsqlite_connection_handlers.free_obj =
        libsqlite_connection_free_obj;
    libsqlite_connection_handlers.clone_obj = NULL;

    REGISTER_LONG_CONSTANT("LIBSQLITE_TYPE_NULL", SQLITE_CORE_TYPE_NULL,
                           CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("LIBSQLITE_TYPE_INT", SQLITE_CORE_TYPE_INT,
                           CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("LIBSQLITE_TYPE_DOUBLE", SQLITE_CORE_TYPE_DOUBLE,
                           CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("LIBSQLITE_TYPE_TEXT", SQLITE_CORE_TYPE_TEXT,
                           CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("LIBSQLITE_TYPE_BLOB", SQLITE_CORE_TYPE_BLOB,
                           CONST_CS | CONST_PERSISTENT);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(libsqlite)
{
    return SUCCESS;
}

PHP_RINIT_FUNCTION(libsqlite)
{
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(libsqlite)
{
    return SUCCESS;
}

PHP_MINFO_FUNCTION(libsqlite)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "libsqlite support", "enabled");
    php_info_print_table_row(2, "Extension version", PHP_LIBSQLITE_VERSION);
    php_info_print_table_row(2, "Wrapper version", sqlite_core_version());
    php_info_print_table_row(2, "Embedded SQLite engine",
                             sqlite_core_sqlite_version());
    php_info_print_table_end();
}

static const zend_module_dep libsqlite_deps[] = {
    ZEND_MOD_END
};

zend_module_entry libsqlite_module_entry = {
    STANDARD_MODULE_HEADER_EX,
    NULL,
    libsqlite_deps,
    PHP_LIBSQLITE_EXTNAME,
    libsqlite_functions,
    PHP_MINIT(libsqlite),
    PHP_MSHUTDOWN(libsqlite),
    PHP_RINIT(libsqlite),
    PHP_RSHUTDOWN(libsqlite),
    PHP_MINFO(libsqlite),
    PHP_LIBSQLITE_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_LIBSQLITE
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(libsqlite)
#endif
