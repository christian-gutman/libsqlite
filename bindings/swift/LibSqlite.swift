// libsqlite3 binding for Swift.
//
// Complete surface of the frozen public API: the 34 symbols in
// src/sqlite/core/sqlite.def. The prototypes come from include/libsqlite.h
// through module.modulemap (`import libsqlite3`); this file adds the Swift
// layer: a Connection class (closes in deinit), named parameters that own
// their native text, and the allocator rule in one place (takeString()).

import libsqlite3
import Foundation

// return codes and frozen constants, exactly as the C API defines them
public enum Sqlite {
    public static let OK = SQLITE_OK
    public static let ERROR = SQLITE_ERROR
    public static let MISUSE = SQLITE_MISUSE
    public static let CANTOPEN = SQLITE_CANTOPEN
    public static let BUFFER_TOO_SMALL = SQLITE_CORE_BUFFER_TOO_SMALL
    public static let BLOB_TOO_SMALL = SQLITE_CORE_BLOB_TOO_SMALL

    public static let TYPE_NULL = SQLITE_CORE_TYPE_NULL
    public static let TYPE_INT = SQLITE_CORE_TYPE_INT
    public static let TYPE_DOUBLE = SQLITE_CORE_TYPE_DOUBLE
    public static let TYPE_TEXT = SQLITE_CORE_TYPE_TEXT
    public static let TYPE_BLOB = SQLITE_CORE_TYPE_BLOB
}

// A row callback with the frozen signature: one call per row, hasrows == 0
// marks an empty result set, non-zero aborts the iteration. Swift imports the
// incomplete C structs (sqlite3, sqlite3_stmt) as OpaquePointer, so the
// statement arrives as an opaque pointer too.
public typealias RowCallback = @convention(c) (
    OpaquePointer?, Int32, Int32, UnsafeMutableRawPointer?
) -> Int32

/// Reads a `char*` allocated by the SQLite allocator and releases it with
/// sqlite_core_string_free (never with free()/deallocate()).
public func takeString(_ pointer: UnsafeMutablePointer<CChar>?) -> String {
    guard let pointer = pointer else { return "" }
    defer { sqlite_core_string_free(pointer) }
    return String(cString: pointer)
}

/// Copies the library's static data (version, error_message,
/// param_clean_prefix): the pointer is never released.
public func staticString(_ pointer: UnsafePointer<CChar>?) -> String {
    guard let pointer = pointer else { return "" }
    return String(cString: pointer)
}

/// Named parameters. sqlite_core_param_t holds *native pointers* (the name
/// and the text value), so ParamSet owns copies and hands out a contiguous
/// array for the duration of the call.
public final class ParamSet {
    fileprivate var items: [sqlite_core_param_t] = []
    private var owned: [UnsafeMutablePointer<CChar>?] = []

    // A contiguous, stable sqlite_core_param_t[]: Swift arrays are only
    // stable inside withUnsafeMutableBufferPointer, so the parameters are
    // copied into an allocated buffer that the C API can read directly.
    private var capacity = 0
    private var storage: UnsafeMutablePointer<sqlite_core_param_t>?

    public init() {}

    deinit {
        storage?.deallocate()
        for copy in owned where copy != nil {
            free(copy)
        }
    }

    /// The parameter array for the current call (nil when there are none).
    fileprivate func paramsPointer() -> UnsafeMutablePointer<sqlite_core_param_t>? {
        if items.isEmpty {
            return nil
        }
        if items.count > capacity {
            capacity = max(16, items.count * 2)
            storage?.deallocate()
            storage = UnsafeMutablePointer<sqlite_core_param_t>.allocate(capacity: capacity)
        }
        guard let storage = storage else { return nil }
        for index in items.indices {
            storage[index] = items[index]
        }
        return storage
    }

    fileprivate var count: Int32 { Int32(items.count) }

    public func text(_ name: String, _ value: String) -> ParamSet {
        items.append(makeParam(name, SQLITE_CORE_TYPE_TEXT, value))
        return self
    }

    public func integer(_ name: String, _ value: Int64) -> ParamSet {
        var param = makeParam(name, SQLITE_CORE_TYPE_INT, nil)
        param.value.iVal = value
        items.append(param)
        return self
    }

