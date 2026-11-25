// Canonical libsqlite3 flow in Rust (see bindings/README.md).
//
// Build & run (Windows):
//   rustc -L native=../../bin/x64-Windows/shared/Release example_full.rs -o example.exe
//   .\example.exe            (libsqlite3.dll next to the exe or on PATH)
//
// Linux/macOS:
//   rustc -C link-arg=-l:libsqlite3.so \
//         -L native=../../bin/x64-Linux/shared/Release example_full.rs -o example
//   LD_LIBRARY_PATH=../../bin/x64-Linux/shared/Release ./example
//
// Exit code = number of failed checks.

// Inner attribute in the file that does the include!(): libsqlite.rs is an
// included file and cannot carry one itself (see the note at its top).
#![allow(dead_code)]

include!("libsqlite.rs");

use std::io::Write;

static mut CHECKS: i32 = 0;
static mut FAILURES: i32 = 0;

fn check(cond: bool, msg: &str) {
    unsafe {
        CHECKS += 1;
        if cond { println!("  ok   - {}", msg); }
        else { FAILURES += 1; println!("  FAIL - {}", msg); }
    }
}

fn write_fixture(path: &str, size: usize) {
    let data: Vec<u8> = (0..size).map(|i| (i & 0xFF) as u8).collect();
    std::fs::write(path, data).unwrap();
}

fn fixture_bytes(path: &str) -> Vec<u8> { std::fs::read(path).unwrap() }

fn main() {
    unsafe { run(); let f = FAILURES; std::process::exit(f); }
}

