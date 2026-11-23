// libsqlite3 binding for C++ (header-only).
//
// Complete surface of the frozen public API: the 34 symbols exported by
// src/sqlite/core/sqlite.def. No compilation of the binding itself is
// needed: include this header and link against libsqlite3
// (libsqlite3.dll / libsqlite3.so / libsqlite3.dylib).
//
// The declarations below are kept one-to-one with src/sqlite/core/sqlite.def
// (checked by bindings/verify_symbols.py) and with the layout of
// sqlite_core_param_t: the static_asserts at the bottom of the file fail
// the build if either drifts.

#ifndef LIBSQLITE_HPP
#define LIBSQLITE_HPP

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

extern "C" {

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
typedef long long sqlite3_int64;

/* return codes used by the whole API */
#define SQLITE_OK 0
#define SQLITE_ERROR 1
#define SQLITE_MISUSE 21
#define SQLITE_CANTOPEN 14

#define SQLITE_CORE_BUFFER_TOO_SMALL (-2)
#define SQLITE_CORE_BLOB_TOO_SMALL (-3)

/* parameter value types (sqlite_core_param_t:type) */
#define SQLITE_CORE_TYPE_NULL 0
#define SQLITE_CORE_TYPE_INT 2
#define SQLITE_CORE_TYPE_DOUBLE 4
#define SQLITE_CORE_TYPE_TEXT 6
#define SQLITE_CORE_TYPE_BLOB 8

/* parameter directions (sqlite_core_param_t:Direction) */
#define SQLITE_CORE_DIRECTION_INPUT 1
#define SQLITE_CORE_DIRECTION_OUTPUT 2
#define SQLITE_CORE_DIRECTION_INPUTOUTPUT 3

/* union { double dVal; sqlite3_int64 iVal;
 *         struct { char *ptr; int len; int max_len; } blob_or_text; } value; */
typedef union sqlite_core_param_value {
    double dVal;
    sqlite3_int64 iVal;
    struct {
        char *ptr;
        int len;
        int max_len;
    } blob_or_text;
} sqlite_core_param_value;

typedef struct sqlite_core_param_t {
    const char *name;
    int no_insensitive;
    int type;
    int Direction;
    sqlite_core_param_value value;
} sqlite_core_param_t;

typedef int (*sqlite_core_row_callback_fn)(
    sqlite3_stmt *stmt,
    int columns,
    int hasrows,
    void *pUserContext);

/* --- the 34 exported symbols (src/sqlite/core/sqlite.def) ------------- */

const char *sqlite_core_version();

const char *sqlite_core_sqlite_version();

int sqlite_core_error_code(sqlite3 *db);

int sqlite_core_error_code_extended(sqlite3 *db);

const char *sqlite_core_error_message(sqlite3 *db);

char *sqlite_core_string_format(const char *format, ...);

char *sqlite_core_string_concat(const char *str1, const char *str2);

void sqlite_core_string_free(const char *input);

char *sqlite_core_string_utf8_to_ansi(const char *utf8_str);

char *sqlite_core_string_ansi_to_utf8(const char *ansi_str);

sqlite3 *sqlite_core_open_ex(const char *uripath, int flags, int *out_rc);

sqlite3 *sqlite_core_open_readonly(const char *uripath, int *out_rc);

sqlite3 *sqlite_core_open_or_create(const char *uripath, int *out_rc);

sqlite3 *sqlite_core_open(const char *uripath, int *out_rc);

sqlite3 *sqlite_core_open_uri_readonly(const char *uripath, int *out_rc);

int sqlite_core_close(sqlite3 *db);

int sqlite_core_is_readonly(sqlite3 *db);

sqlite3_int64 sqlite_core_last_insert_rowid(sqlite3 *db);

int sqlite_core_set_busy_timeout(sqlite3 *db, int ms);

const char *sqlite_core_param_clean_prefix(const char *name);

int sqlite_core_execute_command(
    sqlite3 *db,
    const char *sql_command,
    char *err_buf,
    int err_buf_max);

int sqlite_core_execute_transaction(
    sqlite3 *db,
    const char *sql_command,
    int txexpected,
    char *err_buf,
    int err_buf_max);

int sqlite_core_execute_non_query(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    char *err_buf,
    int err_buf_max);

int sqlite_core_execute_scalar(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    sqlite_core_param_t *out_scalar_result,
    char *err_buf,
    int err_buf_max);

int sqlite_core_execute_reader(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    sqlite_core_row_callback_fn reader_callback,
    void *pUserContext,
    char *err_buf,
    int err_buf_max);

int sqlite_core_begin_transaction(sqlite3 *db, char *err_buf, int err_buf_max);

int sqlite_core_commit_transaction(sqlite3 *db, char *err_buf, int err_buf_max);

int sqlite_core_rollback_transaction(sqlite3 *db, char *err_buf, int err_buf_max);

int sqlite_core_json_writer_callback(
    sqlite3_stmt *stmt,
    int columns,
    int hasrows,
    void *pUserContext);

int sqlite_core_execute_reader_to_json(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    char **out_json,
    char *err_buf,
    int err_buf_max);

int sqlite_core_insert_json(
    sqlite3 *db,
    const char *table_name,
    const char *json_str,
    char *err_buf,
    int err_buf_max);

int sqlite_core_get_blob(
    sqlite3 *db,
    const char *sql,
    void *pDestBuffer,
    int maxLen,
    int *pOutCopied);

int sqlite_core_insert_blob(
    sqlite3 *db,
    const char *table,
    const char *column,
    sqlite3_int64 rowid,
    const char *file_path,
    int chunk_size,
    int *p_out_written,
    char *err_buf,
    int err_buf_max);

int sqlite_core_update_blob(
    sqlite3 *db,
    const char *table,
    const char *column,
    sqlite3_int64 rowid,
    const char *file_path,
    int chunk_size,
    int *p_out_written,
    char *err_buf,
    int err_buf_max);

} /* extern "C" */