    public func floating(_ name: String, _ value: Double) -> ParamSet {
        var param = makeParam(name, SQLITE_CORE_TYPE_DOUBLE, nil)
        param.value.dVal = value
        items.append(param)
        return self
    }

    public func nullValue(_ name: String) -> ParamSet {
        items.append(makeParam(name, SQLITE_CORE_TYPE_NULL, nil))
        return self
    }

    private func makeParam(_ name: String, _ type: Int32, _ value: String?) -> sqlite_core_param_t {
        guard let nameCopy = strdup(name) else { return sqlite_core_param_t() }
        owned.append(nameCopy)
        var param = sqlite_core_param_t()
        param.name = UnsafePointer(nameCopy)
        param.no_insensitive = 0
        param.type = type
        param.Direction = SQLITE_CORE_DIRECTION_INPUT
        param.value.blob_or_text.ptr = nil
        param.value.blob_or_text.len = 0
        param.value.blob_or_text.max_len = 0
        if let value = value {
            let valueCopy = strdup(value)
            owned.append(valueCopy)
            param.value.blob_or_text.ptr = valueCopy
            param.value.blob_or_text.len = -1   // NUL-terminated utf-8
            param.value.blob_or_text.max_len = 0
        }
        return param
    }
}

/// Result of ExecuteScalar: the type tells which union member holds the value
/// (the C out struct doubles as the output, so a text result needs a
/// destination buffer: see Connection.scalarText(_:into:type:)).
public enum ScalarValue: Equatable {
    case null
    case int(Int64)
    case double(Double)
    case text(String)
}

/// RAII connection handle. The five open functions of the frozen API map to
/// the five initializers; the destructor closes the database. Every
/// initializer has an overload that reports the C return code.
public final class Connection {
    private var handle: OpaquePointer?

    /// sqlite_core_open
    public init?(path: String) {
        var rc: Int32 = 0
        handle = sqlite_core_open(path, &rc)
        if handle == nil { return nil }
    }

    public init?(path: String, rc: inout Int32) {
        handle = sqlite_core_open(path, &rc)
        if handle == nil { return nil }
    }

    /// sqlite_core_open_ex
    public init?(openEx path: String, flags: Int32) {
        var rc: Int32 = 0
        handle = sqlite_core_open_ex(path, flags, &rc)
        if handle == nil { return nil }
    }

    public init?(openEx path: String, flags: Int32, rc: inout Int32) {
        handle = sqlite_core_open_ex(path, flags, &rc)
        if handle == nil { return nil }
    }

    /// sqlite_core_open_readonly
    public init?(readonly path: String) {
        var rc: Int32 = 0
        handle = sqlite_core_open_readonly(path, &rc)
        if handle == nil { return nil }
    }

    public init?(readonly path: String, rc: inout Int32) {
        handle = sqlite_core_open_readonly(path, &rc)
        if handle == nil { return nil }
    }

    /// sqlite_core_open_or_create
    public init?(openOrCreate path: String) {
        var rc: Int32 = 0
        handle = sqlite_core_open_or_create(path, &rc)
        if handle == nil { return nil }
    }

    public init?(openOrCreate path: String, rc: inout Int32) {
        handle = sqlite_core_open_or_create(path, &rc)
        if handle == nil { return nil }
    }

    /// sqlite_core_open_uri_readonly
    public init?(uriReadonly path: String) {
        var rc: Int32 = 0
        handle = sqlite_core_open_uri_readonly(path, &rc)
        if handle == nil { return nil }
    }

    public init?(uriReadonly path: String, rc: inout Int32) {
        handle = sqlite_core_open_uri_readonly(path, &rc)
        if handle == nil { return nil }
    }

    public private(set) var isOpen = true

    deinit {
        close()
    }

    /// sqlite_core_close (idempotent: returns 0 when already closed)
    @discardableResult
    public func close() -> Int32 {
        guard let db = handle, isOpen else { return 0 }
        let rc = sqlite_core_close(db)
        handle = nil
        isOpen = false
        return rc
    }

    /// The parameter array for a call: the contiguous pointer of ParamSet
    /// (or nil when the call has no parameters).
    private func paramsView(_ params: ParamSet?) -> (UnsafeMutablePointer<sqlite_core_param_t>?, Int32) {
        guard let params = params else { return (nil, 0) }
        return (params.paramsPointer(), params.count)
    }

