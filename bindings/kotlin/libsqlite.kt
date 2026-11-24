// libsqlite3 binding for Kotlin (JNA).
//
// Complete surface of the frozen public API: the 34 symbols exported by
// src/sqlite/core/sqlite.def. No compilation of native code: the JNA jar
// is the only dependency, and Kotlin calls the library through it.
//
// Two Kotlin specifics are encoded here on purpose:
//   * "jna.encoding" is forced to UTF-8 before loading, because JNA converts
//     String parameters with the platform default charset otherwise (POSIX/C
//     in containers breaks every non-ASCII text);
//   * sqlite_core_param_t holds *native pointers* (name, blob_or_text.ptr),
//     so ParamSet builds the struct array in native memory (offsets per
//     pointer size) and keeps the string copies alive for the call - the same
//     approach the Ruby and Fortran bindings take, instead of subclassing
//     JNA's Structure/Union from Kotlin.

import com.sun.jna.Callback
import com.sun.jna.Library
import com.sun.jna.Memory
import com.sun.jna.Native
import com.sun.jna.Pointer
import com.sun.jna.ptr.IntByReference
import com.sun.jna.ptr.PointerByReference

object Sqlite {

    const val OK = 0
    const val ERROR = 1
    const val MISUSE = 21
    const val CANTOPEN = 14

    const val BUFFER_TOO_SMALL = -2
    const val BLOB_TOO_SMALL = -3

    const val TYPE_NULL = 0
    const val TYPE_INT = 2
    const val TYPE_DOUBLE = 4
    const val TYPE_TEXT = 6
    const val TYPE_BLOB = 8

    const val DIRECTION_INPUT = 1
    const val DIRECTION_OUTPUT = 2
    const val DIRECTION_INPUTOUTPUT = 3

    // sqlite_core_param_t layout (from src/sqlite/core/sqlite_core.h):
    //   const char *name;       pointer
    //   int no_insensitive;     4
    //   int type;               4
    //   int Direction;          4  (+ 4 padding on LP64)
    //   union { double dVal; sqlite3_int64 iVal;
    //          struct { char *ptr; int len; int max_len; } blob_or_text; } value;
    private const val PTR = 8              // com.sun.jna.Native.POINTER_SIZE
    private const val VALUE_OFFSET = 24    // PTR == 8 ? 24 : 16
    private const val PARAM_SIZE = 40      // PTR == 8 ? 40 : 28

    private val CANDIDATES = listOf(
        "libsqlite3.so",
        "libsqlite3.dylib",
        "../../bin/x64-Linux/shared/Release/libsqlite3.so",
        "../../bin/x64-Windows/shared/Release/libsqlite3.dll",
        "../../bin/arm64-Darwin/shared/Release/libsqlite3.dylib",
    )

    /** The 34 exported symbols, one entry per line of sqlite.def. */
    interface Core : Library {
        fun sqlite_core_version(): Pointer?
        fun sqlite_core_sqlite_version(): Pointer?
        fun sqlite_core_error_code(db: Pointer?): Int
        fun sqlite_core_error_code_extended(db: Pointer?): Int
        fun sqlite_core_error_message(db: Pointer?): Pointer?

        fun sqlite_core_string_format(format: String, vararg args: Any?): Pointer?
        fun sqlite_core_string_concat(a: String, b: String): Pointer?
        fun sqlite_core_string_free(value: Pointer?)
        fun sqlite_core_string_utf8_to_ansi(text: String): Pointer?
        fun sqlite_core_string_ansi_to_utf8(text: String): Pointer?

        fun sqlite_core_open(path: String, rc: IntByReference): Pointer?
        fun sqlite_core_open_ex(path: String, flags: Int, rc: IntByReference): Pointer?
        fun sqlite_core_open_readonly(path: String, rc: IntByReference): Pointer?
        fun sqlite_core_open_or_create(path: String, rc: IntByReference): Pointer?
        fun sqlite_core_open_uri_readonly(path: String, rc: IntByReference): Pointer?
        fun sqlite_core_close(db: Pointer?): Int
        fun sqlite_core_is_readonly(db: Pointer?): Int
        fun sqlite_core_last_insert_rowid(db: Pointer?): Long
        fun sqlite_core_set_busy_timeout(db: Pointer?, ms: Int): Int

        fun sqlite_core_param_clean_prefix(name: String): Pointer?

        fun sqlite_core_execute_command(db: Pointer?, sql: String, err: Memory, max: Int): Int
        fun sqlite_core_execute_transaction(db: Pointer?, sql: String, txexpected: Int, err: Memory, max: Int): Int
        fun sqlite_core_execute_non_query(db: Pointer?, sql: String, params: Pointer?, count: Int, err: Memory, max: Int): Int
        fun sqlite_core_execute_scalar(db: Pointer?, sql: String, params: Pointer?, count: Int, out: Memory, err: Pointer?, max: Int): Int
        fun sqlite_core_execute_reader(db: Pointer?, sql: String, params: Pointer?, count: Int,
                                      callback: Callback?, ctx: Pointer?, err: Memory, max: Int): Int

