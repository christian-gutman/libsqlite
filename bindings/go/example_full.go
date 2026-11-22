// Canonical libsqlite3 flow for Go — single file, self-contained.
//
// It declares the frozen 34-symbol surface in the cgo preamble (plus the
// union helpers cgo needs) and implements the canonical example. Merge
// everything into one file on purpose: it works on every cgo version,
// with no cross-file C name resolution.
//
// Run (Linux):
//   CGO_LDFLAGS="-L../bin/x64-Linux/shared/Release -llibsqlite3" \
//     LD_LIBRARY_PATH=../bin/x64-Linux/shared/Release go run .
//
// Run (Windows with the MSYS2/MinGW gcc):
//   CGO_LDFLAGS="-L../bin/x64-Windows/shared/Release -llibsqlite3" \
//     PATH="../bin/x64-Windows/shared/Release;$PATH" go run .

package main

/*
#cgo LDFLAGS: -l:libsqlite3.so
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

// Helpers: cgo cannot touch the fields of a C union directly.
static void param_set_int(sqlite_core_param_t *p, const char *name, long long v)
{
	memset(p, 0, sizeof(*p));
	p->name = name;
	p->type = 2;  // SQLITE_CORE_TYPE_INT
	p->Direction = 1;
	p->value.iVal = v;
}

static void param_set_text(sqlite_core_param_t *p, const char *name, const char *v)
{
	memset(p, 0, sizeof(*p));
	p->name = name;
	p->type = 6;  // SQLITE_CORE_TYPE_TEXT
	p->Direction = 1;
	p->value.blob_or_text.ptr = (char *)v;
	p->value.blob_or_text.len = -1;
}

static long long param_get_int(const sqlite_core_param_t *p)
{
	return p->value.iVal;
}

// Row-counter callback: a C trampoline avoids //export (which does not
// work with 'go run' of a package) and keeps Go pointers out of C.
static long long libsqlite_rows_seen = 0;
static int libsqlite_rows_tramp(sqlite3_stmt *stmt, int cols, int has, void *ctx)
{
	(void)stmt; (void)cols; (void)ctx;
	if (has != 0) libsqlite_rows_seen++;
	return 0;
}
static sqlite_core_row_callback_fn libsqlite_rows_cb(void)
{
	return &libsqlite_rows_tramp;
}
static void libsqlite_rows_reset(void) { libsqlite_rows_seen = 0; }
static long long libsqlite_rows_seen_value(void) { return libsqlite_rows_seen; }
*/
import "C"

import (
	"fmt"
	"os"
	"unsafe"
)

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

var checks, failures int

func check(cond bool, msg string) {
	checks++
	if cond {
		fmt.Println("  ok   - " + msg)
	} else {
		failures++
		fmt.Println("  FAIL - " + msg)
	}
}

func cstr(s string) *C.char { return C.CString(s) }

// take reads a library-allocated string and releases it.
func take(p *C.char) string {
	if p == nil {
		return ""
	}
	defer C.sqlite_core_string_free(p)
	return C.GoString(p)
}

// row counting uses the C trampoline (libsqlite_rows_cb), so no //export
// is needed and no Go pointer crosses into C.
func rowCallback() C.sqlite_core_row_callback_fn {
	C.libsqlite_rows_reset()
	return C.libsqlite_rows_cb()
}

func rowsSeen() int64 { return int64(C.libsqlite_rows_seen_value()) }

func writeFixture(path string, size int) int {
	data := make([]byte, size)
	for i := range data {
		data[i] = byte(i)
	}
	if err := os.WriteFile(path, data, 0o644); err != nil {
		panic(err)
	}
	return size
}

