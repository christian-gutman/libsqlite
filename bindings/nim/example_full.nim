## Canonical libsqlite3 flow in Nim (see bindings/README.md).
##
## Usage: ./run.sh [path-to-libsqlite3]
##   LD_LIBRARY_PATH=<libdir> ./example_full
## Exit code = number of failed checks.

import libsqlite, os, strutils

const DB = "nim_demo.db"
const FIXTURE = "nim_fixture.bin"
const OVERSIZE = "nim_oversize.bin"

var total = 0
var failures = 0

## Reader state + callback: a cdecl proc cannot capture the environment, so
## both live at the top level of the module.
var seen = [0, 0]

proc countRows(stmt: ptr sqlite3_stmt, columns, hasrows: cint, ctx: pointer): cint {.cdecl.} =
  if hasrows == 1:
    seen[0] += 1
    seen[1] = columns
  0

proc check(cond: bool, msg: string) =
  total += 1
  echo(if cond: "  ok   - " else: "  FAIL - ", msg)
  if not cond:
    failures += 1

proc cstr(text: string): cstring = cstring(text)



when isMainModule:
  echo "libsqlite3 Nim binding"
  echo "  library ", libraryVersion(), " / engine ", sqliteVersion()

  for path in [DB, FIXTURE, OVERSIZE]:
    if fileExists(path):
      removeFile(path)

  var fixture = newSeq[byte](1024)
  for i in 0 ..< fixture.len:
    fixture[i] = byte(i and 0xFF)      # includes 0x00 bytes
  writeFile(FIXTURE, fixture)

  # 1. open_or_create
  var rc: cint
  let db = sqlite_core_open_or_create(cstr(DB), addr rc)
  check(db != nil and rc == SQLITE_OK, "open_or_create")
  if db == nil:
    quit(failures)

  # 2. DDL
  var err = newString(512)
  check(sqlite_core_execute_command(db, cstr(
      "CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)"),
      addr err[0], 512) == SQLITE_OK, "create table")

  # 3. named parameters (the union of the C struct)
  var params: ParamSet
  params.addInt("id", 42)
  params.addText("name", "Ada")
  var paramCount: cint
  let paramPtr = params.pointer(paramCount)
  check(sqlite_core_execute_non_query(db, cstr(
      "INSERT INTO person(id, name) VALUES (:id, :name)"),
      paramPtr, paramCount, addr err[0], 512) == 1, "named parameter insert")

  # 4. reader: a Nim cdecl proc becomes the C callback and counts the rows
  check(sqlite_core_execute_reader(db, cstr("SELECT id, name FROM person ORDER BY id"),
      nil, 0, countRows, nil, addr err[0], 512) == 1 and seen[0] == 1 and seen[1] == 2,
      "reader visits the row")

  # 5. scalar
  var outScalar = Param(`type`: TYPE_INT)
  check(sqlite_core_execute_scalar(db, cstr("SELECT COUNT(*) FROM person"), nil, 0,
      addr outScalar, nil, 0) == SQLITE_OK, "scalar counts rows")
  check(outScalar.value.iVal == 1, "scalar value is 1")

  # 5b. scalar as text (the C out struct doubles as the output, so a text
  # result needs a destination buffer)
  var textBuf = newString(512)
  var outTextScalar = Param(`type`: TYPE_TEXT)
  outTextScalar.value.blob_or_text.`ptr` = cast[cstring](addr textBuf[0])
  outTextScalar.value.blob_or_text.max_len = 512
  check(sqlite_core_execute_scalar(db, cstr("SELECT name FROM person WHERE id = 42"), nil, 0,
      addr outTextScalar, nil, 0) == SQLITE_OK, "scalar reads text (call)")
  # the library writes the text and reports its length in blob_or_text.len
  let textLen = outTextScalar.value.blob_or_text.len
  check(textBuf[0 ..< int(textLen)] == "Ada", "scalar reads text (value)")

  # 6. reader to json
  var jsonPtr: cstring
  var doc: string
  let rows = sqlite_core_execute_reader_to_json(db, cstr(
      "SELECT id, name FROM person ORDER BY id"), nil, 0, addr jsonPtr, nil, 0)
  if jsonPtr != nil:
    doc = $jsonPtr
    sqlite_core_string_free(jsonPtr)
  check(rows == 1 and doc.startsWith("["), "reader to json")
  echo "  json: ", doc

  # 6b. same rows through sqlite_core_json_writer_callback: the builder
  # context mirrors sqlite_core_json_builder_t and the callback fills it with
  # the row objects, while execute_reader_to_json adds the array delimiters
  let builder = cast[ptr UncheckedArray[byte]](alloc0(32))
  let rowsCb = sqlite_core_execute_reader(db, cstr(
      "SELECT id, name FROM person ORDER BY id"), nil, 0,
      sqlite_core_json_writer_callback, builder, nil, 0)
  var body = ""
  let bodyPtr = cast[ptr cstring](builder)
  if bodyPtr[] != nil:
    body = $bodyPtr[]
    sqlite_core_string_free(bodyPtr[])
  # execute_reader_to_json appends "\n]" after the last row, so the inner part
  # is the document minus "[\n" and the "\n]" suffix
  let innerText = doc[2 ..< ^2]
  check(rowsCb == 1 and body == innerText, "json_writer_callback builds the rows")

  # 7. json in
  check(sqlite_core_insert_json(db, cstr("person"), cstr(
      "[{\"id\": 2, \"name\": \"Line1\"}, {\"id\": 3, \"name\": \"Line2\"}]"),
      addr err[0], 512) == 2, "insert_json batch")

  # 8. blob round trip (chunked streaming write + read back)
  var written: cint
  let rcBlob = sqlite_core_insert_blob(db, cstr("person"), cstr("data"), 2,
      cstr(FIXTURE), 0, addr written, addr err[0], 512)
  echo "  (insert_blob rc=", rcBlob, " written=", written, ")"
  check(rcBlob == fixture.len and written == fixture.len, "insert_blob streams the file")

  var copied: cint
  var sink = newSeq[byte](fixture.len)
  let rcGet = sqlite_core_get_blob(db, cstr("SELECT data FROM person WHERE id = 2"),
      addr sink[0], cint(sink.len), addr copied)
  check(rcGet == SQLITE_OK and copied == fixture.len, "get_blob reads it back")
  check(sink == fixture, "byte-for-byte round trip")

  check(sqlite_core_last_insert_rowid(db) > 0, "last_insert_rowid")

  # 9. transactions
  check(sqlite_core_commit_transaction(db, addr err[0], 512) != SQLITE_OK,
      "commit without transaction fails")
  check(sqlite_core_begin_transaction(db, addr err[0], 512) == 1, "begin")
  discard sqlite_core_execute_non_query(db, cstr("INSERT INTO person(name) VALUES ('Temp')"),
      nil, 0, addr err[0], 512)
  check(sqlite_core_rollback_transaction(db, addr err[0], 512) == 1, "rollback")
  outScalar = Param(`type`: TYPE_INT)
  discard sqlite_core_execute_scalar(db, cstr("SELECT COUNT(*) FROM person"), nil, 0,
      addr outScalar, nil, 0)
  check(outScalar.value.iVal == 3, "rollback discarded the row")

  # 10. oversize update is reported (column smaller than the file)
  var big = newSeq[byte](fixture.len * 2)
  writeFile(OVERSIZE, big)
  written = 0
  let rcOver = sqlite_core_update_blob(db, cstr("person"), cstr("data"), 2,
      cstr(OVERSIZE), 0, addr written, addr err[0], 512)
  check(rcOver == BLOB_TOO_SMALL, "update_blob refuses to exceed the column")
  if fileExists(OVERSIZE):
    removeFile(OVERSIZE)

  # 11. error paths
  var rcMissing: cint
  let missing = sqlite_core_open_readonly(cstr("missing.db"), addr rcMissing)
  check(missing == nil and rcMissing == SQLITE_CANTOPEN, "open missing file fails")
  check(sqlite_core_execute_non_query(db, cstr("SELECT 1; SELECT 2"), nil, 0,
      addr err[0], 512) == SQLITE_MISUSE, "multi-statement rejected")

  # 12. the rest of the frozen surface
  check(sqlite_core_execute_command(db, cstr(
      "CREATE TABLE person(id INTEGER PRIMARY KEY)"), addr err[0], 512) == SQLITE_ERROR,
      "duplicate create fails")
  check(err.len > 0, "error_message reports the last failure")
  check(sqlite_core_is_readonly(db) == 0, "is_readonly false on a read-write handle")
  check(sqlite_core_set_busy_timeout(db, 1000) == 1, "set_busy_timeout")
  check($sqlite_core_param_clean_prefix(cstr(":name")) == "name",
      "param_clean_prefix strips the prefix")
  check(sqlite_core_error_code(db) >= 0, "error_code reports a code")
  check(takeString(sqlite_core_string_concat(cstr("lib"), cstr("sqlite"))) == "libsqlite",
      "string_concat is released with string_free")
  check(takeString(sqlite_core_string_format(cstr("%s says hi"), cstr("nim"))) == "nim says hi",
      "string_format")
  check(takeString(sqlite_core_string_ansi_to_utf8(cstr("Ada"))) == "Ada",
      "ansi_to_utf8 round trips ascii")
  check(takeString(sqlite_core_string_utf8_to_ansi(cstr("Ada"))) == "Ada",
      "utf8_to_ansi round trips ascii")
  # sqlite_core_open_ex with SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
  var rcEx: cint
  let db2 = sqlite_core_open_ex(cstr(DB & "2"), 0x2 or 0x4, addr rcEx)
  check(rcEx == SQLITE_OK and db2 != nil, "open_ex creates with read-write|create flags")
  discard sqlite_core_close(db2)
  if fileExists(DB & "2"):
    removeFile(DB & "2")

  # 13. close
  check(sqlite_core_close(db) == 1, "close")

  for path in [DB, FIXTURE]:
    if fileExists(path):
      removeFile(path)

  echo "\n", total, " checks, ", failures, " failures"
  quit(failures)