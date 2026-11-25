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

ZEND_DECLARE_MODULE_GLOBALS(libsqlite)

/* ------------------------------------------------------------------ */
/* the connection object                                               */
/* ------------------------------------------------------------------ */

typedef struct _libsqlite_connection_object {
    sqlite3 *db;
    zend_bool_t closed;
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
    zend_bool_t aborted;
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

static const zend_function_entry libsqlite_functions[] = {
    PHP_FE(libsqlite_version, arginfo_libsqlite_version)
    PHP_FE(libsqlite_sqlite_version, arginfo_libsqlite_sqlite_version)
    PHP_FE(libsqlite_param_clean_prefix, arginfo_libsqlite_param_clean_prefix)
    PHP_FE(libsqlite_string_concat, arginfo_libsqlite_string_concat)
    PHP_FE(libsqlite_string_utf8_to_ansi, arginfo_libsqlite_string_utf8_to_ansi)
    PHP_FE(libsqlite_string_ansi_to_utf8, arginfo_libsqlite_string_ansi_to_utf8)
    PHP_FE(libsqlite_json_writer_callback, arginfo_libsqlite_json_writer_callback)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(libsqlite)
{
    zend_class_entry ce;

    INIT_NS_CLASS_ENTRY(ce, "LibSqlite", "Exception", NULL);
    libsqlite_exception_ce = zend_register_internal_class_ex(
        &ce, zend_ce_exception);

    INIT_NS_CLASS_ENTRY(ce, "LibSqlite", "Connection",
                        class_LibSqlite_Connection_methods);
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