namespace libsqlite {

/* ------------------------------------------------------------------ */
/* ABI guards: fail the build if the frozen layout ever drifts          */
/* (40 bytes on LP64, 28 on ILP32: 4 ints after the name pointer, then  */
/* the union of double / int64 / {ptr, len, max_len})                   */
/* ------------------------------------------------------------------ */

static_assert(sizeof(sqlite3_int64) == 8, "sqlite3_int64 must be 64-bit");
static_assert(sizeof(sqlite_core_param_t) == 40 || sizeof(sqlite_core_param_t) == 28,
              "sqlite_core_param_t layout drift (expected 40 on LP64, 28 on ILP32)");
static_assert(offsetof(sqlite_core_param_t, name) == 0,
              "sqlite_core_param_t: name must stay first");
static_assert(offsetof(sqlite_core_param_t, value) >= 16,
              "sqlite_core_param_t: the union must follow the four leading fields");

/* ------------------------------------------------------------------ */
/* C++ layer                                                           */
/* ------------------------------------------------------------------ */

using RowCallback = sqlite_core_row_callback_fn;

inline constexpr int OK = SQLITE_OK;
inline constexpr int MISUSE = SQLITE_MISUSE;
inline constexpr int CANTOPEN = SQLITE_CANTOPEN;
inline constexpr int BUFFER_TOO_SMALL = SQLITE_CORE_BUFFER_TOO_SMALL;
inline constexpr int BLOB_TOO_SMALL = SQLITE_CORE_BLOB_TOO_SMALL;

/* Reads a char* owned by the library's static data (version,
 * error_message, param_clean_prefix): never free it. */
inline std::string static_string(const char *p)
{
    return p == nullptr ? std::string() : std::string(p);
}

/* Reads a char* allocated by the SQLite allocator and releases it with
 * sqlite_core_string_free (never with free/delete). */
inline std::string take_string(char *p)
{
    if (p == nullptr) {
        return std::string();
    }
    std::string out(p);
    sqlite_core_string_free(p);
    return out;
}

/* Named parameters. The C struct keeps *native pointers* (name and, for
 * text, blob_or_text.ptr), so the C++ builder owns the strings and hands
 * out a contiguous sqlite_core_param_t[] once the set is complete. */
class ParamSet {
public:
    ParamSet &text(const std::string &name, const std::string &value)
    {
        // The value pointer is captured *before* add() pushes its own name
        // copy: keep_.back() inside the lambda would otherwise point at the
        // name, not the value.
        keep_.push_back(std::make_shared<std::string>(value));
        char *value_ptr = const_cast<char *>(keep_.back()->c_str());
        return add(name, SQLITE_CORE_TYPE_TEXT, [value_ptr](sqlite_core_param_t &p) {
            p.value.blob_or_text.ptr = value_ptr;
            p.value.blob_or_text.len = -1;   /* NUL-terminated UTF-8 */
            p.value.blob_or_text.max_len = 0;
        });
    }

