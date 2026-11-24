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

if (!defined('FFI_SCOPE')) {
    die("PHP FFI extension is required (php8.x + --enable-ffi)\n");
}

final class LibSqlite
{
    public FFI $lib;

    /** Allocated strings that must outlive the call (parameter text, etc.). */
    private array $keep = [];

    /** C callback type for sqlite_core_row_callback_fn. */
    private \FFI\CData $rowCallbackType;

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

/* string */
char *sqlite_core_string_format(const char *format, ...);
char *sqlite_core_string_concat(const char *a, const char *b);
void sqlite_core_string_free(const char *s);
char *sqlite_core_string_utf8_to_ansi(const char *s);
char *sqlite_core_string_ansi_to_utf8(const char *s);

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
int sqlite_core_execute_reader_to_json(sqlite3 *db, const char *sql, sqlite_core_param_t *params, int count, char **out_json, char *err_buf, int max);
int sqlite_core_insert_json(sqlite3 *db, const char *table, const char *json, char *err_buf, int max);

/* field */
int sqlite_core_get_blob(sqlite3 *db, const char *sql, void *dest, int max_len, int *copied);
int sqlite_core_insert_blob(sqlite3 *db, const char *table, const char *column, long long rowid, const char *file_path, int chunk_size, int *written, char *err, int max);
int sqlite_core_update_blob(sqlite3 *db, const char *table, const char *column, long long rowid, const char *file_path, int chunk_size, int *written, char *err, int max);

/* version */
const char *sqlite_core_version(void);
const char *sqlite_core_sqlite_version(void);
FFI;

        $this->lib = $libPath ? FFI::load($libPath, $cdef) : FFI::cdef($cdef);
        $this->rowCallbackType = FFI::type(
            'int function(FFI\CData, int, int, FFI\CData): int'
        );
    }

    /* ---- allocator rule helpers ---- */

    /** Read a static (library-owned) char* and return a PHP string (no free). */
    public function staticString(\FFI\CData $ptr): ?string
    {
        if ($ptr === null || $ptr->cdata === null) {
            return null;
        }
        return FFI::string($ptr);
    }

    /** Read a heap-allocated char* (SQLite allocator), copy it, and free it. */
    public function takeString(\FFI\CData $ptr): ?string
    {
        if ($ptr === null || $ptr->cdata === null) {
            return null;
        }
        $s = FFI::string($ptr);
        $this->lib->sqlite_core_string_free($ptr);
        return $s;
    }

    /** Keep a PHP string alive so its native pointer stays valid during a C call. */
    public function keep(string $s): \FFI\CData
    {
        $c = FFI::new("char[]", strlen($s) + 1);
        FFI::memcpy($c, $s, strlen($s));
        $c[strlen($s)] = "\0";
        $this->keep[] = $c;
        return $c;
    }

    /* ---- convenience parameter builders ---- */

    public function intParam(string $name, int $value): \FFI\CData
    {
        $p = FFI::new("sqlite_core_param_t");
        $p->name = $this->keep($name);
        $p->no_insensitive = 0;
        $p->type = 2; /* SQLITE_CORE_TYPE_INT */
        $p->Direction = 1; /* SQLITE_CORE_DIRECTION_INPUT */
        $p->value->iVal = $value;
        return $p;
    }

    public function textParam(string $name, string $value): \FFI\CData
    {
        $p = FFI::new("sqlite_core_param_t");
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
        $arr = FFI::new("sqlite_core_param_t[" . $n . "]");
        foreach ($params as $i => $p) {
            FFI::memcpy(FFI::addr($arr[$i]), $p, FFI::sizeof("sqlite_core_param_t"));
        }
        $this->keep[] = $arr; /* keep the array alive */
        return $arr;
    }

    /* ---- scalar out buffer helper ---- */

    /** Prepare an output scalar param with a pre-allocated text buffer. */
    public function scalarOutText(int $maxLen = 512): \FFI\CData
    {
        $p = FFI::new("sqlite_core_param_t");
        $p->type = 6; /* TEXT */
        $p->Direction = 2; /* OUTPUT */
        $buf = FFI::new("char[" . $maxLen . "]");
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

    /* ---- row callback factory ---- */

    /** Create a C callable row callback that increments a PHP counter. */
    public function countRowsCallback(int &$counter): \FFI\CData
    {
        return FFI::callback(
            $this->rowCallbackType,
            function (\FFI\CData $stmt, int $columns, int $hasrows, \FFI\CData $ctx) use (&$counter): int {
                if ($hasrows) {
                    $counter++;
                }
                return 0;
            }
        );
    }
}
