// libsqlite3 binding for Java (JNA).
//
// Complete surface of the frozen public API: the 34 symbols in
// src/sqlite/core/sqlite.def. No native compilation is needed - point the
// loader at the built library (libsqlite3.dll / libsqlite3.so /
// libsqlite3.dylib).
//
// Two JNA specifics are encoded here on purpose:
//   * "jna.encoding" is forced to UTF-8 before loading, because JNA
//     otherwise converts String parameters with the platform default
//     charset (POSIX/C locale in containers breaks every non-ASCII text).
//   * The SQLite allocator frees the returned char* values, so the
//     binding exposes Pointer + string helpers (string() for static
//     data, takeAndFree() for heap data) instead of String returns.

import com.sun.jna.Callback;
import com.sun.jna.Library;
import com.sun.jna.Memory;
import com.sun.jna.Native;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;
import com.sun.jna.Union;
import com.sun.jna.ptr.IntByReference;
import com.sun.jna.ptr.PointerByReference;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.List;

public final class LibSqlite {

    // --- return codes ----------------------------------------------------
    public static final int SQLITE_OK = 0;
    public static final int SQLITE_ERROR = 1;
    public static final int SQLITE_MISUSE = 21;
    public static final int SQLITE_CANTOPEN = 14;

    public static final int SQLITE_CORE_TYPE_NULL = 0;
    public static final int SQLITE_CORE_TYPE_INT = 2;
    public static final int SQLITE_CORE_TYPE_DOUBLE = 4;
    public static final int SQLITE_CORE_TYPE_TEXT = 6;
    public static final int SQLITE_CORE_TYPE_BLOB = 8;

    public static final int SQLITE_CORE_DIRECTION_INPUT = 1;
    public static final int SQLITE_CORE_DIRECTION_OUTPUT = 2;
    public static final int SQLITE_CORE_DIRECTION_INPUTOUTPUT = 3;

    public static final int SQLITE_CORE_BUFFER_TOO_SMALL = -2;
    public static final int SQLITE_CORE_BLOB_TOO_SMALL = -3;

    private LibSqlite() {
    }

    // --- structs (mirror of sqlite_core.h) --------------------------------

    /** union { double dVal; sqlite3_int64 iVal; struct { char *ptr; int len;
     *  int max_len; } blob_or_text; } value; */
    public static class ParamValue extends Union {
        public double dVal;
        public long iVal;
        public BlobOrText blob_or_text;

