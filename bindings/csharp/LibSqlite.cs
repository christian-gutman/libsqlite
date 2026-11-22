// libsqlite3 binding for C# / .NET (P/Invoke).
// Complete frozen public API: the 34 symbols of sqlite.def.
// Windows: libsqlite3.dll; Linux: libsqlite3.so; macOS: libsqlite3.dylib.

using System;
using System.Runtime.InteropServices;

namespace LibSqlite;

public static class Lib
{
    private const string Dll = "libsqlite3";

    // ---- result / error codes -----------------------------------------
    public const int SQLITE_OK = 0;
    public const int SQLITE_MISUSE = 21;
    public const int SQLITE_CANTOPEN = 14;
    public const int SQLITE_CORE_BUFFER_TOO_SMALL = -2;
    public const int SQLITE_CORE_BLOB_TOO_SMALL = -3;

    // ---- parameter types / directions ---------------------------------
    public const int TYPE_NULL = 0;
    public const int TYPE_INT = 2;
    public const int TYPE_DOUBLE = 4;
    public const int TYPE_TEXT = 6;
    public const int TYPE_BLOB = 8;

    public const int DIRECTION_INPUT = 1;
    public const int DIRECTION_OUTPUT = 2;
    public const int DIRECTION_INPUTOUTPUT = 3;

    // ---- functions -----------------------------------------------------
    [DllImport(Dll, EntryPoint = "sqlite_core_version", CharSet = CharSet.Ansi)]
    public static extern IntPtr Version();

    [DllImport(Dll, EntryPoint = "sqlite_core_sqlite_version", CharSet = CharSet.Ansi)]
    public static extern IntPtr SqliteVersion();

    [DllImport(Dll)] public static extern int sqlite_core_error_code(IntPtr db);
    [DllImport(Dll)] public static extern int sqlite_core_error_code_extended(IntPtr db);
    [DllImport(Dll)] public static extern IntPtr sqlite_core_error_message(IntPtr db);

    [DllImport(Dll, EntryPoint = "sqlite_core_string_format", CharSet = CharSet.Ansi)]
    public static extern IntPtr StringFormat(string format, __arglist);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern IntPtr sqlite_core_string_concat(string a, string b);
    [DllImport(Dll)] public static extern void sqlite_core_string_free(IntPtr s);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern IntPtr sqlite_core_string_utf8_to_ansi(string s);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern IntPtr sqlite_core_string_ansi_to_utf8(string s);

    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern IntPtr sqlite_core_open(string path, ref int outRc);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern IntPtr sqlite_core_open_ex(string path, int flags, ref int outRc);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern IntPtr sqlite_core_open_readonly(string path, ref int outRc);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern IntPtr sqlite_core_open_or_create(string path, ref int outRc);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern IntPtr sqlite_core_open_uri_readonly(string path, ref int outRc);
    [DllImport(Dll)] public static extern int sqlite_core_close(IntPtr db);
    [DllImport(Dll)] public static extern int sqlite_core_is_readonly(IntPtr db);
    [DllImport(Dll)] public static extern long sqlite_core_last_insert_rowid(IntPtr db);
    [DllImport(Dll)] public static extern int sqlite_core_set_busy_timeout(IntPtr db, int ms);

    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern IntPtr sqlite_core_param_clean_prefix(string name);

    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern int sqlite_core_execute_command(IntPtr db, string sql, byte[] errBuf, int max);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern int sqlite_core_execute_transaction(IntPtr db, string sql, int txexpected,
                                                             byte[] errBuf, int max);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern int sqlite_core_execute_non_query(IntPtr db, string sql,
                                                           [In] Param[]? parameters, int count,
                                                           byte[]? errBuf, int errBufMax);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern int sqlite_core_execute_scalar(IntPtr db, string sql,
                                                        [In] Param[]? parameters, int count,
                                                        ref Param outScalar, byte[]? errBuf,
                                                        int errBufMax);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern int sqlite_core_execute_reader(IntPtr db, string sql,
                                                        [In] Param[]? parameters, int count,
                                                        RowCallback callback, IntPtr ctx,
                                                        byte[]? errBuf, int errBufMax);

    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern int sqlite_core_begin_transaction(IntPtr db, byte[]? errBuf, int max);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern int sqlite_core_commit_transaction(IntPtr db, byte[]? errBuf, int max);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern int sqlite_core_rollback_transaction(IntPtr db, byte[]? errBuf, int max);

    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern int sqlite_core_execute_reader_to_json(IntPtr db, string sql,
                                                                [In] Param[]? parameters, int count,
                                                                out IntPtr outJson, byte[]? errBuf,
                                                                int errBufMax);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern int sqlite_core_insert_json(IntPtr db, string table, string json,
                                                     byte[]? errBuf, int max);

    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern int sqlite_core_get_blob(IntPtr db, string sql, byte[]? dest, int maxLen,
                                                  ref int copied);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern int sqlite_core_insert_blob(IntPtr db, string table, string column,
                                                     long rowid, string filePath, int chunkSize,
                                                     ref int written, byte[]? errBuf, int max);
    [DllImport(Dll, CharSet = CharSet.Ansi)]
    public static extern int sqlite_core_update_blob(IntPtr db, string table, string column,
                                                     long rowid, string filePath, int chunkSize,
                                                     ref int written, byte[]? errBuf, int max);

    // ---- parameter structure -------------------------------------------
    [StructLayout(LayoutKind.Sequential)]
    public struct Param
    {
        public string? Name;
        public int NoInsensitive;
        public int Type;
        public int Direction;
        public ValueUnion Value;
    }

    [StructLayout(LayoutKind.Explicit)]
    public struct ValueUnion
    {
        [FieldOffset(0)] public double DVal;
        [FieldOffset(0)] public long IVal;
        [FieldOffset(0)] public BlobOrText BlobOrText;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct BlobOrText
    {
        public IntPtr Ptr;
        public int Len;
        public int MaxLen;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate int RowCallback(IntPtr stmt, int columns, int hasrows, IntPtr ctx);

    // ---- helpers --------------------------------------------------------
    public static string PtrToStringAnsi(IntPtr p) =>
        p == IntPtr.Zero ? string.Empty : (Marshal.PtrToStringAnsi(p) ?? string.Empty);

    public static string PtrToStringFree(IntPtr p)
    {
        if (p == IntPtr.Zero) return string.Empty;
        try { return Marshal.PtrToStringAnsi(p) ?? string.Empty; }
        finally { sqlite_core_string_free(p); }
    }

    public static Param Text(string name, string value) => new()
    {
        Name = name,
        Type = TYPE_TEXT,
        Direction = DIRECTION_INPUT,
        Value = new ValueUnion { BlobOrText = new BlobOrText { Ptr = Marshal.StringToHGlobalAnsi(value), Len = -1 } }
    };

    public static Param Int(string name, long value) => new()
    {
        Name = name,
        Type = TYPE_INT,
        Direction = DIRECTION_INPUT,
        Value = new ValueUnion { IVal = value }
    };
}