    ParamSet &integer(const std::string &name, sqlite3_int64 value)
    {
        return add(name, SQLITE_CORE_TYPE_INT, [value](sqlite_core_param_t &p) {
            p.value.iVal = value;
        });
    }

    ParamSet &floating(const std::string &name, double value)
    {
        return add(name, SQLITE_CORE_TYPE_DOUBLE, [value](sqlite_core_param_t &p) {
            p.value.dVal = value;
        });
    }

    ParamSet &null_value(const std::string &name)
    {
        return add(name, SQLITE_CORE_TYPE_NULL, [](sqlite_core_param_t &) {});
    }

    sqlite_core_param_t *data() { return items_.data(); }
    const sqlite_core_param_t *data() const { return items_.data(); }
    int count() const { return static_cast<int>(items_.size()); }

private:
    template <typename Fill>
    ParamSet &add(const std::string &name, int type, Fill fill)
    {
        keep_.push_back(std::make_shared<std::string>(name));
        sqlite_core_param_t p;
        p.name = keep_.back()->c_str();
        p.no_insensitive = 0;
        p.type = type;
        p.Direction = SQLITE_CORE_DIRECTION_INPUT;
        p.value.blob_or_text.ptr = nullptr;
        p.value.blob_or_text.len = 0;
        p.value.blob_or_text.max_len = 0;
        fill(p);
        items_.push_back(p);
        return *this;
    }

    std::vector<sqlite_core_param_t> items_;
    std::vector<std::shared_ptr<std::string>> keep_;
};

/* RAII connection handle. */
class Connection {
public:
    Connection() = default;
    explicit Connection(sqlite3 *handle) : db_(handle) {}

    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    Connection(Connection &&other) noexcept : db_(other.db_)
    {
        other.db_ = nullptr;
    }

    Connection &operator=(Connection &&other) noexcept
    {
        if (this != &other) {
            close();
            db_ = other.db_;
            other.db_ = nullptr;
        }
        return *this;
    }

    ~Connection() { close(); }

    sqlite3 *handle() const { return db_; }
    bool open() const { return db_ != nullptr; }
    explicit operator bool() const { return db_ != nullptr; }

    static Connection open(const std::string &path, int *rc = nullptr)
    {
        /* out_rc is NULL-safe in the C API */
        return Connection(sqlite_core_open(path.c_str(), rc));
    }

    static Connection open_ex(const std::string &path, int flags, int *rc)
    {
        return Connection(sqlite_core_open_ex(path.c_str(), flags, rc));
    }

    static Connection open_readonly(const std::string &path, int *rc)
    {
        return Connection(sqlite_core_open_readonly(path.c_str(), rc));
    }

    static Connection open_or_create(const std::string &path, int *rc)
    {
        return Connection(sqlite_core_open_or_create(path.c_str(), rc));
    }

    static Connection open_uri_readonly(const std::string &path, int *rc)
    {
        return Connection(sqlite_core_open_uri_readonly(path.c_str(), rc));
    }

    int close()
    {
        if (db_ == nullptr) {
            return 0;
        }
        int rc = sqlite_core_close(db_);
        db_ = nullptr;
        return rc;
    }

    /* DDL/DML text (one or more statements allowed). */
    int command(const std::string &sql, std::string *err = nullptr)
    {
        char buf[512];
        int rc = sqlite_core_execute_command(db_, sql.c_str(), buf, sizeof(buf));
        if (err != nullptr) {
            *err = rc == SQLITE_OK ? std::string() : std::string(buf);
        }
        return rc;
    }

    /* Guarded transaction command (txexpected 1 = begin, 0 = commit /
     * rollback). */
    int transaction_command(const std::string &sql, int txexpected,
                            std::string *err = nullptr)
    {
        char buf[512];
        int rc = sqlite_core_execute_transaction(db_, sql.c_str(), txexpected,
                                                 buf, sizeof(buf));
        if (err != nullptr) {
            *err = rc == 1 ? std::string() : std::string(buf);
        }
        return rc;
    }

    bool begin(std::string *err = nullptr)
    {
        return transaction_command("BEGIN IMMEDIATE", 1, err) == 1;
    }

