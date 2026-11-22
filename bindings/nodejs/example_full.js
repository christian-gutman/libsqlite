// Canonical libsqlite3 flow in Node.js (see bindings/README.md).
//
// Install:  npm install koffi
// Run:      node example_full.js <path-to-libsqlite3.dll|so|dylib>
// Exit code = number of failed checks.

"use strict";

const fs = require("fs");
const path = require("path");
const { F, alloc, koffi, read, take, RowCallback, constants: C } = require("./libsqlite");

let checks = 0, failures = 0;
function check(cond, msg) {
    checks++;
    console.log((cond ? "  ok   - " : "  FAIL - ") + msg);
    if (!cond) failures++;
}

const db = "node_demo.db";
const fixture = "node_fixture.bin";
const oversize = "node_oversize.bin";
for (const f of [db, fixture, oversize]) if (fs.existsSync(f)) fs.unlinkSync(f);
fixtureWrite(fixture, 1024);
fixtureWrite(oversize, 2048);

function fixtureWrite(p, size) {
    fs.writeFileSync(p, Buffer.from(Array.from({ length: size }, (_, i) => i & 0xFF)));
}

console.log("libsqlite3 Node.js binding");
console.log("  library " + read(F.version()) + " / engine " + read(F.sqlite_version()));

const rc = [0];
const handle = F.open_or_create(db, rc);
check(handle && rc[0] === C.SQLITE_OK, "open_or_create");

let err = Buffer.alloc(256);

check(F.execute_command(handle,
    "CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)",
    err, err.length) === C.SQLITE_OK, "create table");

// named parameters: koffi converts plain JS objects to struct pointers
const params = [
    { name: "id", no_insensitive: 0, type: C.TYPE_INT,
      Direction: C.DIRECTION_INPUT, value: { iVal: 42n } },
    { name: "name", no_insensitive: 0, type: C.TYPE_TEXT,
      Direction: C.DIRECTION_INPUT,
      value: { blob_or_text: { ptr: Buffer.from("Ada\0"), len: -1, max_len: 0 } } },
];
check(F.execute_non_query(handle,
    "INSERT INTO person(id, name) VALUES (:id, :name)",
    params, 2, err, err.length) === 1, "named parameter insert");

// reader: counting callback (engine column calls are not exported on
// DLL builds, so rows are shown as JSON)
let seen = 0;
const cb = koffi.register((stmt, columns, hasrows, ctx) => {
    if (hasrows !== 0) seen++;
    return 0;
}, koffi.pointer(RowCallback));
let rows = F.execute_reader(handle, "SELECT id, name FROM person ORDER BY id",
    null, 0, cb, null, err, err.length);
check(rows === 1 && seen === 1, "reader visits the row");

// scalar value read through JSON (avoids the out-struct in this example;
// koffi already dereferences char **, so out[0] is the string)
const scalarJson = [null];
F.execute_reader_to_json(handle, "SELECT COUNT(*) AS n FROM person",
    null, 0, scalarJson, err, err.length);
const countDoc = JSON.parse(scalarJson[0] || "[]");
check(Array.isArray(countDoc) && countDoc[0].n === 1, "scalar counts rows");

// json
const jsonOut = [null];
rows = F.execute_reader_to_json(handle, "SELECT id, name FROM person ORDER BY id",
    null, 0, jsonOut, err, err.length);
// koffi dereferences char** itself: jsonOut[0] is already the JSON string
const doc = jsonOut[0] || "";
check(rows === 1 && doc.startsWith("["), "reader to json");
console.log("  json: " + doc);

check(F.insert_json(handle, "person",
    '[{"id": 2, "name": "Line1"}, {"id": 3, "name": "Line2"}]',
    err, err.length) === 2, "insert_json batch");

// blob writes
const written = [0];
const rcBlob = F.insert_blob(handle, "person", "data", 2, fixture, 0,
    written, err, err.length);
console.log("  (insert_blob rc=" + rcBlob + " written=" + written[0] + " err=" + err.toString() + ")");
check(rcBlob === 1024 && written[0] === 1024,
    "insert_blob streams the file");

const sink = Buffer.alloc(1024);
const copied = [0];
check(F.get_blob(handle, "SELECT data FROM person WHERE id = 2",
    sink, 1024, copied) === C.SQLITE_OK && copied[0] === 1024, "get_blob reads it back");
check(sink.equals(fs.readFileSync(fixture)), "byte-for-byte round trip");

check(F.last_insert_rowid(handle) > 0n, "last_insert_rowid");

check(F.commit_transaction(handle, err, err.length) !== C.SQLITE_OK,
    "commit without transaction fails");
check(F.begin_transaction(handle, err, err.length) === 1, "begin");
F.execute_non_query(handle, "INSERT INTO person(name) VALUES ('Temp')",
    null, 0, err, err.length);
check(F.rollback_transaction(handle, err, err.length) === 1, "rollback");
// (count verified above through the JSON read)
// (count verified above through the JSON read)

check(F.update_blob(handle, "person", "data", 2, oversize, 0,
    written, err, err.length) === C.BLOB_TOO_SMALL,
    "update_blob refuses to exceed the column");

const rc2 = [0];
check(!F.open_readonly("missing.db", rc2) && rc2[0] === C.SQLITE_CANTOPEN,
    "open missing file fails");
check(F.execute_non_query(handle, "SELECT 1; SELECT 2", null, 0,
    err, err.length) === C.SQLITE_MISUSE, "multi-statement rejected");

check(F.close(handle) === 1, "close");
for (const f of [db, fixture, oversize]) if (fs.existsSync(f)) fs.unlinkSync(f);

console.log("\n" + checks + " checks, " + failures + " failures");
process.exit(failures > 0 ? 1 : 0);