func main() {
	fmt.Println("libsqlite3 Go binding")
	fmt.Println("  library " + C.GoString(C.sqlite_core_version()) +
		" / engine " + C.GoString(C.sqlite_core_sqlite_version()))

	dbFile, fixture := "go_demo.db", "go_fixture.bin"
	for _, f := range []string{dbFile, fixture, "go_oversize.bin"} {
		os.Remove(f)
	}
	fixtureSize := writeFixture(fixture, 1024)
	writeFixture("go_oversize.bin", fixtureSize*2)

	err := make([]C.char, 256)
	var rc C.int

	db := C.sqlite_core_open_or_create(cstr(dbFile), &rc)
	check(db != nil && rc == SQLITE_OK, "open_or_create")
	if db == nil {
		os.Exit(1)
	}
	defer C.sqlite_core_close(db)

	check(int(C.sqlite_core_execute_command(db,
		cstr("CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)"),
		&err[0], 256)) == SQLITE_OK, "create table")

	var pid, pname C.sqlite_core_param_t
	C.param_set_int(&pid, cstr("id"), 42)
	C.param_set_text(&pname, cstr("name"), cstr("Ada"))
	check(int(C.sqlite_core_execute_non_query(db,
		cstr("INSERT INTO person(id, name) VALUES (:id, :name)"),
		&pid, 2, &err[0], 256)) == 1, "named parameter insert")

	// reader with a counting callback (engine column calls are not
	// exported on DLL builds, so rows are printed as JSON)
	cb := rowCallback()
	rows := int(C.sqlite_core_execute_reader(db,
		cstr("SELECT id, name FROM person ORDER BY id"), nil, 0,
		cb, nil, &err[0], 256))
	check(rows == 1 && rowsSeen() == 1, "reader visits the row")

	var out C.sqlite_core_param_t
	out._type = TYPE_INT
	check(int(C.sqlite_core_execute_scalar(db, cstr("SELECT COUNT(*) FROM person"),
		nil, 0, &out, &err[0], 256)) == SQLITE_OK &&
		int64(C.param_get_int(&out)) == 1, "scalar counts rows")

	var jsonPtr *C.char
	rows = int(C.sqlite_core_execute_reader_to_json(db,
		cstr("SELECT id, name FROM person ORDER BY id"), nil, 0,
		&jsonPtr, &err[0], 256))
	doc := take(jsonPtr)
	check(rows == 1 && len(doc) > 0 && doc[0] == '[', "reader to json")
	fmt.Println("  json: " + doc)

	check(int(C.sqlite_core_insert_json(db, cstr("person"),
		cstr(`[{"id": 2, "name": "Line1"}, {"id": 3, "name": "Line2"}]`),
		&err[0], 256)) == 2, "insert_json batch")

	var written C.int
	check(int(C.sqlite_core_insert_blob(db, cstr("person"), cstr("data"), 2,
		cstr(fixture), 0, &written, &err[0], 256)) == fixtureSize &&
		int(written) == fixtureSize, "insert_blob streams the file")

	sink := make([]byte, fixtureSize)
	var copied C.int
	check(int(C.sqlite_core_get_blob(db, cstr("SELECT data FROM person WHERE id = 2"),
		unsafe.Pointer(&sink[0]), C.int(fixtureSize), &copied)) == SQLITE_OK &&
		int(copied) == fixtureSize, "get_blob reads it back")
	stored, _ := os.ReadFile(fixture)
	check(string(sink[:copied]) == string(stored), "byte-for-byte round trip")

	check(int64(C.sqlite_core_last_insert_rowid(db)) > 0, "last_insert_rowid")

	check(int(C.sqlite_core_commit_transaction(db, &err[0], 256)) != SQLITE_OK,
		"commit without transaction fails")
	check(int(C.sqlite_core_begin_transaction(db, &err[0], 256)) == 1, "begin")
	C.sqlite_core_execute_non_query(db, cstr("INSERT INTO person(name) VALUES ('Temp')"),
		nil, 0, &err[0], 256)
	check(int(C.sqlite_core_rollback_transaction(db, &err[0], 256)) == 1, "rollback")
	var counted C.sqlite_core_param_t
	counted._type = TYPE_INT
	C.sqlite_core_execute_scalar(db, cstr("SELECT COUNT(*) FROM person"),
		nil, 0, &counted, &err[0], 256)
	check(int64(C.param_get_int(&counted)) == 3, "rollback discarded the row")

	check(int(C.sqlite_core_update_blob(db, cstr("person"), cstr("data"), 2,
		cstr("go_oversize.bin"), 0, &written, &err[0], 256)) == BLOB_TOO_SMALL,
		"update_blob refuses to exceed the column")

	var rc2 C.int
	check(C.sqlite_core_open_readonly(cstr("missing.db"), &rc2) == nil &&
		rc2 == SQLITE_CANTOPEN, "open missing file fails")
	check(int(C.sqlite_core_execute_non_query(db, cstr("SELECT 1; SELECT 2"),
		nil, 0, &err[0], 256)) == SQLITE_MISUSE, "multi-statement rejected")

	check(int(C.sqlite_core_close(db)) == 1, "close")

	for _, f := range []string{dbFile, fixture, "go_oversize.bin"} {
		os.Remove(f)
	}
	fmt.Printf("\n%d checks, %d failures\n", checks, failures)
	if failures > 0 {
		os.Exit(1)
	}
}