    // --- DDL / DML ------------------------------------------------------

    /// sqlite_core_execute_command (one or more statements allowed)
    @discardableResult
    public func command(_ sql: String) -> (rc: Int32, error: String) {
        var buffer = [CChar](repeating: 0, count: 512)
        let rc = sqlite_core_execute_command(handle, sql, &buffer, Int32(buffer.count))
        return (rc, rc == Sqlite.OK ? "" : String(cString: buffer))
    }

    /// sqlite_core_execute_transaction (txexpected 1 = begin, 0 = commit / rollback)
    @discardableResult
    public func transaction(_ sql: String, txexpected: Bool) -> (rc: Int32, error: String) {
        var buffer = [CChar](repeating: 0, count: 512)
        let rc = sqlite_core_execute_transaction(handle, sql, txexpected ? 1 : 0,
                                                 &buffer, Int32(buffer.count))
        return (rc, rc == 1 ? "" : String(cString: buffer))
    }

    public func begin() -> Bool { transaction("BEGIN IMMEDIATE", txexpected: true).rc == 1 }
    public func commit() -> Bool { transaction("COMMIT", txexpected: false).rc == 1 }
    public func rollback() -> Bool { transaction("ROLLBACK", txexpected: false).rc == 1 }

    /// sqlite_core_begin_transaction / _commit_transaction /
    /// _rollback_transaction: the dedicated transaction functions of the
    /// frozen API (same semantics as begin()/commit()/rollback()).
    public func beginTransaction() -> (rc: Int32, error: String) {
        var buffer = [CChar](repeating: 0, count: 512)
        let rc = sqlite_core_begin_transaction(handle, &buffer, Int32(buffer.count))
        return (rc, rc == 1 ? "" : String(cString: buffer))
    }

    public func commitTransaction() -> (rc: Int32, error: String) {
        var buffer = [CChar](repeating: 0, count: 512)
        let rc = sqlite_core_commit_transaction(handle, &buffer, Int32(buffer.count))
        return (rc, rc == 1 ? "" : String(cString: buffer))
    }

    public func rollbackTransaction() -> (rc: Int32, error: String) {
        var buffer = [CChar](repeating: 0, count: 512)
        let rc = sqlite_core_rollback_transaction(handle, &buffer, Int32(buffer.count))
        return (rc, rc == 1 ? "" : String(cString: buffer))
    }

    /// sqlite_core_execute_non_query (single statement, named parameters)
    @discardableResult
    public func nonQuery(_ sql: String, params: ParamSet?) -> (rc: Int32, error: String) {
        var buffer = [CChar](repeating: 0, count: 512)
        let (pointer, count) = paramsView(params)
        let rc = sqlite_core_execute_non_query(handle, sql, pointer, count,
                                               &buffer, Int32(buffer.count))
        return (rc, rc < 0 ? String(cString: buffer) : "")
    }

    // --- ExecuteScalar ---------------------------------------------------

    /// sqlite_core_execute_scalar. INTEGER/DOUBLE results are read straight
    /// from the union; TEXT results need a destination buffer (the library
    /// only writes when it fits), so use scalarText(_:into:type:).
    public func scalar(_ sql: String, params: ParamSet?) -> (rc: Int32, value: ScalarValue) {
        var out = sqlite_core_param_t()
        out.type = SQLITE_CORE_TYPE_INT
        let (pointer, count) = paramsView(params)
        let rc = sqlite_core_execute_scalar(handle, sql, pointer, count, &out, nil, 0)
        guard rc == Sqlite.OK else { return (rc, .null) }
        switch out.type {
        case SQLITE_CORE_TYPE_INT: return (rc, .int(out.value.iVal))
        case SQLITE_CORE_TYPE_DOUBLE: return (rc, .double(out.value.dVal))
        case SQLITE_CORE_TYPE_TEXT: return (rc, .text(""))
        default: return (rc, .null)
        }
    }

    public func scalarInt(_ sql: String) -> Int64 {
        let result = scalar(sql, params: nil)
        if case let .int(value) = result.value { return value }
        if case let .double(value) = result.value { return Int64(value) }
        return 0
    }

