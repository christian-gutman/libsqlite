// Canonical libsqlite3 flow in Java (see bindings/README.md).
//
// Usage: java -cp classes;jna.jar ExampleFull [path-to-libsqlite3]
//        java -cp classes:jna.jar  ExampleFull [path-to-libsqlite3]
// Exit code = number of failed checks.

import com.sun.jna.Memory;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.IntByReference;
import com.sun.jna.ptr.PointerByReference;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.util.Arrays;

public final class ExampleFull {

    private static final String DB = "java_demo.db";
    private static final String FIXTURE = "java_fixture.bin";
    private static final String OVERSIZE = "java_oversize.bin";

    private static int total = 0;
    private static int failures = 0;

    private static LibSqlite.SqliteCore lib;

    private ExampleFull() {
    }

    private static void check(boolean cond, String msg) {
        total++;
        System.out.println((cond ? "  ok   - " : "  FAIL - ") + msg);
        if (!cond) {
            failures++;
        }
    }

    public static void main(String[] args) {
        lib = LibSqlite.load(args.length > 0 ? args[0] : null);

        System.out.println("libsqlite3 Java binding");
        System.out.println("  library " + LibSqlite.string(lib.sqlite_core_version())
                + " / engine " + LibSqlite.string(lib.sqlite_core_sqlite_version()));

        for (String f : Arrays.asList(DB, FIXTURE, OVERSIZE)) {
            new File(f).delete();
        }
        byte[] fixture = new byte[1024];
        for (int i = 0; i < fixture.length; i++) {
            fixture[i] = (byte) (i & 0xFF);      // includes 0x00 bytes
        }
        try {
            Files.write(new File(FIXTURE).toPath(), fixture);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        byte[] err = new byte[256];

        // 1. open_or_create
        IntByReference rc = new IntByReference();
        Pointer db = lib.sqlite_core_open_or_create(DB, rc);
        check(db != null && rc.getValue() == LibSqlite.SQLITE_OK, "open_or_create");
        if (db == null) {
            System.out.println(System.lineSeparator() + total + " checks, "
                    + failures + " failures");
            System.exit(failures);
        }

        // 2. DDL
        check(lib.sqlite_core_execute_command(db,
                "CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)",
                err, err.length) == LibSqlite.SQLITE_OK, "create table");

        // 3. named parameters (union of the C struct)
        LibSqlite.Param[] params = LibSqlite.paramArray(2);
        params[0].asInt("id", 42);
        params[1].asText("name", "Ada");
        params[0].write();
        params[1].write();
        check(lib.sqlite_core_execute_non_query(db,
                "INSERT INTO person(id, name) VALUES (:id, :name)",
                params, params.length, err, err.length) == 1,
                "named parameter insert");
        LibSqlite.releaseParams();

        // 4. reader callback (rows arrive as JSON: the exported surface
        //    cannot call the engine column API on a DLL build)
        final int[] seen = {0};
        LibSqlite.RowCallback cb = new LibSqlite.RowCallback() {
            @Override
            public int callback(Pointer stmt, int columns, int hasrows,
                    Pointer ctx) {
                if (hasrows == 1) {
                    seen[0]++;
                }
                return 0;
            }
        };
        check(lib.sqlite_core_execute_reader(db,
                "SELECT id, name FROM person ORDER BY id",
                null, 0, cb, null, err, err.length) == 1 && seen[0] == 1,
                "reader visits the row");

        // 5. scalar
        LibSqlite.Param out = new LibSqlite.Param();
        out.type = LibSqlite.SQLITE_CORE_TYPE_INT;
        out.write();
        check(lib.sqlite_core_execute_scalar(db,
                "SELECT COUNT(*) FROM person", null, 0, out, err,
                err.length) == LibSqlite.SQLITE_OK, "scalar counts rows");
        out.read();
        out.value.read();
        check(out.value.iVal == 1L, "scalar value is 1");

        // 6. reader to json
        PointerByReference json = new PointerByReference();
        int rows = lib.sqlite_core_execute_reader_to_json(db,
                "SELECT id, name FROM person ORDER BY id", null, 0, json,
                err, err.length);
        String doc = LibSqlite.takeAndFree(lib, json.getValue());
        check(rows == 1 && doc != null && doc.startsWith("["),
                "reader to json");
        System.out.println("  json: " + doc);

        // 7. json in
        check(lib.sqlite_core_insert_json(db, "person",
                "[{\"id\": 2, \"name\": \"Line1\"}, {\"id\": 3, \"name\": \"Line2\"}]",
                err, err.length) == 2, "insert_json batch");

        // 8. blob round trip (chunked streaming write + read back)
        IntByReference written = new IntByReference();
        int rcBlob = lib.sqlite_core_insert_blob(db, "person", "data", 2,
                FIXTURE, 0, written, err, err.length);
        System.out.println("  (insert_blob rc=" + rcBlob + " written="
                + written.getValue() + " err=" + LibSqlite.errorText(err).trim() + ")");
        check(rcBlob == fixture.length && written.getValue() == fixture.length,
                "insert_blob streams the file");

        Memory sink = new Memory(fixture.length);
        sink.clear();
        IntByReference copied = new IntByReference();
        int rcGet = lib.sqlite_core_get_blob(db,
                "SELECT data FROM person WHERE id = 2", sink,
                fixture.length, copied);
        byte[] back = sink.getByteArray(0, copied.getValue());
        byte[] fromDisk = readBytes(FIXTURE);
        check(rcGet == LibSqlite.SQLITE_OK && copied.getValue() == fixture.length,
                "get_blob reads it back");
        check(Arrays.equals(back, fromDisk), "byte-for-byte round trip");

        check(lib.sqlite_core_last_insert_rowid(db) > 0,
                "last_insert_rowid");

        // 9. transactions
        check(lib.sqlite_core_commit_transaction(db, err, err.length) != LibSqlite.SQLITE_OK,
                "commit without transaction fails");
        check(lib.sqlite_core_begin_transaction(db, err, err.length) == 1,
                "begin");
        lib.sqlite_core_execute_non_query(db,
                "INSERT INTO person(name) VALUES ('Temp')", null, 0, err,
                err.length);
        check(lib.sqlite_core_rollback_transaction(db, err, err.length) == 1,
                "rollback");
        LibSqlite.Param counted = new LibSqlite.Param();
        counted.type = LibSqlite.SQLITE_CORE_TYPE_INT;
        counted.write();
        lib.sqlite_core_execute_scalar(db, "SELECT COUNT(*) FROM person", null,
                0, counted, err, err.length);
        counted.read();
        counted.value.read();
        check(counted.value.iVal == 3L, "rollback discarded the row");

        // 10. oversize update is reported (column smaller than the file)
        byte[] big = new byte[fixture.length * 2];
        try {
            Files.write(new File(OVERSIZE).toPath(), big);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
        IntByReference written2 = new IntByReference();
        int rcOver = lib.sqlite_core_update_blob(db, "person", "data", 2,
                OVERSIZE, 0, written2, err, err.length);
        check(rcOver == LibSqlite.SQLITE_CORE_BLOB_TOO_SMALL,
                "update_blob refuses to exceed the column");
        new File(OVERSIZE).delete();

        // 11. error paths
        IntByReference rc2 = new IntByReference();
        Pointer missing = lib.sqlite_core_open_readonly("missing.db", rc2);
        check(missing == null && rc2.getValue() == LibSqlite.SQLITE_CANTOPEN,
                "open missing file fails");
        check(lib.sqlite_core_execute_non_query(db,
                "SELECT 1; SELECT 2", null, 0, err, err.length) == LibSqlite.SQLITE_MISUSE,
                "multi-statement rejected");

        // 12. close
        check(lib.sqlite_core_close(db) == 1, "close");

        for (String f : Arrays.asList(DB, FIXTURE)) {
            new File(f).delete();
        }
        System.out.println(System.lineSeparator() + total + " checks, "
                + failures + " failures");
        System.exit(failures);
    }

    private static byte[] readBytes(String path) {
        try {
            return Files.readAllBytes(new File(path).toPath());
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}