        fun sqlite_core_begin_transaction(db: Pointer?, err: Memory, max: Int): Int
        fun sqlite_core_commit_transaction(db: Pointer?, err: Memory, max: Int): Int
        fun sqlite_core_rollback_transaction(db: Pointer?, err: Memory, max: Int): Int

        fun sqlite_core_json_writer_callback(stmt: Pointer?, columns: Int, hasrows: Int, ctx: Pointer?): Int
        fun sqlite_core_execute_reader_to_json(db: Pointer?, sql: String, params: Pointer?, count: Int,
                                              out: PointerByReference, err: Memory, max: Int): Int
        fun sqlite_core_insert_json(db: Pointer?, table: String, json: String, err: Memory, max: Int): Int

        fun sqlite_core_get_blob(db: Pointer?, sql: String, dest: Pointer?, max: Int, copied: IntByReference): Int
        fun sqlite_core_insert_blob(db: Pointer?, table: String, column: String, rowid: Long,
                                   file: String, chunk: Int, written: IntByReference, err: Memory, max: Int): Int
        fun sqlite_core_update_blob(db: Pointer?, table: String, column: String, rowid: Long,
                                   file: String, chunk: Int, written: IntByReference, err: Memory, max: Int): Int
    }

    /** Reader trampoline: a JNA Callback interface implemented by the caller. */
    fun interface RowCallback : Callback {
        fun callback(stmt: Pointer?, columns: Int, hasrows: Int, ctx: Pointer?): Int
    }

    lateinit var lib: Core
        private set

    var libpath: String = ""
        private set

    fun load(path: String? = null) {
        // JNA converts String parameters with the platform charset unless
        // jna.encoding is set first (the container locale is POSIX/C).
        if (System.getProperty("jna.encoding") == null) {
            System.setProperty("jna.encoding", "UTF-8")
        }
        val resolved = path ?: CANDIDATES.firstOrNull { java.io.File(it).isFile }
            ?: throw UnsatisfiedLinkError("libsqlite3 library not found; pass the path explicitly")
        if (!java.io.File(resolved).isFile) {
            throw UnsatisfiedLinkError("libsqlite3 library not found: $resolved")
        }
        libpath = resolved
        lib = Native.load(resolved, Core::class.java)
    }

    // --- library-level helpers ------------------------------------------

    fun version(): String = staticString(lib.sqlite_core_version())
    fun sqliteVersion(): String = staticString(lib.sqlite_core_sqlite_version())
    fun paramCleanPrefix(name: String): String = staticString(lib.sqlite_core_param_clean_prefix(name))
    fun stringConcat(a: String, b: String): String = takeString(lib.sqlite_core_string_concat(a, b))
    fun stringFormat(format: String, argument: String): String =
        takeString(lib.sqlite_core_string_format(format, argument))
    fun ansiToUtf8(text: String): String = takeString(lib.sqlite_core_string_ansi_to_utf8(text))
    fun utf8ToAnsi(text: String): String = takeString(lib.sqlite_core_string_utf8_to_ansi(text))

    // --- string / buffer helpers ----------------------------------------

    /** NUL-terminated utf-8 copy of a string, pinned until released. */
    fun cstring(value: String): Memory {
        val bytes = value.toByteArray(Charsets.UTF_8)
        val memory = Memory(bytes.size + 1L)
        memory.write(0, bytes, 0, bytes.size)
        memory.setByte(bytes.size.toLong(), 0)
        return memory
    }

    fun errBuffer(): Memory {
        val memory = Memory(512)
        memory.clear()
        return memory
    }

    fun errText(buffer: Memory): String {
        val bytes = buffer.getByteArray(0, 512)
        var end = 0
        while (end < bytes.size && bytes[end] != 0.toByte()) end++
        return if (end == 0) "" else String(bytes, 0, end, Charsets.UTF_8)
    }

    /** Reads a char* the library owns (static data): never free it. */
    fun staticString(pointer: Pointer?): String {
        if (pointer == null) return ""
        return pointer.getString(0, "UTF-8")
    }

    /** Reads a char* allocated by the SQLite allocator and releases it. */
    fun takeString(pointer: Pointer?): String {
        if (pointer == null) return ""
        val value = pointer.getString(0, "UTF-8")
        lib.sqlite_core_string_free(pointer)
        return value
    }

    fun intOut(): IntByReference = IntByReference(0)

