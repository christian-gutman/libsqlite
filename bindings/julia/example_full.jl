# Canonical libsqlite3 flow in Julia (see bindings/README.md).
#
# Usage: julia --project=. example_full.jl [path-to-libsqlite3]
# No packages are required: include("libsqlite.jl") and ccall.
# Exit code = number of failed checks.

include("libsqlite.jl")
using .Libsqlite

const DB = "julia_demo.db"
const FIXTURE = "julia_fixture.bin"
const OVERSIZE = "julia_oversize.bin"

mutable struct Counter
    total::Int
    failures::Int
end

const STATE = Counter(0, 0)

function check(cond::Bool, msg::AbstractString)
    STATE.total += 1
    println(cond ? "  ok   - " : "  FAIL - ", msg)
    cond || (STATE.failures += 1)
    nothing
end

function main()
    lib = isempty(ARGS) ? nothing : ARGS[1]
    Libsqlite.load!(lib)

    (libver, engine) = Libsqlite.version_pair()
    println("libsqlite3 Julia binding")
    println("  library ", libver, " / engine ", engine)

    for path in (DB, FIXTURE, OVERSIZE)
        isfile(path) && rm(path)
    end
    fixture = UInt8.([mod(i, 0x100) for i in 1:1024])   # includes 0x00 bytes
    open(FIXTURE, "w") do io
        write(io, fixture)
    end

    # 1. open_or_create
    db, rc = Libsqlite.open_or_create(DB)
    check(rc == Libsqlite.SQLITE_OK, "open_or_create")
    db == C_NULL && return STATE.failures

    # 2. DDL
    r, _ = Libsqlite.command(db, "CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)")
    check(r == Libsqlite.SQLITE_OK, "create table")

    # 3. named parameters (the union of the C struct)
    params = Libsqlite.ParamSet()
    Libsqlite.integer(params, "id", 42)
    Libsqlite.text(params, "name", "Ada")
    r, _ = Libsqlite.non_query(db, "INSERT INTO person(id, name) VALUES (:id, :name)", params)
    check(r == 1, "named parameter insert")

    # 4. reader: a Julia function becomes the C callback (@cfunction) and
    # counts the visited rows
    r, rows, columns = Libsqlite.reader_count(db, "SELECT id, name FROM person ORDER BY id")
    check(r == 1 && rows == 1 && columns == 2, "reader visits the row")

    # 5. scalar
    count = Libsqlite.scalar_int(db, "SELECT COUNT(*) FROM person")
    check(count == 1, "scalar counts rows")

    # 5b. scalar as text (the C out struct doubles as the output, so a text
    # result needs a destination buffer)
    rc_text, name, type = Libsqlite.scalar_text(db, "SELECT name FROM person WHERE id = 42")
    check(rc_text == Libsqlite.SQLITE_OK && type == Libsqlite.TYPE_TEXT && name == "Ada",
          "scalar reads text")

    # 6. reader to json
    rows_json, doc = Libsqlite.reader_json(db, "SELECT id, name FROM person ORDER BY id")
    check(rows_json == 1 && doc !== nothing && startswith(doc, "["), "reader to json")
    println("  json: ", doc)

    # 6b. same rows through sqlite_core_json_writer_callback
    rows_cb, body = Libsqlite.json_writer_callback(db, "SELECT id, name FROM person ORDER BY id")
    inner = replace(replace(doc, r"^\[\n" => ""), r"\n\]$" => "")
    check(rows_cb == 1 && body == inner, "json_writer_callback builds the rows")

    # 7. json in
    rows_in, _ = Libsqlite.insert_json(db, "person",
        "[{\"id\": 2, \"name\": \"Line1\"}, {\"id\": 3, \"name\": \"Line2\"}]")
    check(rows_in == 2, "insert_json batch")

    # 8. blob round trip (chunked streaming write + read back)
    rc_blob, written, _ = Libsqlite.insert_blob(db, "person", "data", 2, FIXTURE)
    println("  (insert_blob rc=", rc_blob, " written=", written, ")")
    check(rc_blob == length(fixture) && written == length(fixture),
          "insert_blob streams the file")

    rc_get, data, copied = Libsqlite.get_blob(db, "SELECT data FROM person WHERE id = 2")
    check(rc_get == Libsqlite.SQLITE_OK && copied == length(fixture) && data == fixture,
          "get_blob reads it back")
    check(data == fixture, "byte-for-byte round trip")

    check(Libsqlite.last_insert_rowid(db) > 0, "last_insert_rowid")

    # 9. transactions
    check(Libsqlite.commit_transaction(db) != Libsqlite.SQLITE_OK,
          "commit without transaction fails")
    check(Libsqlite.begin_transaction(db) == 1, "begin")
    Libsqlite.non_query(db, "INSERT INTO person(name) VALUES ('Temp')")
    check(Libsqlite.rollback_transaction(db) == 1, "rollback")
    check(Libsqlite.scalar_int(db, "SELECT COUNT(*) FROM person") == 3,
          "rollback discarded the row")

    # 10. oversize update is reported (column smaller than the file)
    open(OVERSIZE, "w") do io
        write(io, zeros(UInt8, length(fixture) * 2))
    end
    rc_over, _, _ = Libsqlite.update_blob(db, "person", "data", 2, OVERSIZE)
    check(rc_over == Libsqlite.BLOB_TOO_SMALL, "update_blob refuses to exceed the column")
    isfile(OVERSIZE) && rm(OVERSIZE)

    # 11. error paths
    _, rc_missing = Libsqlite.open_readonly("missing.db")
    check(rc_missing == Libsqlite.SQLITE_CANTOPEN, "open missing file fails")
    r, _ = Libsqlite.non_query(db, "SELECT 1; SELECT 2")
    check(r == Libsqlite.SQLITE_MISUSE, "multi-statement rejected")

    # 12. the rest of the frozen surface
    r, err = Libsqlite.command(db, "CREATE TABLE person(id INTEGER PRIMARY KEY)")
    check(r == Libsqlite.SQLITE_ERROR, "duplicate create fails")
    check(!isempty(err), "error_message reports the last failure")
    check(Libsqlite.is_readonly(db) == 0, "is_readonly false on a read-write handle")
    check(Libsqlite.set_busy_timeout(db, 1000) == 1, "set_busy_timeout")
    check(Libsqlite.param_clean_prefix(":name") == "name",
          "param_clean_prefix strips the prefix")
    check(Libsqlite.error_code(db) >= 0, "error_code reports a code")
    # sqlite_core_open_ex with SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
    db2, rc_ex = Libsqlite.open_ex(DB * "2", 0x00000002 | 0x00000004)
    check(rc_ex == Libsqlite.SQLITE_OK && db2 != C_NULL,
          "open_ex creates with read-write|create flags")
    Libsqlite.close(db2)
    isfile(DB * "2") && rm(DB * "2")
    check(Libsqlite.ansi_to_utf8("Ada") == "Ada", "ansi_to_utf8 round trips ascii")
    check(Libsqlite.utf8_to_ansi("Ada") == "Ada", "utf8_to_ansi round trips ascii")
    check(Libsqlite.string_concat("lib", "sqlite") == "libsqlite",
          "string_concat is released with string_free")
    check(Libsqlite.string_format("%s says hi", "julia") == "julia says hi", "string_format")

    # 13. close
    check(Libsqlite.close(db) == 1, "close")

    for path in (DB, FIXTURE)
        isfile(path) && rm(path)
    end
    println("\n", STATE.total, " checks, ", STATE.failures, " failures")
    STATE.failures
end

exit(main())