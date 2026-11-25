<?php
/**
 * libsqlite3 PHP binding (FFI)
 *
 * Declares the full frozen API (34 symbols from src/sqlite/core/sqlite.def)
 * using PHP's FFI extension. No external packages; only php-cli + FFI.
 *
 * Usage:
 *   php libsqlite.php /path/to/libsqlite3.so   # standalone smoke test
 *   php example_full.php /path/to/libsqlite3.so # canonical 19-step flow
 */

declare(strict_types=1);

/* FFI is a compiled-in extension: detect it with class_exists/extension_loaded,
 * NOT with defined('FFI_SCOPE') — that global constant does not exist in
 * PHP 8.2 (verified: extension_loaded('ffi') is true while defined('FFI_SCOPE')
 * is false), so that check rejected every valid runtime. */
if (!extension_loaded('ffi') || !class_exists('FFI')) {
    /* Exit non-zero: die() alone exits 0, which masks the failure in a
     * container CMD that reports the example's exit code. */
    fwrite(STDERR, "PHP FFI extension is required (php8.x + --enable-ffi)\n");
    exit(2);
}

final class LibSqlite
{
    public FFI $lib;

    /** Allocated strings that must outlive the call (parameter text, etc.). */
    private array $keep = [];

    public function __construct(?string $libPath = null)
    {
        $cdef = <<<'FFI'
typedef int sqlite3;
typedef int sqlite3_stmt;

typedef enum {
    SQLITE_CORE_TYPE_NULL = 0,
    SQLITE_CORE_TYPE_INT = 2,
    SQLITE_CORE_TYPE_DOUBLE = 4,
    SQLITE_CORE_TYPE_TEXT = 6,
    SQLITE_CORE_TYPE_BLOB = 8
} sqlite_core_param_type_t;

typedef enum {
    SQLITE_CORE_DIRECTION_INPUT = 1,
    SQLITE_CORE_DIRECTION_OUTPUT = 2,
    SQLITE_CORE_DIRECTION_INPUTOUTPUT = 3
} sqlite_core_param_direction_t;

typedef struct {
    char *name;
    int no_insensitive;
    int type;
    int Direction;
    union {
        double dVal;
        long long iVal;
        struct {
            char *ptr;
            int len;
            int max_len;
        } blob_or_text;
    } value;
} sqlite_core_param_t;

typedef int (*sqlite_core_row_callback_fn)(
    sqlite3_stmt *stmt,
    int columns,
    int hasrows,
    void *pUserContext
);

/* error */
int sqlite_core_error_code(sqlite3 *db);
int sqlite_core_error_code_extended(sqlite3 *db);
const char *sqlite_core_error_message(sqlite3 *db);

/* string — NOTE: the functions that return heap-owned memory are declared
 * as returning void *, not char *. PHP FFI converts a char* return value
 * into a PHP string automatically, which destroys the pointer before it can
 * be released with sqlite_core_string_free. Declaring void* keeps the
 * pointer so the allocator rule (take_string) can be honoured.
 * Functions returning static library data keep `const char *`: those become
 * plain PHP strings and must NOT be freed. */
void *sqlite_core_string_format(const char *format, ...);
void *sqlite_core_string_concat(const char *a, const char *b);
void sqlite_core_string_free(void *s);
void *sqlite_core_string_utf8_to_ansi(const char *s);
void *sqlite_core_string_ansi_to_utf8(const char *s);

/* cnx */
sqlite3 *sqlite_core_open(const char *path, int *out_rc);
sqlite3 *sqlite_core_open_or_create(const char *path, int *out_rc);
sqlite3 *sqlite_core_open_readonly(const char *path, int *out_rc);
sqlite3 *sqlite_core_open_uri_readonly(const char *path, int *out_rc);
sqlite3 *sqlite_core_open_ex(const char *path, int flags, int *out_rc);
int sqlite_core_close(sqlite3 *db);
int sqlite_core_is_readonly(sqlite3 *db);
long long sqlite_core_last_insert_rowid(sqlite3 *db);
int sqlite_core_set_busy_timeout(sqlite3 *db, int ms);

/* parameter */
const char *sqlite_core_param_clean_prefix(const char *name);

/* command */
int sqlite_core_execute_command(sqlite3 *db, const char *sql, char *err_buf, int err_buf_max);
int sqlite_core_execute_non_query(sqlite3 *db, const char *sql, sqlite_core_param_t *params, int count, char *err_buf, int max);
int sqlite_core_execute_scalar(sqlite3 *db, const char *sql, sqlite_core_param_t *params, int count, sqlite_core_param_t *out, char *err_buf, int max);
int sqlite_core_execute_reader(sqlite3 *db, const char *sql, sqlite_core_param_t *params, int count, sqlite_core_row_callback_fn cb, void *ctx, char *err_buf, int max);
int sqlite_core_execute_transaction(sqlite3 *db, const char *sql, int txexpected, char *err_buf, int max);

/* transaction */
int sqlite_core_begin_transaction(sqlite3 *db, char *err_buf, int max);
int sqlite_core_commit_transaction(sqlite3 *db, char *err_buf, int max);
int sqlite_core_rollback_transaction(sqlite3 *db, char *err_buf, int max);

/* builder */
int sqlite_core_json_writer_callback(sqlite3_stmt *stmt, int columns, int hasrows, void *ctx);
int sqlite_core_execute_reader_to_json(sqlite3 *db, const char *sql, sqlite_core_param_t *params, int count, void **out_json, char *err_buf, int max);
int sqlite_core_insert_json(sqlite3 *db, const char *table, const char *json, char *err_buf, int max);

/* field */
int sqlite_core_get_blob(sqlite3 *db, const char *sql, void *dest, int max_len, int *copied);
int sqlite_core_insert_blob(sqlite3 *db, const char *table, const char *column, long long rowid, const char *file_path, int chunk_size, int *written, char *err, int max);
int sqlite_core_update_blob(sqlite3 *db, const char *table, const char *column, long long rowid, const char *file_path, int chunk_size, int *written, char *err, int max);

/* version */
const char *sqlite_core_version(void);
const char *sqlite_core_sqlite_version(void);
FFI;

        /* FFI::cdef(CODE, LIB) declares the surface and links LIB in one
         * call. FFI::load() is only for preloading a header *file*, and it
         * takes a single argument — passing (path, code) raises
         * ArgumentCountError. */
        $this->lib = FFI::cdef($cdef, $libPath);
        /* A C function pointer type would be written here (int(*)(void*,int,int,void*)),
         * but PHP FFI has no way to *create* one from PHP code — see
         * readerCallbackSupport() below. */
    }

