// libsqlite3 binding for Rust: the frozen 34-symbol surface, no crates.
#![allow(dead_code)] // the whole surface is declared; each example uses a subset
//
// The library is linked with the platform name:
//   Windows: libsqlite3.lib (import library next to libsqlite3.dll)
//   Linux/macOS: rustc -C link-arg=-l:libsqlite3.so -L <dir>  (the file
//                already carries the "lib" prefix)
//
// Build:
//   rustc -L native=../../bin/x64-Windows/shared/Release example_full.rs -o example.exe

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_double, c_int, c_void};

#[link(name = "libsqlite3")]
extern "C" {
    fn sqlite_core_version() -> *const c_char;
    fn sqlite_core_sqlite_version() -> *const c_char;
    fn sqlite_core_error_code(db: *mut c_void) -> c_int;
    fn sqlite_core_error_code_extended(db: *mut c_void) -> c_int;
    fn sqlite_core_error_message(db: *mut c_void) -> *const c_char;
    fn sqlite_core_string_format(fmt: *const c_char) -> *mut c_char;
    fn sqlite_core_string_concat(a: *const c_char, b: *const c_char) -> *mut c_char;
    fn sqlite_core_string_free(s: *const c_char);
    fn sqlite_core_string_utf8_to_ansi(s: *const c_char) -> *mut c_char;
    fn sqlite_core_string_ansi_to_utf8(s: *const c_char) -> *mut c_char;
    fn sqlite_core_open(path: *const c_char, out_rc: *mut c_int) -> *mut c_void;
    fn sqlite_core_open_ex(path: *const c_char, flags: c_int, out_rc: *mut c_int) -> *mut c_void;
    fn sqlite_core_open_readonly(path: *const c_char, out_rc: *mut c_int) -> *mut c_void;
    fn sqlite_core_open_or_create(path: *const c_char, out_rc: *mut c_int) -> *mut c_void;
    fn sqlite_core_open_uri_readonly(path: *const c_char, out_rc: *mut c_int) -> *mut c_void;
    fn sqlite_core_close(db: *mut c_void) -> c_int;
    fn sqlite_core_is_readonly(db: *mut c_void) -> c_int;
    fn sqlite_core_last_insert_rowid(db: *mut c_void) -> i64;
    fn sqlite_core_set_busy_timeout(db: *mut c_void, ms: c_int) -> c_int;
    fn sqlite_core_param_clean_prefix(name: *const c_char) -> *const c_char;
    fn sqlite_core_execute_command(
        db: *mut c_void, sql: *const c_char, err: *mut c_char, max: c_int,
    ) -> c_int;
    fn sqlite_core_execute_transaction(
        db: *mut c_void, sql: *const c_char, tx: c_int, err: *mut c_char, max: c_int,
    ) -> c_int;
    fn sqlite_core_execute_non_query(
        db: *mut c_void, sql: *const c_char, params: *mut Param, count: c_int,
        err: *mut c_char, max: c_int,
    ) -> c_int;
    fn sqlite_core_execute_scalar(
        db: *mut c_void, sql: *const c_char, params: *mut Param, count: c_int,
        out: *mut Param, err: *mut c_char, max: c_int,
    ) -> c_int;
    fn sqlite_core_execute_reader(
        db: *mut c_void, sql: *const c_char, params: *mut Param, count: c_int,
        cb: RowCallback, ctx: *mut c_void, err: *mut c_char, max: c_int,
    ) -> c_int;
    fn sqlite_core_begin_transaction(db: *mut c_void, err: *mut c_char, max: c_int) -> c_int;
    fn sqlite_core_commit_transaction(db: *mut c_void, err: *mut c_char, max: c_int) -> c_int;
    fn sqlite_core_rollback_transaction(db: *mut c_void, err: *mut c_char, max: c_int) -> c_int;
    fn sqlite_core_json_writer_callback(
        stmt: *mut c_void, columns: c_int, hasrows: c_int, ctx: *mut c_void,
    ) -> c_int;
    fn sqlite_core_execute_reader_to_json(
        db: *mut c_void, sql: *const c_char, params: *mut Param, count: c_int,
        out_json: *mut *mut c_char, err: *mut c_char, max: c_int,
    ) -> c_int;
    fn sqlite_core_insert_json(
        db: *mut c_void, table: *const c_char, json: *const c_char,
        err: *mut c_char, max: c_int,
    ) -> c_int;
    fn sqlite_core_get_blob(
        db: *mut c_void, sql: *const c_char, dest: *mut c_void, max: c_int,
        copied: *mut c_int,
    ) -> c_int;
    fn sqlite_core_insert_blob(
        db: *mut c_void, table: *const c_char, column: *const c_char, rowid: i64,
        file: *const c_char, chunk: c_int, written: *mut c_int, err: *mut c_char, max: c_int,
    ) -> c_int;
    fn sqlite_core_update_blob(
        db: *mut c_void, table: *const c_char, column: *const c_char, rowid: i64,
        file: *const c_char, chunk: c_int, written: *mut c_int, err: *mut c_char, max: c_int,
    ) -> c_int;
}

