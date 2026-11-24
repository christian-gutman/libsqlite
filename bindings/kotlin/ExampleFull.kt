// Canonical libsqlite3 flow in Kotlin (see bindings/README.md).
//
// Usage: kotlin -cp .:jna.jar ExampleFullKt [path-to-libsqlite3]
// Exit code = number of failed checks.

import Sqlite

const val DB = "kotlin_demo.db"
const val FIXTURE = "kotlin_fixture.bin"
const val OVERSIZE = "kotlin_oversize.bin"

var total = 0
var failures = 0

fun check(cond: Boolean, msg: String) {
    total++
    println((if (cond) "  ok   - " else "  FAIL - ") + msg)
    if (!cond) failures++
}

fun cstr(text: String) = Sqlite.cstring(text)

fun main(args: Array<String>) {
    Sqlite.load(if (args.isNotEmpty()) args[0] else null)

    println("libsqlite3 Kotlin binding")
    println("  library ${Sqlite.version()} / engine ${Sqlite.sqliteVersion()}")

    for (path in listOf(DB, FIXTURE, OVERSIZE)) java.io.File(path).delete()
    val fixture = ByteArray(1024) { (it and 0xFF).toByte() }   // includes 0x00
    java.io.File(FIXTURE).writeBytes(fixture)

    // 1. open_or_create
    val rc = Sqlite.intOut()
    val db = Sqlite.lib.sqlite_core_open_or_create(DB, rc)
    check(rc.value == Sqlite.OK, "open_or_create")
    if (rc.value != Sqlite.OK || db == null) {
        println("\n$total checks, $failures failures")
        System.exit(failures)
    }
    val connection = Sqlite.Connection(db!!)

    // 2. DDL
    check(connection.command(
        "CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)").first == Sqlite.OK,
        "create table")

    // 3. named parameters (the union of the C struct)
    val params = Sqlite.ParamSet().integer("id", 42).text("name", "Ada")
    check(connection.nonQuery("INSERT INTO person(id, name) VALUES (:id, :name)", params).first == 1,
        "named parameter insert")

    // 4. reader: a JNA Callback implemented in Kotlin counts the visited rows
    val counter = Sqlite.RowCounter()
    check(connection.readRows("SELECT id, name FROM person ORDER BY id", counter).first == 1 &&
          counter.rows == 1 && counter.columns == 2,
        "reader visits the row")

    // 5. scalar
    check(connection.scalarInt("SELECT COUNT(*) FROM person") == 1L, "scalar counts rows")

    // 5b. scalar as text (the C out struct doubles as the output, so a text
    // result needs a destination buffer)
    val (rcText, name, type) = connection.scalarText("SELECT name FROM person WHERE id = 42")
    check(rcText == Sqlite.OK && type == Sqlite.TYPE_TEXT && name == "Ada", "scalar reads text")

    // 6. reader to json
    val (rows, doc) = connection.readerJson("SELECT id, name FROM person ORDER BY id")
    check(rows == 1 && doc.startsWith("["), "reader to json")
    println("  json: $doc")

    // 6b. sqlite_core_json_writer_callback is part of the bound surface
    // (see the Core interface in libsqlite.kt): a C function pointer cannot
    // be obtained from an interface-bound JNA method, so the example
    // verifies the JSON path through execute_reader_to_json, like the Java,
    // C#, Go and C++ bindings do.

    // 7. json in
    check(connection.insertJson("person",
        "[{\"id\": 2, \"name\": \"Line1\"}, {\"id\": 3, \"name\": \"Line2\"}]").first == 2,
        "insert_json batch")

    // 8. blob round trip (chunked streaming write + read back)
    val (rcBlob, written, _) = connection.insertBlob("person", "data", 2, FIXTURE)
    println("  (insert_blob rc=$rcBlob written=$written)")
    check(rcBlob == fixture.size && written == fixture.size, "insert_blob streams the file")

    val (rcGet, back, copied) = connection.getBlob("SELECT data FROM person WHERE id = 2")
    check(rcGet == Sqlite.OK && copied == fixture.size && back.contentEquals(fixture),
        "get_blob reads it back")
    check(back.contentEquals(fixture), "byte-for-byte round trip")

    check(connection.lastInsertRowid > 0, "last_insert_rowid")

    // 9. transactions
    check(!connection.commit(), "commit without transaction fails")
    check(connection.begin(), "begin")
    connection.nonQuery("INSERT INTO person(name) VALUES ('Temp')")
    check(connection.rollback(), "rollback")
    check(connection.scalarInt("SELECT COUNT(*) FROM person") == 3L,
        "rollback discarded the row")

    // 10. oversize update is reported (column smaller than the file)
    java.io.File(OVERSIZE).writeBytes(ByteArray(fixture.size * 2))
    check(connection.updateBlob("person", "data", 2, OVERSIZE).first == Sqlite.BLOB_TOO_SMALL,
        "update_blob refuses to exceed the column")
    java.io.File(OVERSIZE).delete()

    // 11. error paths
    val rcMissing = Sqlite.intOut()
    val missing = Sqlite.lib.sqlite_core_open_readonly("missing.db", rcMissing)
    check(missing == null && rcMissing.value == Sqlite.CANTOPEN, "open missing file fails")
    check(connection.nonQuery("SELECT 1; SELECT 2").first == Sqlite.MISUSE,
        "multi-statement rejected")

    // 12. the rest of the frozen surface
    check(connection.command("CREATE TABLE person(id INTEGER PRIMARY KEY)").first == Sqlite.ERROR,
        "duplicate create fails")
    check(connection.errorMessage.isNotEmpty(), "error_message reports the last failure")
    check(connection.isReadonly == 0, "is_readonly false on a read-write handle")
    check(connection.setBusyTimeout(1000) == 1, "set_busy_timeout")
    check(Sqlite.paramCleanPrefix(":name") == "name", "param_clean_prefix strips the prefix")
    check(connection.errorCode >= 0, "error_code reports a code")
    check(Sqlite.stringConcat("lib", "sqlite") == "libsqlite",
        "string_concat is released with string_free")
    check(Sqlite.stringFormat("%s says hi", "kotlin") == "kotlin says hi", "string_format")
    check(Sqlite.ansiToUtf8("Ada") == "Ada", "ansi_to_utf8 round trips ascii")
    check(Sqlite.utf8ToAnsi("Ada") == "Ada", "utf8_to_ansi round trips ascii")
    // sqlite_core_open_ex with SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
    val rcEx = Sqlite.intOut()
    val db2 = Sqlite.lib.sqlite_core_open_ex("${DB}2", 0x2 or 0x4, rcEx)
    check(rcEx.value == Sqlite.OK && db2 != null, "open_ex creates with read-write|create flags")
    Sqlite.lib.sqlite_core_close(db2)
    java.io.File("${DB}2").delete()

    // 13. close
    check(connection.close() == 1, "close")

    for (path in listOf(DB, FIXTURE)) java.io.File(path).delete()
    println("\n$total checks, $failures failures")
    System.exit(failures)
}