    /// Same call with the pre-allocated buffer the C API needs for TEXT.
    public func scalarText(_ sql: String, params: ParamSet?) -> (rc: Int32, value: String, type: Int32) {
        var out = sqlite_core_param_t()
        out.type = SQLITE_CORE_TYPE_TEXT
        var buffer = [CChar](repeating: 0, count: 512)
        var rc: Int32 = 0
        buffer.withUnsafeMutableBufferPointer { raw in
            out.value.blob_or_text.ptr = raw.baseAddress
            out.value.blob_or_text.max_len = Int32(raw.count)
            let (pointer, count) = paramsView(params)
            rc = sqlite_core_execute_scalar(handle, sql, pointer, count, &out, nil, 0)
        }
        let value = (rc == Sqlite.OK && out.type == SQLITE_CORE_TYPE_TEXT)
            ? String(cString: buffer)
            : ""
        return (rc, value, out.type)
    }

    // --- ExecuteReader ---------------------------------------------------

    /// sqlite_core_execute_reader driven by the module's counting trampoline
    /// (Swift cannot build a C function pointer from a context-capturing
    /// closure). A caller with its own C callback can use readRows(_:params:
    /// callback:context:) below instead.
    public func readRows(_ sql: String, params: ParamSet?) -> (rc: Int32, rows: Int32, columns: Int32, error: String) {
        var stats = libsqlite3_row_stats()
        var buffer = [CChar](repeating: 0, count: 512)
        let (pointer, count) = paramsView(params)
        var rc: Int32 = 0
        withUnsafeMutablePointer(to: &stats) { raw in
            rc = sqlite_core_execute_reader(handle, sql, pointer, count,
                                            libsqlite3_row_counter,
                                            UnsafeMutableRawPointer(raw),
                                            &buffer, Int32(buffer.count))
        }
        return (rc, Int32(stats.rows), Int32(stats.columns),
                rc < 0 ? String(cString: buffer) : "")
    }

    /// Same call with an explicit C callback and context.
    @discardableResult
    public func readRows(_ sql: String, params: ParamSet?, callback: RowCallback,
                         context: UnsafeMutableRawPointer?) -> (rc: Int32, error: String) {
        var buffer = [CChar](repeating: 0, count: 512)
        let (pointer, count) = paramsView(params)
        let rc = sqlite_core_execute_reader(handle, sql, pointer, count, callback,
                                            context, &buffer, Int32(buffer.count))
        return (rc, rc < 0 ? String(cString: buffer) : "")
    }

    // --- JSON ------------------------------------------------------------

    /// sqlite_core_execute_reader_to_json: the heap string is released here.
    public func readerJson(_ sql: String, params: ParamSet?) -> (rows: Int32, json: String) {
        var out: UnsafeMutablePointer<CChar>?
        let (pointer, count) = paramsView(params)
        let rows = sqlite_core_execute_reader_to_json(handle, sql, pointer, count,
                                                      &out, nil, 0)
        return (rows, takeString(out))
    }

    /// sqlite_core_insert_json
    @discardableResult
    public func insertJson(table: String, json: String) -> (rows: Int32, error: String) {
        var buffer = [CChar](repeating: 0, count: 512)
        let rows = sqlite_core_insert_json(handle, table, json, &buffer,
                                           Int32(buffer.count))
        return (rows, rows < 0 ? String(cString: buffer) : "")
    }

    /// sqlite_core_execute_reader driven by sqlite_core_json_writer_callback:
    /// the specialized callback fills the builder context (mirror of
    /// sqlite_core_json_builder_t from include/libsqlite.h) and `body` is the
    /// concatenated row objects — the array delimiters are added by
    /// execute_reader_to_json, not by the callback. The SQLite-allocated
    /// buffer is released here.
    public func readerJsonCallback(_ sql: String, params: ParamSet?) -> (rc: Int32, rows: Int32, body: String) {
        var builder = libsqlite3_json_builder_t()
        let (pointer, count) = paramsView(params)
        var rc: Int32 = 0
        withUnsafeMutablePointer(to: &builder) { raw in
            rc = sqlite_core_execute_reader(handle, sql, pointer, count,
                                            sqlite_core_json_writer_callback,
                                            UnsafeMutableRawPointer(raw), nil, 0)
        }
        let rows = Int32(builder.rows_processed)
        let body = takeString(builder.string_ptr)
        return (rc, rows, body)
    }

