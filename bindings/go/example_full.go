package main

// Canonical libsqlite3 flow in Go (see bindings/README.md).
//
// Run (Linux):
//   CGO_LDFLAGS="-L../bin/x64-Linux/shared/Release -llibsqlite3" \
//     LD_LIBRARY_PATH=../bin/x64-Linux/shared/Release go run example_full.go libsqlite.go
//
// Run (Windows with the MSYS2/MinGW gcc):
//   CGO_LDFLAGS="-L../bin/x64-Windows/shared/Release -llibsqlite3" \
//     PATH="../bin/x64-Windows/shared/Release;$PATH" go run example_full.go libsqlite.go

/*
#include <stdlib.h>
*/
import "C"

import (
	"fmt"
	"os"
	"unsafe"
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

//export countRows
func countRows(stmt unsafe.Pointer, columns, hasrows C.int, ctx unsafe.Pointer) C.int {
	if hasrows != 0 {
		*(*C.int)(ctx)++
	}
	return 0
}

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

	err := make([]byte, 256)
	var rc C.int

	db := C.sqlite_core_open_or_create(cstr(dbFile), &rc)
	check(db != nil && rc == C.SQLITE_OK, "open_or_create")
	if db == nil {
		os.Exit(1)
	}
	defer C.sqlite_core_close(db)

	check(int(C.sqlite_core_execute_command(db,
		cstr("CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)"),
		&err[0], 256)) == C.SQLITE_OK, "create table")

	var pid, pname C.sqlite_core_param_t
	C.param_set_int(&pid, cstr("id"), 42)
	C.param_set_text(&pname, cstr("name"), cstr("Ada"))
	check(int(C.sqlite_core_execute_non_query(db,
		cstr("INSERT INTO person(id, name) VALUES (:id, :name)"),
		&pid, 2, &err[0], 256)) == 1, "named parameter insert")

	// reader with a counting callback (engine column calls are not
	// exported on DLL builds, so rows are printed as JSON)
	counter := (*C.int)(C.malloc(C.sizeof_int))
	*counter = 0
	rows := int(C.sqlite_core_execute_reader(db,
		cstr("SELECT id, name FROM person ORDER BY id"), nil, 0,
		C.sqlite_core_row_callback_fn(C.countRows), unsafe.Pointer(counter),
		&err[0], 256))
	check(rows == 1 && *counter == 1, "reader visits the row")

	var out C.sqlite_core_param_t
	out._type = C.TYPE_INT
	check(int(C.sqlite_core_execute_scalar(db, cstr("SELECT COUNT(*) FROM person"),
		nil, 0, &out, &err[0], 256)) == C.SQLITE_OK &&
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
		unsafe.Pointer(&sink[0]), C.int(fixtureSize), &copied)) == C.SQLITE_OK &&
		int(copied) == fixtureSize, "get_blob reads it back")
	stored, _ := os.ReadFile(fixture)
	check(string(sink[:copied]) == string(stored), "byte-for-byte round trip")

	check(int64(C.sqlite_core_last_insert_rowid(db)) > 0, "last_insert_rowid")

	check(int(C.sqlite_core_commit_transaction(db, &err[0], 256)) != C.SQLITE_OK,
		"commit without transaction fails")
	check(int(C.sqlite_core_begin_transaction(db, &err[0], 256)) == 1, "begin")
	C.sqlite_core_execute_non_query(db, cstr("INSERT INTO person(name) VALUES ('Temp')"),
		nil, 0, &err[0], 256)
	check(int(C.sqlite_core_rollback_transaction(db, &err[0], 256)) == 1, "rollback")
	var counted C.sqlite_core_param_t
	counted._type = C.TYPE_INT
	C.sqlite_core_execute_scalar(db, cstr("SELECT COUNT(*) FROM person"),
		nil, 0, &counted, &err[0], 256)
	check(int64(C.param_get_int(&counted)) == 3, "rollback discarded the row")

	check(int(C.sqlite_core_update_blob(db, cstr("person"), cstr("data"), 2,
		cstr("go_oversize.bin"), 0, &written, &err[0], 256)) == C.BLOB_TOO_SMALL,
		"update_blob refuses to exceed the column")

	var rc2 C.int
	check(C.sqlite_core_open_readonly(cstr("missing.db"), &rc2) == nil &&
		rc2 == C.SQLITE_CANTOPEN, "open missing file fails")
	check(int(C.sqlite_core_execute_non_query(db, cstr("SELECT 1; SELECT 2"),
		nil, 0, &err[0], 256)) == C.SQLITE_MISUSE, "multi-statement rejected")

	check(int(C.sqlite_core_close(db)) == 1, "close")

	for _, f := range []string{dbFile, fixture, "go_oversize.bin"} {
		os.Remove(f)
	}
	fmt.Printf("\n%d checks, %d failures\n", checks, failures)
	if failures > 0 {
		os.Exit(1)
	}
}