    bool commit(std::string *err = nullptr)
    {
        return transaction_command("COMMIT", 0, err) == 1;
    }

    bool rollback(std::string *err = nullptr)
    {
        return transaction_command("ROLLBACK", 0, err) == 1;
    }

    int non_query(const std::string &sql, ParamSet *params, std::string *err = nullptr)
    {
        char buf[512];
        char *keep = buf;
        keep[0] = '\0';
        int rc = sqlite_core_execute_non_query(
            db_, sql.c_str(), params == nullptr ? nullptr : params->data(),
            params == nullptr ? 0 : params->count(), keep, sizeof(buf));
        if (err != nullptr) {
            *err = rc < 0 ? std::string(keep) : std::string();
        }
        return rc;
    }

    int non_query(const std::string &sql, std::string *err = nullptr)
    {
        return non_query(sql, nullptr, err);
    }

    /* ExecuteScalar: the C out struct doubles as the output, so a TEXT
     * result needs a destination buffer (the library only writes when
     * blob_or_text.ptr/max_len fit). Use scalar_text() for that case. */
    bool scalar(const std::string &sql, ParamSet *params, sqlite_core_param_t &out,
                int *type_out = nullptr, std::string *err = nullptr)
    {
        std::memset(&out, 0, sizeof(out));
        out.type = SQLITE_CORE_TYPE_INT;
        return scalar_impl(sql, params, out, type_out, err);
    }

    sqlite3_int64 scalar_int(const std::string &sql, std::string *err = nullptr)
    {
        sqlite_core_param_t out;
        bool ok = scalar(sql, nullptr, out, nullptr, err);
        return ok ? out.value.iVal : 0;
    }

    /* Same call with a pre-allocated buffer for the text case; type_out
     * reports SQLITE_CORE_TYPE_* of what the query produced. */
    bool scalar_text(const std::string &sql, ParamSet *params, std::string &value,
                     int *type_out = nullptr, std::string *err = nullptr)
    {
        sqlite_core_param_t out;
        std::memset(&out, 0, sizeof(out));
        out.type = SQLITE_CORE_TYPE_TEXT;
        std::vector<char> buffer(512, '\0');
        out.value.blob_or_text.ptr = buffer.data();
        out.value.blob_or_text.max_len = static_cast<int>(buffer.size());
        int type = SQLITE_CORE_TYPE_NULL;
        bool ok = scalar_impl(sql, params, out, &type, err);
        value = (ok && type == SQLITE_CORE_TYPE_TEXT)
                    ? std::string(out.value.blob_or_text.ptr,
                                  static_cast<std::size_t>(out.value.blob_or_text.len))
                    : std::string();
        if (type_out != nullptr) {
            *type_out = type;
        }
        return ok;
    }

/* Reader: one call of reader_callback per row (hasrows == 0 marks an
     * empty result set). Returns the row count or a negative SQLite
     * code. */
    int read_rows(const std::string &sql, ParamSet *params, RowCallback cb,
                  void *ctx, std::string *err = nullptr)
    {
        char buf[512];
        char *keep = buf;
        keep[0] = '\0';
        int rc = sqlite_core_execute_reader(
            db_, sql.c_str(), params == nullptr ? nullptr : params->data(),
            params == nullptr ? 0 : params->count(), cb, ctx, keep,
            sizeof(buf));
        if (err != nullptr) {
            *err = rc < 0 ? std::string(keep) : std::string();
        }
        return rc;
    }

    /* Result set as a JSON array (heap string: released with
     * sqlite_core_string_free here). rows receives the row count (or a
     * negative SQLite code on failure). */
    std::string reader_json(const std::string &sql, ParamSet *params, int *rows,
                            std::string *err = nullptr)
    {
        *rows = 0;
        char *json = nullptr;
        char buf[512];
        char *keep = buf;
        keep[0] = '\0';
        int rc = sqlite_core_execute_reader_to_json(
            db_, sql.c_str(), params == nullptr ? nullptr : params->data(),
            params == nullptr ? 0 : params->count(), &json, keep,
            sizeof(buf));
        if (err != nullptr) {
            *err = rc < 0 ? std::string(keep) : std::string();
        }
        std::string out = take_string(json);
        *rows = rc;
        return out;
    }