    // --- blob fields -----------------------------------------------------

    /// sqlite_core_get_blob: the buffer grows on SQLITE_CORE_BUFFER_TOO_SMALL
    public func getBlob(_ sql: String) -> (rc: Int32, data: [UInt8], copied: Int32) {
        var capacity = 4096
        var buffer = [UInt8](repeating: 0, count: capacity)
        var copied: Int32 = 0
        var rc: Int32 = Sqlite.OK
        while true {
            copied = 0
            rc = sqlite_core_get_blob(handle, sql, &buffer, Int32(capacity), &copied)
            if rc == SQLITE_CORE_BUFFER_TOO_SMALL && capacity < 64 * 1024 * 1024 {
                capacity *= 2
                buffer = [UInt8](repeating: 0, count: capacity)
                continue
            }
            break
        }
        let data = (rc == Sqlite.OK && copied > 0) ? Array(buffer.prefix(Int(copied))) : []
        return (rc, data, copied)
    }

    /// sqlite_core_insert_blob (chunked streaming write; rowid == 0 inserts)
    @discardableResult
    public func insertBlob(table: String, column: String, rowid: Int64, file: String,
                           chunkSize: Int32 = 0) -> (rc: Int32, written: Int32, error: String) {
        var written: Int32 = 0
        var buffer = [CChar](repeating: 0, count: 512)
        let rc = sqlite_core_insert_blob(handle, table, column, rowid, file,
                                         chunkSize, &written, &buffer,
                                         Int32(buffer.count))
        return (rc, written, rc < 0 ? String(cString: buffer) : "")
    }

    /// sqlite_core_update_blob
    @discardableResult
    public func updateBlob(table: String, column: String, rowid: Int64, file: String,
                           chunkSize: Int32 = 0) -> (rc: Int32, written: Int32, error: String) {
        var written: Int32 = 0
        var buffer = [CChar](repeating: 0, count: 512)
        let rc = sqlite_core_update_blob(handle, table, column, rowid, file,
                                         chunkSize, &written, &buffer,
                                         Int32(buffer.count))
        return (rc, written, rc < 0 ? String(cString: buffer) : "")
    }

    // --- connection state / errors --------------------------------------

    public var isReadonly: Int32 { sqlite_core_is_readonly(handle) }
    public var lastInsertRowid: Int64 { sqlite_core_last_insert_rowid(handle) }
    public var errorCode: Int32 { sqlite_core_error_code(handle) }
    public var errorCodeExtended: Int32 { sqlite_core_error_code_extended(handle) }
    public var errorMessage: String { staticString(sqlite_core_error_message(handle)) }

    @discardableResult
    public func setBusyTimeout(_ ms: Int32) -> Int32 {
        sqlite_core_set_busy_timeout(handle, ms)
    }
}

// --- library-level helpers (the rest of the frozen surface) -------------

public func libraryVersion() -> String {
    staticString(sqlite_core_version())
}

public func sqliteVersion() -> String {
    staticString(sqlite_core_sqlite_version())
}

/// sqlite_core_param_clean_prefix: returns a pointer into the input.
public func paramCleanPrefix(_ name: String) -> String {
    staticString(sqlite_core_param_clean_prefix(name))
}

// sqlite_core_string_format is C-variadic and Swift marks imported variadics
// unavailable, so the frozen symbol is bound directly with the single-argument
// signature this binding needs (@_silgen_name is the documented escape hatch).
@_silgen_name("sqlite_core_string_format")
private func sqlite_core_string_format_c(_ format: UnsafePointer<CChar>,
                                         _ argument: UnsafePointer<CChar>) -> UnsafeMutablePointer<CChar>?

/// sqlite_core_string_format / _concat / _ansi_to_utf8 / _utf8_to_ansi:
/// every result is SQLite-allocated and released here.
public func stringFormat(_ format: String, _ argument: String) -> String {
    takeString(sqlite_core_string_format_c(format, argument))
}

public func stringConcat(_ left: String, _ right: String) -> String {
    takeString(sqlite_core_string_concat(left, right))
}

public func ansiToUtf8(_ text: String) -> String {
    takeString(sqlite_core_string_ansi_to_utf8(text))
}

public func utf8ToAnsi(_ text: String) -> String {
    takeString(sqlite_core_string_utf8_to_ansi(text))
}