"""Canonical libsqlite3 flow in Python (see bindings/README.md).

Usage: python example_full.py [path-to-libsqlite3]
Exit code = number of failed checks.
"""

import ctypes
import os
import sys

from binding import (
    Lib,
    SqliteCoreParam,
    SQLITE_OK,
    SQLITE_MISUSE,
    SQLITE_CANTOPEN,
    SQLITE_CORE_TYPE_INT,
    SQLITE_CORE_BLOB_TOO_SMALL,
)

DB = "python_demo.db"
FIXTURE = "python_fixture.bin"
CHECKS = [0, 0]  # total, failures


def check(cond, msg):
    CHECKS[0] += 1
    print(("  ok   - " if cond else "  FAIL - ") + msg)
    if not cond:
        CHECKS[1] += 1


def main():
    lib_path = sys.argv[1] if len(sys.argv) > 1 else None
    lib = Lib(lib_path)
    lib.lib.util = None

    print("libsqlite3 Python binding")
    print("  library %s / engine %s" % (lib.string(lib.lib.sqlite_core_version()),
                                        lib.string(lib.lib.sqlite_core_sqlite_version())))

    for f in (DB, FIXTURE):
        if os.path.exists(f):
            os.remove(f)
    with open(FIXTURE, "wb") as fh:
        fh.write(bytes(range(256)) * 4)  # 1024 bytes, includes 0x00

    rc = ctypes.c_int(0)
    db = lib.lib.sqlite_core_open_or_create(DB.encode(), ctypes.byref(rc))
    check(db is not None and rc.value == SQLITE_OK, "open_or_create")
    if not db:
        return CHECKS[1]

    err = ctypes.create_string_buffer(256)

    # DDL
    check(lib.lib.sqlite_core_execute_command(
        db, b"CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)",
        err, len(err)) == SQLITE_OK, "create table")

    # named parameters
    params = (SqliteCoreParam * 2)(SqliteCoreParam.integer("id", 42),
                                   SqliteCoreParam.text("name", "Ada"))
    check(lib.lib.sqlite_core_execute_non_query(
        db, b"INSERT INTO person(id, name) VALUES (:id, :name)",
        params, 2, err, len(err)) == 1, "named parameter insert")

    # reader callback (counts rows; the exported surface cannot call the
    # engine column API on a DLL build, so rows are shown as JSON)
    seen = [0]

    @ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p)
    def on_row(stmt, columns, hasrows, ctx):
        seen[0] += 1 if hasrows else 0
        return 0

    check(lib.lib.sqlite_core_execute_reader(
        db, b"SELECT id, name FROM person ORDER BY id", None, 0,
        on_row, None, err, len(err)) == 1 and seen[0] == 1, "reader visits the row")

    # scalar
    out = SqliteCoreParam()
    out.type = SQLITE_CORE_TYPE_INT
    check(lib.lib.sqlite_core_execute_scalar(
        db, b"SELECT COUNT(*) FROM person", None, 0,
        ctypes.byref(out), err, len(err)) == SQLITE_OK and out.value.iVal == 1,
        "scalar counts rows")

    # json out
    json_ptr = ctypes.c_void_p()
    rows = lib.lib.sqlite_core_execute_reader_to_json(
        db, b"SELECT id, name FROM person ORDER BY id", None, 0,
        ctypes.byref(json_ptr), err, len(err))
    doc = lib.take(json_ptr) if json_ptr.value else None
    check(rows == 1 and doc is not None and doc.startswith("["), "reader to json")
    print("  json: " + (doc or ""))

    # json in
    check(lib.lib.sqlite_core_insert_json(
        db, b"person",
        b'[{"id": 2, "name": "Line1"}, {"id": 3, "name": "Line2"}]',
        err, len(err)) == 2, "insert_json batch")

    # blob writes (chunked) + read back
    written = ctypes.c_int(0)
    fixture_size = os.path.getsize(FIXTURE)
    check(lib.lib.sqlite_core_insert_blob(
        db, b"person", b"data", 2, FIXTURE.encode(), 0,
        ctypes.byref(written), err, len(err)) == fixture_size and written.value == fixture_size,
        "insert_blob streams the file")

    sink = ctypes.create_string_buffer(fixture_size)
    copied = ctypes.c_int(0)
    check(lib.lib.sqlite_core_get_blob(
        db, b"SELECT data FROM person WHERE id = 2", sink, fixture_size,
        ctypes.byref(copied)) == SQLITE_OK and copied.value == fixture_size,
        "get_blob reads it back")
    with open(FIXTURE, "rb") as fh:
        check(sink.raw[:copied.value] == fh.read(), "byte-for-byte round trip")

    check(lib.lib.sqlite_core_last_insert_rowid(db) > 0, "last_insert_rowid")

    # transaction paths
    check(lib.lib.sqlite_core_commit_transaction(db, err, len(err)) != SQLITE_OK,
          "commit without transaction fails")
    check(lib.lib.sqlite_core_begin_transaction(db, err, len(err)) == 1, "begin")
    lib.lib.sqlite_core_execute_non_query(
        db, b"INSERT INTO person(name) VALUES ('Temp')", None, 0, err, len(err))
    check(lib.lib.sqlite_core_rollback_transaction(db, err, len(err)) == 1, "rollback")
    out2 = SqliteCoreParam()
    out2.type = SQLITE_CORE_TYPE_INT
    lib.lib.sqlite_core_execute_scalar(db, b"SELECT COUNT(*) FROM person", None, 0,
                                       ctypes.byref(out2), err, len(err))
    check(out2.value.iVal == 3, "rollback discarded the row")

    # oversize update is reported (a file larger than the stored blob)
    BIG = "python_oversize.bin"
    with open(BIG, "wb") as fh:
        fh.write(bytes(fixture_size * 2))
    rc_over = lib.lib.sqlite_core_update_blob(
        db, b"person", b"data", 2, BIG.encode(), 0,
        ctypes.byref(written), err, len(err))
    check(rc_over == SQLITE_CORE_BLOB_TOO_SMALL,
          "update_blob refuses to exceed the column")
    os.remove(BIG)

    # error paths
    rc2 = ctypes.c_int(0)
    missing = lib.lib.sqlite_core_open_readonly(b"missing.db", ctypes.byref(rc2))
    check(not missing and rc2.value == SQLITE_CANTOPEN, "open missing file fails")
    check(lib.lib.sqlite_core_execute_non_query(
        db, b"SELECT 1; SELECT 2", None, 0, err, len(err)) == SQLITE_MISUSE,
        "multi-statement rejected")

    check(lib.lib.sqlite_core_close(db) == 1, "close")

    for f in (DB, FIXTURE):
        if os.path.exists(f):
            os.remove(f)
    print("\n%d checks, %d failures" % (CHECKS[0], CHECKS[1]))
    return CHECKS[1]


if __name__ == "__main__":
    sys.exit(main())