        public ParamValue() {
            this.blob_or_text = new BlobOrText();
        }

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("dVal", "iVal", "blob_or_text");
        }
    }

    public static class BlobOrText extends Structure {
        public Pointer ptr;
        public int len;
        public int max_len;

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("ptr", "len", "max_len");
        }
    }

    /** sqlite_core_param_t - reproduce the layout exactly (40 bytes on
     *  LP64, the 4 padding bytes after Direction belong to the union
     *  alignment and JNA inserts them). */
    public static class Param extends Structure {
        public Pointer name;
        public int no_insensitive;
        public int type;
        public int Direction;
        public ParamValue value;

        // JNA pitfall: any extra Java instance field counts as a mapped
        // field, so a Structure can only keep data that appears in
        // getFieldOrder(). The native strings for name/value are pinned in
        // the PINS list below instead, and released after the call.
        public Param() {
            this.value = new ParamValue();
        }

        @Override
        protected List<String> getFieldOrder() {
            return Arrays.asList("name", "no_insensitive", "type",
                    "Direction", "value");
        }

        /** Named TEXT parameter: len = -1 tells the library the pointer is
         *  NUL-terminated utf-8 text. */
        public Param asText(String name, String value) {
            this.name = cString(name);
            this.type = SQLITE_CORE_TYPE_TEXT;
            this.Direction = SQLITE_CORE_DIRECTION_INPUT;
            this.value.setType(BlobOrText.class);
            this.value.blob_or_text = new BlobOrText();
            this.value.blob_or_text.ptr = cString(value);
            this.value.blob_or_text.len = -1;
            this.value.blob_or_text.max_len = 0;
            return this;
        }

        /** Named INTEGER parameter (sqlite3_int64 is 64-bit everywhere). */
        public Param asInt(String name, long value) {
            this.name = cString(name);
            this.type = SQLITE_CORE_TYPE_INT;
            this.Direction = SQLITE_CORE_DIRECTION_INPUT;
            this.value.setType(long.class);
            this.value.iVal = value;
            return this;
        }
    }

    /** sqlite_core_row_callback_fn(stmt, columns, hasrows, ctx). */
    public interface RowCallback extends Callback {
        int callback(Pointer stmt, int columns, int hasrows, Pointer ctx);
    }

    // --- the 34 exported symbols ------------------------------------------

    public interface SqliteCore extends Library {

        // version
        Pointer sqlite_core_version();

        Pointer sqlite_core_sqlite_version();

        // error
        int sqlite_core_error_code(Pointer db);

        int sqlite_core_error_code_extended(Pointer db);

        Pointer sqlite_core_error_message(Pointer db);

        // string
        Pointer sqlite_core_string_format(String format, Object... args);

        Pointer sqlite_core_string_concat(String str1, String str2);

        void sqlite_core_string_free(Pointer input);

        Pointer sqlite_core_string_ansi_to_utf8(String ansi_str);

        Pointer sqlite_core_string_utf8_to_ansi(String utf8_str);

        // connection
        Pointer sqlite_core_open(String uripath, IntByReference out_rc);

        Pointer sqlite_core_open_ex(String uripath, int flags,
                IntByReference out_rc);

        Pointer sqlite_core_open_readonly(String uripath,
                IntByReference out_rc);

        Pointer sqlite_core_open_or_create(String uripath,
                IntByReference out_rc);

        Pointer sqlite_core_open_uri_readonly(String uripath,
                IntByReference out_rc);

        int sqlite_core_close(Pointer db);

        int sqlite_core_is_readonly(Pointer db);

        long sqlite_core_last_insert_rowid(Pointer db);

        int sqlite_core_set_busy_timeout(Pointer db, int ms);

        // parameter
        Pointer sqlite_core_param_clean_prefix(String name);

        // command
        int sqlite_core_execute_command(Pointer db, String sql_command,
                byte[] err_buf, int err_buf_max);

        int sqlite_core_execute_transaction(Pointer db, String sql_command,
                int txexpected, byte[] err_buf, int err_buf_max);

        int sqlite_core_execute_non_query(Pointer db, String sql,
                Param[] params, int paramCount, byte[] err_buf,
                int err_buf_max);

        int sqlite_core_execute_scalar(Pointer db, String sql,
                Param[] params, int paramCount, Param out_scalar_result,
                byte[] err_buf, int err_buf_max);

        int sqlite_core_execute_reader(Pointer db, String sql,
                Param[] params, int paramCount, RowCallback reader_callback,
                Pointer pUserContext, byte[] err_buf, int err_buf_max);

        // transaction
        int sqlite_core_begin_transaction(Pointer db, byte[] err_buf,
                int err_buf_max);

        int sqlite_core_commit_transaction(Pointer db, byte[] err_buf,
                int err_buf_max);

        int sqlite_core_rollback_transaction(Pointer db, byte[] err_buf,
                int err_buf_max);

        // json builder
        int sqlite_core_json_writer_callback(Pointer stmt, int columns,
                int hasrows, Pointer ctx);

        int sqlite_core_execute_reader_to_json(Pointer db, String sql,
                Param[] params, int paramCount,
                PointerByReference out_json, byte[] err_buf,
                int err_buf_max);

        int sqlite_core_insert_json(Pointer db, String table_name,
                String json_str, byte[] err_buf, int err_buf_max);

        // field (blob)
        int sqlite_core_get_blob(Pointer db, String sql, Pointer pDestBuffer,
                int maxLen, IntByReference pOutCopied);

        int sqlite_core_insert_blob(Pointer db, String table, String column,
                long rowid, String file_path, int chunk_size,
                IntByReference p_out_written, byte[] err_buf,
                int err_buf_max);

        int sqlite_core_update_blob(Pointer db, String table, String column,
                long rowid, String file_path, int chunk_size,
                IntByReference p_out_written, byte[] err_buf,
                int err_buf_max);
    }

    // --- loading ----------------------------------------------------------

    private static final String[] CANDIDATES = {
        "libsqlite3.dll",
        "libsqlite3.so",
        "libsqlite3.dylib",
        "../bin/x64-Windows/shared/Release/libsqlite3.dll",
        "../bin/x32-Windows/shared/Release/libsqlite3.dll",
        "../bin/x64-Linux/shared/Release/libsqlite3.so",
        "../bin/arm64-Darwin/shared/Release/libsqlite3.dylib",
        "../../bin/x64-Windows/shared/Release/libsqlite3.dll",
        "../../bin/x64-Linux/shared/Release/libsqlite3.so",
    };

    /** Resolves the library like the other bindings: explicit path, then the
     *  usual relative locations. */
    public static File resolve(String path) {
        if (path != null && !path.isEmpty()) {
            File f = new File(path);
            if (f.isFile()) {
                return f;
            }
            throw new UnsatisfiedLinkError("library not found: " + path);
        }
        for (String cand : CANDIDATES) {
            File f = new File(cand);
            if (f.isFile()) {
                return f;
            }
        }
        throw new UnsatisfiedLinkError(
                "libsqlite3 library not found; pass the path explicitly");
    }

    /** Loads the interface against a library file (absolute path or one of
     *  the candidates). */
    public static SqliteCore load(String path) {
        // Must happen before JNA maps any String: the container locale is
        // POSIX/C, and the default charset there is ASCII.
        if (System.getProperty("jna.encoding") == null) {
            System.setProperty("jna.encoding", "UTF-8");
        }
        File f = resolve(path);
        try {
            return Native.load(f.getAbsolutePath(), SqliteCore.class);
        } catch (UnsatisfiedLinkError byPath) {
            // Fallback: add the directory to JNA's search path and load by
            // file name (the name already carries the "lib" prefix and the
            // extension, so no mangling is needed).
            System.setProperty("jna.library.path", f.getParent());
            return Native.load(f.getName(), SqliteCore.class);
        }
    }

    // --- helpers -----------------------------------------------------------

    /** Native strings alive while a call may read them: cString() pins the
     *  memory here, releaseParams() drops it once the call returned. */
    private static final List<Memory> PINS = new java.util.ArrayList<Memory>();

    /** Releases the native strings pinned while filling parameters. */
    public static void releaseParams() {
        PINS.clear();
    }

    /** NUL-terminated utf-8 copy of a Java string in native memory, pinned
     *  until releaseParams() (JNA Structures cannot keep private fields). */
    public static Memory cString(String s) {
        byte[] bytes = (s == null ? "" : s)
                .getBytes(StandardCharsets.UTF_8);
        Memory m = new Memory(bytes.length + 1);
        m.write(0, bytes, 0, bytes.length);
        m.setByte(bytes.length, (byte) 0);
        PINS.add(m);
        return m;
    }

    /** Reads a char* owned by the library's static data (version,
     *  error_message, param_clean_prefix): never free it. */
    public static String string(Pointer p) {
        if (p == null) {
            return null;
        }
        return p.getString(0, "UTF-8");
    }

    /** Reads a char* allocated by the SQLite allocator and releases it with
     *  sqlite_core_string_free (never the host allocator). */
    public static String takeAndFree(SqliteCore lib, Pointer p) {
        if (p == null) {
            return null;
        }
        try {
            return p.getString(0, "UTF-8");
        } finally {
            lib.sqlite_core_string_free(p);
        }
    }

    /** Decodes an err_buf filled by the library as utf-8 text. */
    public static String errorText(byte[] buf) {
        int end = 0;
        while (end < buf.length && buf[end] != 0) {
            end++;
        }
        return end == 0 ? "" : new String(buf, 0, end,
                StandardCharsets.UTF_8);
    }

    /** Contiguous sqlite_core_param_t array (JNA only keeps native code
     *  happy when the array is allocated through toArray()). */
    public static Param[] paramArray(int count) {
        return (Param[]) new Param().toArray(count);
    }
}