// libsqlite3 binding for Node.js (koffi). Complete frozen API: 34 symbols.
//
// Install: npm install koffi   (single dependency; prebuilt binaries)
// Run:     node example_full.js <path-to-libsqlite3.dll|so|dylib>

"use strict";

const koffi = require("koffi");

const LIB_CANDIDATES = [
    "libsqlite3.dll",
    "libsqlite3.so",
    "libsqlite3.dylib",
    process.argv[2],
].filter(Boolean);

function loadLib() {
    const path = require("path");
    for (const candidate of LIB_CANDIDATES) {
        const abs = path.isAbsolute(candidate) ? candidate
            : path.join(__dirname, "..", "..", "bin",
                process.platform === "win32" ? "x64-Windows" : "x64-Linux",
                "shared", "Release", candidate);
        if (require("fs").existsSync(abs)) return koffi.load(abs);
    }
    throw new Error("libsqlite3 library not found; pass the path as argv[2]");
}

const lib = loadLib();

// ---- constants --------------------------------------------------------
const SQLITE_OK = 0;
const SQLITE_MISUSE = 21;
const SQLITE_CANTOPEN = 14;
const TYPE_NULL = 0;
const TYPE_INT = 2;
const TYPE_DOUBLE = 4;
const TYPE_TEXT = 6;
const TYPE_BLOB = 8;
const DIRECTION_INPUT = 1;
const DIRECTION_OUTPUT = 2;
const BLOB_TOO_SMALL = -3;

// ---- the parameter struct (mirror of sqlite_core_param_t) ------------
const Param = koffi.struct("sqlite_core_param_t", {
    name: "const char *",
    no_insensitive: "int",
    type: "int",
    Direction: "int",
    value: koffi.union({
        dVal: "double",
        iVal: "int64_t",
        blob_or_text: koffi.struct({
            ptr: "char *",
            len: "int",
            max_len: "int",
        }),
    }),
});

// ---- the callback prototype (must exist before the function table) ---
const RowCallback = koffi.proto("sqlite_core_row_callback_fn", "int",
    ["void *", "int", "int", "void *"]);

// ---- the 34 functions -------------------------------------------------
const F = {
    version: lib.func("sqlite_core_version", "const char *", []),
    sqlite_version: lib.func("sqlite_core_sqlite_version", "const char *", []),
    error_code: lib.func("sqlite_core_error_code", "int", ["void *"]),
    error_code_extended: lib.func("sqlite_core_error_code_extended", "int", ["void *"]),
    error_message: lib.func("sqlite_core_error_message", "const char *", ["void *"]),
    string_format: lib.func("sqlite_core_string_format", "char *", ["const char *"]),
    string_concat: lib.func("sqlite_core_string_concat", "char *", ["const char *", "const char *"]),
    string_free: lib.func("sqlite_core_string_free", "void", ["const char *"]),
    utf8_to_ansi: lib.func("sqlite_core_string_utf8_to_ansi", "char *", ["const char *"]),
    ansi_to_utf8: lib.func("sqlite_core_string_ansi_to_utf8", "char *", ["const char *"]),
    open: lib.func("sqlite_core_open", "void *", ["const char *", "int *"]),
    open_ex: lib.func("sqlite_core_open_ex", "void *", ["const char *", "int", "int *"]),
    open_readonly: lib.func("sqlite_core_open_readonly", "void *", ["const char *", koffi.out(koffi.pointer("int"))]),
    open_or_create: lib.func("sqlite_core_open_or_create", "void *", ["const char *", "int *"]),
    open_uri_readonly: lib.func("sqlite_core_open_uri_readonly", "void *", ["const char *", "int *"]),
    close: lib.func("sqlite_core_close", "int", ["void *"]),
    is_readonly: lib.func("sqlite_core_is_readonly", "int", ["void *"]),
    last_insert_rowid: lib.func("sqlite_core_last_insert_rowid", "int64_t", ["void *"]),
    set_busy_timeout: lib.func("sqlite_core_set_busy_timeout", "int", ["void *", "int"]),
    param_clean_prefix: lib.func("sqlite_core_param_clean_prefix", "const char *", ["const char *"]),
    execute_command: lib.func("sqlite_core_execute_command", "int", ["void *", "const char *", "char *", "int"]),
    execute_transaction: lib.func("sqlite_core_execute_transaction", "int", ["void *", "const char *", "int", "char *", "int"]),
    execute_non_query: lib.func("sqlite_core_execute_non_query", "int", ["void *", "const char *", koffi.pointer(Param), "int", "char *", "int"]),
    execute_scalar: lib.func("sqlite_core_execute_scalar", "int", ["void *", "const char *", koffi.pointer(Param), "int", koffi.pointer(Param), "char *", "int"]),
    execute_reader: lib.func("sqlite_core_execute_reader", "int", ["void *", "const char *", koffi.pointer(Param), "int", koffi.pointer(RowCallback), "void *", "char *", "int"]),
    begin_transaction: lib.func("sqlite_core_begin_transaction", "int", ["void *", "char *", "int"]),
    commit_transaction: lib.func("sqlite_core_commit_transaction", "int", ["void *", "char *", "int"]),
    rollback_transaction: lib.func("sqlite_core_rollback_transaction", "int", ["void *", "char *", "int"]),
    json_writer_callback: lib.func("sqlite_core_json_writer_callback", "int", ["void *", "int", "int", "void *"]),
    execute_reader_to_json: lib.func("sqlite_core_execute_reader_to_json", "int", ["void *", "const char *", koffi.pointer(Param), "int", koffi.out("char **"), "char *", "int"]),
    insert_json: lib.func("sqlite_core_insert_json", "int", ["void *", "const char *", "const char *", "char *", "int"]),
    get_blob: lib.func("sqlite_core_get_blob", "int", ["void *", "const char *", "void *", "int", koffi.out(koffi.pointer("int"))]),
    insert_blob: lib.func("sqlite_core_insert_blob", "int", ["void *", "const char *", "const char *", "int64_t", "const char *", "int", koffi.out(koffi.pointer("int32_t")), "char *", "int"]),
    update_blob: lib.func("sqlite_core_update_blob", "int", ["void *", "const char *", "const char *", "int64_t", "const char *", "int", koffi.out(koffi.pointer("int32_t")), "char *", "int"]),
};

module.exports = {
    lib, F, Param, RowCallback, koffi,
    // allocates one (count omitted) or count Param instances
    alloc(count) { return count === undefined ? koffi.alloc(Param) : koffi.alloc(Param, count); },
    constants: {
        SQLITE_OK, SQLITE_MISUSE, SQLITE_CANTOPEN,
        TYPE_NULL, TYPE_INT, TYPE_DOUBLE, TYPE_TEXT, TYPE_BLOB,
        DIRECTION_INPUT, DIRECTION_OUTPUT, BLOB_TOO_SMALL,
    },
    // reads a heap string owned by the SQLite allocator and frees it
    take(ptr) {
        if (!ptr) return "";
        try { return Buffer.from(ptr).toString("utf8"); }
        finally { F.string_free(ptr); }
    },
    // reads a static library string (never freed)
    read(ptr) { return ptr ? Buffer.from(ptr).toString("utf8") : ""; },
};