pub const SQLITE_OK: i32 = 0;
pub const SQLITE_MISUSE: i32 = 21;
pub const SQLITE_CANTOPEN: i32 = 14;
pub const TYPE_NULL: i32 = 0;
pub const TYPE_INT: i32 = 2;
pub const TYPE_DOUBLE: i32 = 4;
pub const TYPE_TEXT: i32 = 6;
pub const TYPE_BLOB: i32 = 8;
pub const DIRECTION_INPUT: i32 = 1;
pub const DIRECTION_OUTPUT: i32 = 2;
pub const BLOB_TOO_SMALL: i32 = -3;

pub type RowCallback = extern "C" fn(*mut c_void, c_int, c_int, *mut c_void) -> c_int;

#[repr(C)]
pub union ParamValue {
    pub d_val: c_double,
    pub i_val: i64,
    pub blob_or_text: BlobOrText,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct BlobOrText {
    pub ptr: *mut c_char,
    pub len: c_int,
    pub max_len: c_int,
}

#[repr(C)]
pub struct Param {
    pub name: *const c_char,
    pub no_insensitive: c_int,
    pub ptype: c_int,
    pub direction: c_int,
    pub value: ParamValue,
}

impl Param {
    pub fn int_param(name: &str, v: i64) -> Param {
        Param {
            name: CString::new(name).unwrap().into_raw(),
            no_insensitive: 0,
            ptype: TYPE_INT,
            direction: DIRECTION_INPUT,
            value: ParamValue { i_val: v },
        }
    }
    pub fn text_param(name: &str, v: &str) -> Param {
        Param {
            name: CString::new(name).unwrap().into_raw(),
            no_insensitive: 0,
            ptype: TYPE_TEXT,
            direction: DIRECTION_INPUT,
            value: ParamValue {
                blob_or_text: BlobOrText {
                    ptr: CString::new(v).unwrap().into_raw(),
                    len: -1,
                    max_len: 0,
                },
            },
        }
    }
    pub fn scalar_out_int() -> Param {
        Param {
            name: std::ptr::null(),
            no_insensitive: 0,
            ptype: TYPE_INT,
            direction: DIRECTION_INPUT,
            value: ParamValue { i_val: 0 },
        }
    }
    pub fn as_i64(&self) -> i64 { unsafe { self.value.i_val } }
}

/// Reads a static library string (versions, messages): never freed.
pub unsafe fn static_str(p: *const c_char) -> String {
    if p.is_null() { String::new() } else { CStr::from_ptr(p).to_string_lossy().into_owned() }
}

/// Reads a heap string owned by the SQLite allocator and frees it.
pub unsafe fn take_str(p: *mut c_char) -> String {
    if p.is_null() { return String::new(); }
    let s = CStr::from_ptr(p).to_string_lossy().into_owned();
    sqlite_core_string_free(p);
    s
}

pub unsafe fn cs(s: &str) -> *const c_char { CString::new(s).unwrap().into_raw() }