    fun pointerOut(): PointerByReference = PointerByReference()

    /** Zeroed sqlite_core_param_t, used as the out_scalar_result of
     *  sqlite_core_execute_scalar. */
    fun paramStruct(): Memory {
        val memory = Memory(PARAM_SIZE.toLong())
        memory.clear()
        return memory
    }

    /** sqlite_core_param_t:type lives right after no_insensitive. */
    fun typeOf(struct: Memory): Int = struct.getInt(PTR + 4L)

    /** Named parameters: the C struct array is built in native memory and
     *  the copies of name/value live in this object. */
    class ParamSet {
        private val owned = ArrayList<Memory>()
        private val items = ArrayList<Array<Any>>()   // kind, name, value

        fun text(name: String, value: String): ParamSet {
            items.add(arrayOf("text", name, value))
            return this
        }

        fun integer(name: String, value: Long): ParamSet {
            items.add(arrayOf("int", name, value))
            return this
        }

        fun floating(name: String, value: Double): ParamSet {
            items.add(arrayOf("double", name, value))
            return this
        }

        fun nullValue(name: String): ParamSet {
            items.add(arrayOf("null", name, ""))
            return this
        }

        val count: Int get() = items.size

        /** Builds the contiguous array and returns it as a Pointer. */
        fun pointer(): Pointer? {
            if (items.isEmpty()) return null
            val array = Memory(PARAM_SIZE.toLong() * items.size)
            array.clear()
            items.forEachIndexed { index, item ->
                val kind = item[0] as String
                val base = index * PARAM_SIZE.toLong()
                val namePtr = own(item[1] as String)
                array.setPointer(base, namePtr)
                array.setInt(base + PTR, 0)                             // no_insensitive
                array.setInt(base + PTR + 4, typeCode(kind))
                array.setInt(base + PTR + 8, DIRECTION_INPUT)
                fillValue(array, base + VALUE_OFFSET, item)
            }
            return array
        }

        private fun typeCode(kind: String): Int = when (kind) {
            "int" -> TYPE_INT
            "double" -> TYPE_DOUBLE
            "text" -> TYPE_TEXT
            else -> TYPE_NULL
        }

        private fun fillValue(array: Memory, offset: Long, item: Array<Any>) {
            when (item[0] as String) {
                "int" -> array.setLong(offset, item[2] as Long)
                "double" -> array.setDouble(offset, item[2] as Double)
                "text" -> {
                    array.setPointer(offset, own(item[2] as String))
                    array.setInt(offset + PTR, -1)      // NUL-terminated UTF-8
                    array.setInt(offset + PTR + 4, 0)   // max_len unused
                }
            }
        }

        private fun own(value: String): Memory {
            val memory = cstring(value)
            owned.add(memory)
            return memory
        }
    }

    /** Row counter for the reader callback (kept by the caller). */
    class RowCounter {
        var rows: Int = 0
        var columns: Int = 0
    }

    /** Connection handle wrapper: close() calls sqlite_core_close and returns
     *  its result (1 on success). */
    class Connection(val handle: Pointer) {

        fun command(sql: String): Pair<Int, String> {
            val err = errBuffer()
            val rc = lib.sqlite_core_execute_command(handle, sql, err, 512)
            return rc to (if (rc == OK) "" else errText(err))
        }

        fun transaction(sql: String, txexpected: Boolean): Pair<Int, String> {
            val err = errBuffer()
            val rc = lib.sqlite_core_execute_transaction(handle, sql, if (txexpected) 1 else 0, err, 512)
            return rc to (if (rc == 1) "" else errText(err))
        }

        fun begin(): Boolean = transaction("BEGIN IMMEDIATE", true).first == 1
        fun commit(): Boolean = transaction("COMMIT", false).first == 1
        fun rollback(): Boolean = transaction("ROLLBACK", false).first == 1

        fun beginTransaction(): Pair<Int, String> {
            val err = errBuffer()
            val rc = lib.sqlite_core_begin_transaction(handle, err, 512)
            return rc to (if (rc == 1) "" else errText(err))
        }

        fun commitTransaction(): Pair<Int, String> {
            val err = errBuffer()
            val rc = lib.sqlite_core_commit_transaction(handle, err, 512)
            return rc to (if (rc == 1) "" else errText(err))
        }

        fun rollbackTransaction(): Pair<Int, String> {
            val err = errBuffer()
            val rc = lib.sqlite_core_rollback_transaction(handle, err, 512)
            return rc to (if (rc == 1) "" else errText(err))
        }

        fun nonQuery(sql: String, params: ParamSet? = null): Pair<Int, String> {
            val err = errBuffer()
            val ptr = params?.pointer()
            val rc = lib.sqlite_core_execute_non_query(handle, sql, ptr, params?.count ?: 0, err, 512)
            return rc to (if (rc < 0) errText(err) else "")
        }

