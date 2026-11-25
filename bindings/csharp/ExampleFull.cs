// Canonical libsqlite3 flow in C# (see bindings/README.md).
// Run: copy the built libsqlite3.dll next to this exe, then
//   dotnet run --project . -- ../../bin/x64-Windows/shared/Release/libsqlite3.dll
// or set the environment variable LibSqlite_DllPath before running.

using System.Runtime.InteropServices;
using LibSqlite;

var failures = 0;
var checks = 0;

void Check(bool cond, string msg)
{
    checks++;
    Console.WriteLine((cond ? "  ok   - " : "  FAIL - ") + msg);
    if (!cond) failures++;
}

// A fixed DllImport name is resolved by the OS from the app directory; the
// path argument of this example switches the OS-level search via LoadLibrary
// is not possible for DllImport, so we always copy the DLL next to the exe.
Console.WriteLine("libsqlite3 C# binding");
Console.WriteLine("  library " + Lib.PtrToStringAnsi(Lib.Version()) +
                  " / engine " + Lib.PtrToStringAnsi(Lib.SqliteVersion()));

var err = new byte[256];
var rc = 0;

var db = Lib.sqlite_core_open_or_create("csharp_demo.db", ref rc);
Check(db != IntPtr.Zero && rc == Lib.SQLITE_OK, "open_or_create");
if (db == IntPtr.Zero) return 1;

const string fixture = "csharp_fixture.bin";
const string oversize = "csharp_oversize.bin";
const int size = 1024;
File.WriteAllBytes(fixture, Enumerable.Range(0, size).Select(i => (byte)i).ToArray());
File.WriteAllBytes(oversize, Enumerable.Range(0, size * 2).Select(i => (byte)i).ToArray());

Check(Lib.sqlite_core_execute_command(db,
          "CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)",
          err, err.Length) == Lib.SQLITE_OK, "create table");

var pars = new[]
{
    Lib.Int("id", 42),
    Lib.Text("name", "Ada"),
};
// keep the text buffers alive during the call
var handles = pars.Where(p => p.Type == Lib.TYPE_TEXT)
                   .Select(p => p.Value.BlobOrText.Ptr).ToArray();
try
{
    Check(Lib.sqlite_core_execute_non_query(db,
              "INSERT INTO person(id, name) VALUES (:id, :name)",
              pars, pars.Length, err, err.Length) == 1, "named parameter insert");
}
finally
{
    foreach (var h in handles) Marshal.FreeHGlobal(h);
}

int seen = 0;
Lib.RowCallback callback = (stmt, columns, hasrows, ctx) =>
{
    if (hasrows != 0) seen++;
    return 0;
};

Check(Lib.sqlite_core_execute_reader(db, "SELECT id, name FROM person ORDER BY id",
          null, 0, callback, IntPtr.Zero, err, err.Length) == 1 && seen == 1,
      "reader visits the row");

var outParam = default(Lib.Param);
Check(Lib.sqlite_core_execute_scalar(db, "SELECT COUNT(*) FROM person", null, 0,
          ref outParam, err, err.Length) == Lib.SQLITE_OK, "scalar executes");

var jsonPtr = IntPtr.Zero;
var rows = Lib.sqlite_core_execute_reader_to_json(db, "SELECT id, name FROM person ORDER BY id",
    null, 0, out jsonPtr, err, err.Length);
var doc = Lib.PtrToStringAnsi(jsonPtr) ?? "";
Lib.sqlite_core_string_free(jsonPtr);
Check(rows == 1 && doc.StartsWith("["), "reader to json");
Console.WriteLine("  json: " + doc);

Check(Lib.sqlite_core_insert_json(db, "person",
          "[{\"id\": 2, \"name\": \"Line1\"}, {\"id\": 3, \"name\": \"Line2\"}]",
          err, err.Length) == 2, "insert_json batch");

var written = 0;
Check(Lib.sqlite_core_insert_blob(db, "person", "data", 2, fixture, 0,
          ref written, err, err.Length) == size && written == size,
      "insert_blob streams the file");

var sink = new byte[size];
var copied = 0;
Check(Lib.sqlite_core_get_blob(db, "SELECT data FROM person WHERE id = 2",
          sink, size, ref copied) == Lib.SQLITE_OK && copied == size,
      "get_blob reads it back");
Check(sink.SequenceEqual(File.ReadAllBytes(fixture)), "byte-for-byte round trip");

Check(Lib.sqlite_core_last_insert_rowid(db) > 0, "last_insert_rowid");

Check(Lib.sqlite_core_commit_transaction(db, err, err.Length) != Lib.SQLITE_OK,
      "commit without transaction fails");
Check(Lib.sqlite_core_begin_transaction(db, err, err.Length) == 1, "begin");
Lib.sqlite_core_execute_non_query(db, "INSERT INTO person(name) VALUES ('Temp')",
    null, 0, err, err.Length);
Check(Lib.sqlite_core_rollback_transaction(db, err, err.Length) == 1, "rollback");
Lib.sqlite_core_execute_scalar(db, "SELECT COUNT(*) FROM person", null, 0,
    ref outParam, err, err.Length);
Check(outParam.Value.IVal == 3, "rollback discarded the row");

Check(Lib.sqlite_core_update_blob(db, "person", "data", 2, oversize, 0,
          ref written, err, err.Length) == Lib.SQLITE_CORE_BLOB_TOO_SMALL,
      "update_blob refuses to exceed the column");

var rc2 = 0;
Check(Lib.sqlite_core_open_readonly("missing.db", ref rc2) == IntPtr.Zero &&
      rc2 == Lib.SQLITE_CANTOPEN, "open missing file fails");
Check(Lib.sqlite_core_execute_non_query(db, "SELECT 1; SELECT 2", null, 0,
          err, err.Length) == Lib.SQLITE_MISUSE, "multi-statement rejected");

// error diagnostics + busy timeout: the three one-liner symbols every
    // binding must exercise (bindings/CONTRACT.md). error_message is static
    // library data: PtrToStringAnsi reads it and frees nothing.
    Check(Lib.sqlite_core_set_busy_timeout(db, 1000) == 1, "set_busy_timeout");
    Check(Lib.sqlite_core_error_code(db) != 0, "error_code reports the engine failure");
    Check(Lib.sqlite_core_error_code_extended(db) != 0, "error_code_extended reports the engine failure");
    Check(!string.IsNullOrEmpty(Lib.PtrToStringAnsi(Lib.sqlite_core_error_message(db))),
          "error_message reports the last failure");

    Check(Lib.sqlite_core_close(db) == 1, "close");

foreach (var f in new[] { "csharp_demo.db", fixture, oversize })
    File.Delete(f);

Console.WriteLine($"\n{checks} checks, {failures} failures");
return failures;