    /* ---- allocator rule helpers ---- */

    /**
     * Read a string the library owns as *static* data (versions,
     * error_message, param_clean_prefix). Those are declared `const char *`,
     * so PHP FFI already returns a PHP string: nothing to free.
     * Accepts either the PHP string or a CData pointer, for safety.
     */
    public function staticString($ptr): ?string
    {
        if ($ptr === null || $ptr === '') {
            return null;
        }
        if (is_string($ptr)) {
            return $ptr;
        }
        return FFI::isNull($ptr) ? null : FFI::string($ptr);
    }

    /**
     * Read a heap-allocated string (SQLite allocator), copy it into a PHP
     * string and release the original with sqlite_core_string_free — never
     * the PHP/GC side. Those functions are declared `void *` in the cdef so
     * the pointer survives long enough to be freed.
     */
    public function takeString(?\FFI\CData $ptr): ?string
    {
        if ($ptr === null || FFI::isNull($ptr)) {
            return null;
        }
        $s = FFI::string($ptr);
        $this->lib->sqlite_core_string_free($ptr);
        return $s;
    }

    /**
     * Keep a PHP string alive so its native pointer stays valid during a C
     * call.
     *
     * `$lib->new('char[N]', false)` makes the buffer NON-owned: FFI refuses
     * to assign an *owned* pointer into a struct's `char *` field
     * ("Attempt to perform assign of owned C pointer"), because the struct
     * could outlive it. A non-owned pointer assigns fine. The buffer is then
     * pinned in $this->keep[] so it survives for the duration of the call;
     * releasing it is not possible through FFI (there is no free() for
     * non-owned CData), so the binding trades a small, bounded leak for
     * correctness — documented in README.md.
     */
    public function keep(string $s): \FFI\CData
    {
        $len = strlen($s);
        $c = $this->lib->new("char[" . ($len + 1) . "]", false);
        /* memcpy exactly the string length: FFI hands the PHP string over
         * as a NUL-terminated char*, and asking for len+1 bytes triggers
         * "Attempt to read over string boundary". Terminate by hand. */
        FFI::memcpy($c, $s, $len);
        $c[$len] = "\0";
        $this->keep[] = $c;
        return $c;
    }

