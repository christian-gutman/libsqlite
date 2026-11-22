// Package libsqlite binds the frozen libsqlite3 public API (34 symbols)
// for Go through cgo.
//
// Build:  cgo LDFLAGS point at the built library, e.g.
//   CGO_LDFLAGS="-L../bin/x64-Linux/shared/Release -llibsqlite3" go run example_full.go
package main

/*
#cgo LDFLAGS: -llibsqlite3
#include <stdint.h>

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

typedef struct {
	const char *name;
	int no_insensitive;
	int type;
	int Direction;
	union {
		double dVal;
		long long iVal;
		struct { char *ptr; int len; int max_len; } blob_or_text;
	} value;
} sqlite_core_param_t;

typedef int (*sqlite_core_row_callback_fn)(sqlite3_stmt *, int, int, void *);

const char *sqlite_core_version();
const char *sqlite_core_sqlite_version();
int sqlite_core_error_code(sqlite3 *);
int sqlite_core_error_code_extended(sqlite3 *);
const char *sqlite_core_error_message(sqlite3 *);
char *sqlite_core_string_format(const char *, ...);
char *sqlite_core_string_concat(const char *, const char *);
void sqlite_core_string_free(const char *);
char *sqlite_core_string_utf8_to_ansi(const char *);
char *sqlite_core_string_ansi_to_utf8(const char *);
sqlite3 *sqlite_core_open(const char *, int *);
sqlite3 *sqlite_core_open_ex(const char *, int, int *);
sqlite3 *sqlite_core_open_readonly(const char *, int *);
sqlite3 *sqlite_core_open_or_create(const char *, int *);
sqlite3 *sqlite_core_open_uri_readonly(const char *, int *);
int sqlite_core_close(sqlite3 *);
int sqlite_core_is_readonly(sqlite3 *);
long long sqlite_core_last_insert_rowid(sqlite3 *);
int sqlite_core_set_busy_timeout(sqlite3 *, int);
const char *sqlite_core_param_clean_prefix(const char *);
int sqlite_core_execute_command(sqlite3 *, const char *, char *, int);
int sqlite_core_execute_transaction(sqlite3 *, const char *, int, char *, int);
int sqlite_core_execute_non_query(sqlite3 *, const char *, sqlite_core_param_t *, int, char *, int);
int sqlite_core_execute_scalar(sqlite3 *, const char *, sqlite_core_param_t *, int, sqlite_core_param_t *, char *, int);
int sqlite_core_execute_reader(sqlite3 *, const char *, sqlite_core_param_t *, int, sqlite_core_row_callback_fn, void *, char *, int);
int sqlite_core_begin_transaction(sqlite3 *, char *, int);
int sqlite_core_commit_transaction(sqlite3 *, char *, int);
int sqlite_core_rollback_transaction(sqlite3 *, char *, int);
int sqlite_core_json_writer_callback(sqlite3_stmt *, int, int, void *);
int sqlite_core_execute_reader_to_json(sqlite3 *, const char *, sqlite_core_param_t *, int, char **, char *, int);
int sqlite_core_insert_json(sqlite3 *, const char *, const char *, char *, int);
int sqlite_core_get_blob(sqlite3 *, const char *, void *, int, int *);
int sqlite_core_insert_blob(sqlite3 *, const char *, const char *, long long, const char *, int, int *, char *, int);
int sqlite_core_update_blob(sqlite3 *, const char *, const char *, long long, const char *, int, int *, char *, int);

/* Helpers: cgo cannot touch the fields of a C union directly. */
static void param_set_int(sqlite_core_param_t *p, const char *name, long long v)
{
	memset(p, 0, sizeof(*p));
	p->name = name;
	p->type = 2;  /* SQLITE_CORE_TYPE_INT */
	p->Direction = 1;
	p->value.iVal = v;
}

static void param_set_text(sqlite_core_param_t *p, const char *name, const char *v)
{
	memset(p, 0, sizeof(*p));
	p->name = name;
	p->type = 6;  /* SQLITE_CORE_TYPE_TEXT */
	p->Direction = 1;
	p->value.blob_or_text.ptr = (char *)v;
	p->value.blob_or_text.len = -1;
}

static long long param_get_int(const sqlite_core_param_t *p)
{
	return p->value.iVal;
}
*/
import "C"

// Constants (mirror sqlite_core.h).
const (
	SQLITE_OK        = 0
	SQLITE_MISUSE    = 21
	SQLITE_CANTOPEN  = 14
	TYPE_NULL        = 0
	TYPE_INT         = 2
	TYPE_DOUBLE      = 4
	TYPE_TEXT        = 6
	TYPE_BLOB        = 8
	DIRECTION_INPUT  = 1
	DIRECTION_OUTPUT = 2
	BLOB_TOO_SMALL   = -3
)