unsafe fn run() {
    println!("libsqlite3 Rust binding");
    println!("  library {} / engine {}",
             static_str(sqlite_core_version()),
             static_str(sqlite_core_sqlite_version()));

    let _ = std::io::stdout().flush();

    let mut rc: i32 = 0;
    let db = sqlite_core_open_or_create(cs("rust_demo.db"), &mut rc);
    check(!db.is_null() && rc == SQLITE_OK, "open_or_create");

    if db.is_null() { std::process::exit(1); }

    let mut err = vec![0i8; 256];

    check(sqlite_core_execute_command(db,
        cs("CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)"),
        err.as_mut_ptr(), 256) == SQLITE_OK, "create table");

    // named parameters (the CStrings are kept alive until after the call)
    let mut pars = [Param::int_param("id", 42), Param::text_param("name", "Ada")];
    check(sqlite_core_execute_non_query(db,
        cs("INSERT INTO person(id, name) VALUES (:id, :name)"),
        pars.as_mut_ptr(), 2, err.as_mut_ptr(), 256) == 1, "named parameter insert");

    // reader callback (counts rows; no engine calls, so it works on DLLs)
    let mut seen: i64 = 0;
    extern "C" fn on_row(_s: *mut c_void, _c: i32, has: i32, ctx: *mut c_void) -> i32 {
        unsafe { *(ctx as *mut i64) += if has != 0 { 1 } else { 0 }; }
        0
    }
    let rows = sqlite_core_execute_reader(db,
        cs("SELECT id, name FROM person ORDER BY id"),
        std::ptr::null_mut(), 0, on_row, &mut seen as *mut i64 as *mut c_void,
        err.as_mut_ptr(), 256);
    check(rows == 1 && seen == 1, "reader visits the row");

    let mut out = Param::scalar_out_int();
    check(sqlite_core_execute_scalar(db, cs("SELECT COUNT(*) FROM person"),
        std::ptr::null_mut(), 0, &mut out, err.as_mut_ptr(), 256) == SQLITE_OK &&
        out.as_i64() == 1, "scalar counts rows");

    let mut json_ptr: *mut c_char = std::ptr::null_mut();
    let rows = sqlite_core_execute_reader_to_json(db,
        cs("SELECT id, name FROM person ORDER BY id"),
        std::ptr::null_mut(), 0, &mut json_ptr, err.as_mut_ptr(), 256);
    let doc = take_str(json_ptr);
    check(rows == 1 && doc.starts_with('['), "reader to json");
    println!("  json: {}", doc);

    check(sqlite_core_insert_json(db, cs("person"),
        cs(r#"[{"id": 2, "name": "Line1"}, {"id": 3, "name": "Line2"}]"#),
        err.as_mut_ptr(), 256) == 2, "insert_json batch");

    write_fixture("rust_fixture.bin", 1024);
    write_fixture("rust_oversize.bin", 2048);

    let mut written: i32 = 0;
    check(sqlite_core_insert_blob(db, cs("person"), cs("data"), 2,
        cs("rust_fixture.bin"), 0, &mut written, err.as_mut_ptr(), 256) == 1024 &&
        written == 1024, "insert_blob streams the file");

    let mut sink = vec![0u8; 1024];
    let mut copied: i32 = 0;
    check(sqlite_core_get_blob(db, cs("SELECT data FROM person WHERE id = 2"),
        sink.as_mut_ptr() as *mut c_void, 1024, &mut copied) == SQLITE_OK &&
        copied == 1024, "get_blob reads it back");
    check(sink == fixture_bytes("rust_fixture.bin"), "byte-for-byte round trip");

    check(sqlite_core_last_insert_rowid(db) > 0, "last_insert_rowid");

    check(sqlite_core_commit_transaction(db, err.as_mut_ptr(), 256) != SQLITE_OK,
        "commit without transaction fails");
    check(sqlite_core_begin_transaction(db, err.as_mut_ptr(), 256) == 1, "begin");
    sqlite_core_execute_non_query(db, cs("INSERT INTO person(name) VALUES ('Temp')"),
        std::ptr::null_mut(), 0, err.as_mut_ptr(), 256);
    check(sqlite_core_rollback_transaction(db, err.as_mut_ptr(), 256) == 1, "rollback");
    let mut counted = Param::scalar_out_int();
    sqlite_core_execute_scalar(db, cs("SELECT COUNT(*) FROM person"),
        std::ptr::null_mut(), 0, &mut counted, err.as_mut_ptr(), 256);
    check(counted.as_i64() == 3, "rollback discarded the row");

    check(sqlite_core_update_blob(db, cs("person"), cs("data"), 2,
        cs("rust_oversize.bin"), 0, &mut written, err.as_mut_ptr(), 256) == BLOB_TOO_SMALL,
        "update_blob refuses to exceed the column");

    let mut rc2: i32 = 0;
    check(sqlite_core_open_readonly(cs("missing.db"), &mut rc2).is_null() &&
        rc2 == SQLITE_CANTOPEN, "open missing file fails");
    check(sqlite_core_execute_non_query(db, cs("SELECT 1; SELECT 2"),
        std::ptr::null_mut(), 0, err.as_mut_ptr(), 256) == SQLITE_MISUSE,
        "multi-statement rejected");

    // error diagnostics + busy timeout: the three one-liner symbols every
    // binding must exercise (bindings/CONTRACT.md). error_message is static
    // library data, so static_str copies it and frees nothing.
    check(sqlite_core_set_busy_timeout(db, 1000) == 1, "set_busy_timeout");
    sqlite_core_execute_non_query(db, cs("SELECT * FROM no_such_table"),
        std::ptr::null_mut(), 0, err.as_mut_ptr(), 256);
    check(sqlite_core_error_code(db) != SQLITE_OK, "error_code reports the engine failure");
    check(sqlite_core_error_code_extended(db) != SQLITE_OK, "error_code_extended reports the engine failure");
    check(!static_str(sqlite_core_error_message(db)).is_empty(), "error_message reports the last failure");

    check(sqlite_core_close(db) == 1, "close");

    for f in ["rust_demo.db", "rust_fixture.bin", "rust_oversize.bin"] {
        let _ = std::fs::remove_file(f);
    }
    unsafe { println!("\n{} checks, {} failures", CHECKS, FAILURES); }
}