    /* ---- convenience parameter builders ---- */

    public function intParam(string $name, int $value): \FFI\CData
    {
        $p = $this->lib->new("sqlite_core_param_t");
        $p->name = $this->keep($name);
        $p->no_insensitive = 0;
        $p->type = 2; /* SQLITE_CORE_TYPE_INT */
        $p->Direction = 1; /* SQLITE_CORE_DIRECTION_INPUT */
        $p->value->iVal = $value;
        return $p;
    }

    public function textParam(string $name, string $value): \FFI\CData
    {
        $p = $this->lib->new("sqlite_core_param_t");
        $p->name = $this->keep($name);
        $p->no_insensitive = 0;
        $p->type = 6; /* SQLITE_CORE_TYPE_TEXT */
        $p->Direction = 1; /* SQLITE_CORE_DIRECTION_INPUT */
        $ptr = $this->keep($value);
        $p->value->blob_or_text->ptr = $ptr;
        $p->value->blob_or_text->len = -1; /* NUL-terminated */
        $p->value->blob_or_text->max_len = 0;
        return $p;
    }

    /** Build a contiguous sqlite_core_param_t[] array from an array of param CData. */
    public function paramArray(array $params): \FFI\CData
    {
        $n = count($params);
        $arr = $this->lib->new("sqlite_core_param_t[" . $n . "]", false);
        foreach ($params as $i => $p) {
            FFI::memcpy(
                FFI::addr($arr[$i]),
                $p,
                FFI::sizeof($arr[$i])
            );
        }
        $this->keep[] = $arr; /* keep the array alive */
        return $arr;
    }

    /* ---- scalar out buffer helper ---- */

    /** Prepare an output scalar param with a pre-allocated text buffer. */
    public function scalarOutText(int $maxLen = 512): \FFI\CData
    {
        $p = $this->lib->new("sqlite_core_param_t");
        $p->type = 6; /* TEXT */
        $p->Direction = 2; /* OUTPUT */
        $buf = $this->lib->new("char[" . $maxLen . "]");
        $p->value->blob_or_text->ptr = $buf;
        $p->value->blob_or_text->len = 0;
        $p->value->blob_or_text->max_len = $maxLen;
        $this->keep[] = $buf;
        return $p;
    }

    /** Read the text result from a scalar out param. */
    public function scalarOutTextValue(\FFI\CData $p): ?string
    {
        $ptr = $p->value->blob_or_text->ptr;
        $len = $p->value->blob_or_text->len;
        if ($ptr === null || $len <= 0) {
            return null;
        }
        return FFI::string($ptr, $len);
    }

    /* ---- reader callback: a documented limitation ---- */

    /**
     * PHP FFI CANNOT turn a PHP closure into a C function pointer: the
     * extension has no callback mechanism at all (the complete method list
     * is cdef, load, scope, new, free, cast, type, typeof, arrayType, addr,
     * sizeof, alignof, memcpy, memcmp, memset, string, isNull — verified on
     * PHP 8.2, and there is no FFI::callback()).
     *
     * `sqlite_core_execute_reader` therefore cannot be driven from PHP, and
     * `sqlite_core_json_writer_callback` — although declared here, like every
     * other frozen symbol — cannot be handed to it either, because there is
     * no way to take the address of a symbol bound through FFI.
     *
     * This is the same limitation the Java, C#, Go, C++ and Kotlin bindings
     * hit and document: the callback is bound as part of the frozen surface,
     * but the reader is exercised through the exported
     * `sqlite_core_execute_reader_to_json`, which is the documented path on
     * a shared build.
     *
     * Callers that need a real row callback must run a C trampoline, the way
     * the R and Swift bindings do.
     */
    public function readerCallbackSupport(): array
    {
        return [
            'php_callbacks' => false,
            'execute_reader' => 'unavailable from PHP (needs a C callback)',
            'reader_path' => 'sqlite_core_execute_reader_to_json',
        ];
    }
}