        fun scalarInt(sql: String): Long? {
            val out = paramStruct()
            val rc = lib.sqlite_core_execute_scalar(handle, sql, null, 0, out, null, 0)
            if (rc != OK || typeOf(out) != TYPE_INT) return null
            return out.getLong(VALUE_OFFSET.toLong())
        }

        /** Same call with the destination buffer the C API needs for TEXT. */
        fun scalarText(sql: String): Triple<Int, String, Int> {
            val out = paramStruct()
            val buffer = Memory(512)
            buffer.clear()
            out.setPointer(VALUE_OFFSET.toLong(), buffer)
            out.setInt(VALUE_OFFSET + PTR.toLong(), 0)
            out.setInt(VALUE_OFFSET + PTR.toLong() + 4, 512)
            val err = errBuffer()
            val rc = lib.sqlite_core_execute_scalar(handle, sql, null, 0, out, err, 512)
            val value = if (rc == OK && typeOf(out) == TYPE_TEXT) {
                val bytes = buffer.getByteArray(0, 512)
                var end = 0
                while (end < bytes.size && bytes[end] != 0.toByte()) end++
                String(bytes, 0, end, Charsets.UTF_8)
            } else ""
            return Triple(rc, value, typeOf(out))
        }

        /** sqlite_core_execute_reader with a caller-supplied callback. */
        fun readRows(sql: String, callback: RowCallback): Pair<Int, String> {
            val err = errBuffer()
            val rc = lib.sqlite_core_execute_reader(handle, sql, null, 0, callback, null, err, 512)
            return rc to (if (rc < 0) errText(err) else "")
        }

        /** Convenience: the counting trampoline fills a RowCounter. */
        fun readRows(sql: String, counter: RowCounter): Pair<Int, String> {
            val callback = RowCallback { _, columns, hasrows, _ ->
                if (hasrows == 1) {
                    counter.rows++
                    counter.columns = columns
                }
                0
            }
            return readRows(sql, callback)
        }

        fun readerJson(sql: String, params: ParamSet? = null): Pair<Int, String> {
            val err = errBuffer()
            val out = pointerOut()
            val ptr = params?.pointer()
            val rows = lib.sqlite_core_execute_reader_to_json(handle, sql, ptr, params?.count ?: 0, out, err, 512)
            return rows to takeString(out.value)
        }

        fun insertJson(table: String, json: String): Pair<Int, String> {
            val err = errBuffer()
            val rows = lib.sqlite_core_insert_json(handle, table, json, err, 512)
            return rows to (if (rows < 0) errText(err) else "")
        }

        fun getBlob(sql: String): Triple<Int, ByteArray, Int> {
            var capacity = 4096
            var buffer = Memory(capacity.toLong())
            while (true) {
                buffer.clear()
                val copied = intOut()
                val rc = lib.sqlite_core_get_blob(handle, sql, buffer, capacity, copied)
                if (rc == BUFFER_TOO_SMALL && capacity < 64 * 1024 * 1024) {
                    capacity *= 2
                    buffer = Memory(capacity.toLong())
                    continue
                }
                val data = if (rc == OK && copied.value > 0) buffer.getByteArray(0, copied.value) else ByteArray(0)
                return Triple(rc, data, copied.value)
            }
        }

        fun insertBlob(table: String, column: String, rowid: Long, file: String, chunk: Int = 0): Triple<Int, Int, String> {
            val err = errBuffer()
            val written = intOut()
            val rc = lib.sqlite_core_insert_blob(handle, table, column, rowid, file, chunk, written, err, 512)
            return Triple(rc, written.value, if (rc < 0) errText(err) else "")
        }

        fun updateBlob(table: String, column: String, rowid: Long, file: String, chunk: Int = 0): Triple<Int, Int, String> {
            val err = errBuffer()
            val written = intOut()
            val rc = lib.sqlite_core_update_blob(handle, table, column, rowid, file, chunk, written, err, 512)
            return Triple(rc, written.value, if (rc < 0) errText(err) else "")
        }

        val isReadonly: Int get() = lib.sqlite_core_is_readonly(handle)
        val lastInsertRowid: Long get() = lib.sqlite_core_last_insert_rowid(handle)
        val errorCode: Int get() = lib.sqlite_core_error_code(handle)
        val errorMessage: String get() = staticString(lib.sqlite_core_error_message(handle))

        fun setBusyTimeout(ms: Int): Int = lib.sqlite_core_set_busy_timeout(handle, ms)

        /** sqlite_core_close: 1 on success (flag-style return, see
         *  docs/behavior.md). */
        fun close(): Int = lib.sqlite_core_close(handle)
    }
}