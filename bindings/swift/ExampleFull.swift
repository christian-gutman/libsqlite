// Canonical libsqlite3 flow in Swift (see bindings/README.md).
//
// Build & run (Linux):
//   swiftc -I include LibSqlite.swift example_full.swift \
//       -L <libdir> -l:libsqlite3.so -Xlinker -rpath -Xlinker <libdir>
//   ./main
// Exit code = number of failed checks.

import Foundation

let DB = "swift_demo.db"
let FIXTURE = "swift_fixture.bin"
let OVERSIZE = "swift_oversize.bin"

var total = 0
var failures = 0

func check(_ cond: Bool, _ msg: String) {
    total += 1
    if cond {
        print("  ok   - \(msg)")
    } else {
        print("  FAIL - \(msg)")
        failures += 1
    }
}

// The canonical flow. Swift only allows executable statements at the top
// level of main.swift, so the flow lives in a function that main.swift calls
// and its exit code is the number of failed checks.
func exampleMain() throws -> Int32 {
    // 1024 bytes with 0x00 every 256 positions, like every other binding
    let fixture: [UInt8] = (0..<1024).map { UInt8($0 & 0xFF) }

for path in [DB, FIXTURE, OVERSIZE] {
    try? FileManager.default.removeItem(atPath: path)
}
try Data(fixture).write(to: URL(fileURLWithPath: FIXTURE))

print("libsqlite3 Swift binding")
print("  library \(libraryVersion()) / engine \(sqliteVersion())")

// 1. open_or_create
var openRc: Int32 = 0
guard let db = Connection(openOrCreate: DB, rc: &openRc) else {
    print("open_or_create failed (rc \(openRc))")
    exit(Int32(failures))
}
check(openRc == Sqlite.OK, "open_or_create")

// 2. DDL
let created = db.command("CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)")
check(created.rc == Sqlite.OK, "create table")

// 3. named parameters (the union of the C struct)
let params = ParamSet().integer("id", 42).text("name", "Ada")
let inserted = db.nonQuery("INSERT INTO person(id, name) VALUES (:id, :name)", params: params)
check(inserted.rc == 1, "named parameter insert")

// 4. reader: the C trampoline counts the visited rows (Swift cannot turn a
// context-capturing closure into a C function pointer), so the rows are
// counted here and printed through reader_to_json below
let read = db.readRows("SELECT id, name FROM person ORDER BY id", params: nil)
check(read.rc == 1 && read.rows == 1 && read.columns == 2, "reader visits the row")

// 5. scalar
let scalar = db.scalar("SELECT COUNT(*) FROM person", params: nil)
check(scalar.rc == Sqlite.OK, "scalar counts rows")
if case let .int(count) = scalar.value {
    check(count == 1, "scalar value is 1")
} else {
    check(false, "scalar value is 1")
}

// 5b. scalar as text (the C out struct doubles as the output)
let scalarText = db.scalarText("SELECT name FROM person WHERE id = 42", params: nil)
check(scalarText.rc == Sqlite.OK && scalarText.type == Sqlite.TYPE_TEXT && scalarText.value == "Ada",
      "scalar reads text")

// 6. reader to json
let doc = db.readerJson("SELECT id, name FROM person ORDER BY id", params: nil)
check(doc.rows == 1 && doc.json.hasPrefix("["), "reader to json")
print("  json: \(doc.json)")

// 6b. same rows through sqlite_core_json_writer_callback: the callback
// concatenates the row objects and execute_reader_to_json adds the array
// delimiters, so both documents match once the delimiters are stripped
let viaCallback = db.readerJsonCallback("SELECT id, name FROM person ORDER BY id", params: nil)
var body = doc.json
if body.hasPrefix("[\n") && body.hasSuffix("\n]") {
    body = String(body.dropFirst(2).dropLast(2))
}
check(viaCallback.rc == 1 && viaCallback.rows == 1 && viaCallback.body == body,
      "json_writer_callback builds the rows")

// 7. json in
let batch = db.insertJson(table: "person",
                          json: "[{\"id\": 2, \"name\": \"Line1\"}, {\"id\": 3, \"name\": \"Line2\"}]")
check(batch.rows == 2, "insert_json batch")

// 8. blob round trip (chunked streaming write + read back)
let writeResult = db.insertBlob(table: "person", column: "data", rowid: 2, file: FIXTURE)
print("  (insert_blob rc=\(writeResult.rc) written=\(writeResult.written) err=\(writeResult.error))")
check(writeResult.rc == Int32(fixture.count) && writeResult.written == Int32(fixture.count),
      "insert_blob streams the file")

let readBack = db.getBlob("SELECT data FROM person WHERE id = 2")
check(readBack.rc == Sqlite.OK && readBack.copied == Int32(fixture.count) && readBack.data == fixture,
      "get_blob reads it back")
check(readBack.data == fixture, "byte-for-byte round trip")

check(db.lastInsertRowid > 0, "last_insert_rowid")

// 9. transactions
check(!db.commit(), "commit without transaction fails")
check(db.beginTransaction().rc == 1, "begin")
_ = db.nonQuery("INSERT INTO person(name) VALUES ('Temp')", params: nil)
check(db.rollbackTransaction().rc == 1, "rollback")
check(db.scalarInt("SELECT COUNT(*) FROM person") == 3, "rollback discarded the row")

// 10. oversize update is reported (column smaller than the file)
try Data([UInt8](repeating: 0, count: fixture.count * 2)).write(to: URL(fileURLWithPath: OVERSIZE))
let overResult = db.updateBlob(table: "person", column: "data", rowid: 2, file: OVERSIZE)
check(overResult.rc == Sqlite.BLOB_TOO_SMALL, "update_blob refuses to exceed the column")
try? FileManager.default.removeItem(atPath: OVERSIZE)

// 11. error paths
var missingRc: Int32 = 0
let missing = Connection(readonly: "missing.db", rc: &missingRc)
check(missing == nil && missingRc == Sqlite.CANTOPEN, "open missing file fails")
check(db.nonQuery("SELECT 1; SELECT 2", params: nil).rc == Sqlite.MISUSE,
      "multi-statement rejected")

// 12. the rest of the frozen surface
let broken = db.command("CREATE TABLE person(id INTEGER PRIMARY KEY)")
check(broken.rc == Sqlite.ERROR, "duplicate create fails")
check(!db.errorMessage.isEmpty, "error_message reports the last failure")
    check(db.errorCode != 0, "error_code reports the engine failure")
    check(db.errorCodeExtended != 0, "error_code_extended reports the engine failure")
check(db.isReadonly == 0, "is_readonly false on a read-write handle")
check(db.setBusyTimeout(1000) == 1, "set_busy_timeout")
check(paramCleanPrefix(":name") == "name", "param_clean_prefix strips the prefix")
check(stringConcat("lib", "sqlite") == "libsqlite", "string_concat")
check(stringFormat("%s says hi", "swift") == "swift says hi", "string_format")
check(ansiToUtf8("Ada") == "Ada", "ansi_to_utf8 round trips ascii")
check(utf8ToAnsi("Ada") == "Ada", "utf8_to_ansi round trips ascii")

// 13. close
check(db.close() == 1, "close")
check(!db.isOpen, "connection is released after close")

for path in [DB, FIXTURE] {
    try? FileManager.default.removeItem(atPath: path)
}
print("\n\(total) checks, \(failures) failures")
return Int32(failures)
}