    /* JSON array of objects -> INSERT (runs in one transaction when no
     * transaction is active). */
    int insert_json(const std::string &table, const std::string &json,
                    std::string *err = nullptr)
    {
        char buf[512];
        char *keep = buf;
        keep[0] = '\0';
        int rc = sqlite_core_insert_json(db_, table.c_str(), json.c_str(), keep,
                                         sizeof(buf));
        if (err != nullptr) {
            *err = rc < 0 ? std::string(keep) : std::string();
        }
        return rc;
    }

    /* BLOB read into a destination buffer (grows it on
     * SQLITE_CORE_BUFFER_TOO_SMALL, so the caller only passes the size). */
    std::vector<unsigned char> get_blob(const std::string &sql, int *rc_out,
                                       int *copied_out = nullptr)
    {
        int cap = 4096;
        std::vector<unsigned char> buf(static_cast<std::size_t>(cap));
        int copied = 0;
        int rc = SQLITE_OK;
        for (;;) {
            copied = 0;
            rc = sqlite_core_get_blob(db_, sql.c_str(), buf.data(), cap, &copied);
            if (rc == SQLITE_CORE_BUFFER_TOO_SMALL && cap < (64 * 1024 * 1024)) {
                cap *= 2;
                buf.resize(static_cast<std::size_t>(cap));
                continue;
            }
            break;
        }
        if (rc_out != nullptr) {
            *rc_out = rc;
        }
        if (copied_out != nullptr) {
            *copied_out = copied;
        }
        if (rc != SQLITE_OK || copied <= 0) {
            return std::vector<unsigned char>();
        }
        buf.resize(static_cast<std::size_t>(copied));
        return buf;
    }

    /* Streaming file -> BLOB column. rowid == 0 inserts a new row. */
    int insert_blob(const std::string &table, const std::string &column,
                    sqlite3_int64 rowid, const std::string &file, int chunk,
                    int *written, std::string *err = nullptr)
    {
        char buf[512];
        char *keep = buf;
        keep[0] = '\0';
        int out = 0;
        int rc = sqlite_core_insert_blob(db_, table.c_str(), column.c_str(), rowid,
                                         file.c_str(), chunk, &out, keep,
                                         sizeof(buf));
        if (written != nullptr) {
            *written = out;
        }
        if (err != nullptr) {
            *err = rc < 0 ? std::string(keep) : std::string();
        }
        return rc;
    }

    int update_blob(const std::string &table, const std::string &column,
                    sqlite3_int64 rowid, const std::string &file, int chunk,
                    int *written, std::string *err = nullptr)
    {
        char buf[512];
        char *keep = buf;
        keep[0] = '\0';
        int out = 0;
        int rc = sqlite_core_update_blob(db_, table.c_str(), column.c_str(), rowid,
                                         file.c_str(), chunk, &out, keep,
                                         sizeof(buf));
        if (written != nullptr) {
            *written = out;
        }
        if (err != nullptr) {
            *err = rc < 0 ? std::string(keep) : std::string();
        }
        return rc;
    }

    /* connection info / errors */
    bool is_readonly() const { return sqlite_core_is_readonly(db_) == 1; }
    sqlite3_int64 last_insert_rowid() const
    {
        return sqlite_core_last_insert_rowid(db_);
    }
    int set_busy_timeout(int ms) const
    {
        return sqlite_core_set_busy_timeout(db_, ms);
    }
    int error_code() const { return sqlite_core_error_code(db_); }
    int error_code_extended() const { return sqlite_core_error_code_extended(db_); }
    std::string error_message() const
    {
        return static_string(sqlite_core_error_message(db_));
    }

private:
    bool scalar_impl(const std::string &sql, ParamSet *params,
                     sqlite_core_param_t &out, int *type_out, std::string *err)
    {
        char buf[512];
        char *keep = buf;
        keep[0] = '\0';
        int rc = sqlite_core_execute_scalar(db_, sql.c_str(),
                                            params == nullptr ? nullptr : params->data(),
                                            params == nullptr ? 0 : params->count(),
                                            &out, keep, sizeof(buf));
        if (err != nullptr) {
            *err = rc == SQLITE_OK ? std::string() : std::string(keep);
        }
        if (type_out != nullptr) {
            *type_out = out.type;
        }
        return rc == SQLITE_OK;
    }

    sqlite3 *db_ = nullptr;
};

} /* namespace libsqlite */

#endif /* LIBSQLITE_HPP */