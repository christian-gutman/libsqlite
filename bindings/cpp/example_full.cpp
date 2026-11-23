// Canonical libsqlite3 flow in C++ (see bindings/README.md).
//
// Build & run (Linux):
//   g++ -std=c++17 -I bindings/cpp example_full.cpp
//       -L <libdir> -l:libsqlite3.so -Wl,-rpath,<libdir>
//   ./a.out <libdir>/libsqlite3.so
// Windows:
//   cl /EHsc /I bindings\cpp example_full.cpp libsqlite3.lib
//   example_full.exe
// Exit code = number of failed checks.

#include "libsqlite.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

using namespace libsqlite;

static int g_total = 0;
static int g_failures = 0;

static void check(bool cond, const std::string &msg)
{
    g_total++;
    std::printf("  %s - %s\n", cond ? "ok  " : "FAIL", msg.c_str());
    if (!cond) {
        g_failures++;
    }
}

/* callback that counts the visited rows (hasrows == 0 marks an empty set) */
static int count_rows(sqlite3_stmt *, int columns, int hasrows, void *ctx)
{
    auto *state = static_cast<int *>(ctx);
    if (hasrows == 1) {
        state[0]++;
        state[1] = columns;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *lib_path = argc > 1 ? argv[1] : nullptr;

    std::printf("libsqlite3 C++ binding\n");
    std::printf("  library %s / engine %s\n", sqlite_core_version(),
                sqlite_core_sqlite_version());
    /* C++ resolves the library at link/load time: the argument is the
     * artifact the run is documented against (see README). */
    if (lib_path != nullptr) {
        std::printf("  artifact: %s\n", lib_path);
    }

    const std::string db_file = "cpp_demo.db";
    const std::string fixture = "cpp_fixture.bin";
    const std::string oversize = "cpp_oversize.bin";
    const std::string missing = "missing.db";
    const std::size_t size = 1024;

    for (const std::string *f : {&db_file, &fixture, &oversize, &missing}) {
        std::remove(f->c_str());
    }

    std::vector<unsigned char> bytes(size);
    for (std::size_t i = 0; i < size; i++) {
        bytes[i] = static_cast<unsigned char>(i & 0xFF);   // includes 0x00
    }
    {
        std::ofstream out(fixture, std::ios::binary);
        out.write(reinterpret_cast<const char *>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }

    // 1. open_or_create
    int rc = 0;
    Connection db = Connection::open_or_create(db_file, &rc);
    check(rc == OK && db.open(), "open_or_create");
    if (!db.open()) {
        std::printf("\n%d checks, %d failures\n", g_total, g_failures);
        return g_failures;
    }

    // 2. DDL
    check(db.command(
              "CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)") == OK,
          "create table");

    // 3. named parameters (union of the C struct)
    ParamSet params;
    params.integer("id", 42).text("name", "Ada");
    check(db.non_query("INSERT INTO person(id, name) VALUES (:id, :name)",
                       &params) == 1,
          "named parameter insert");

    // 4. reader callback
    int seen[2] = {0, 0};
    check(db.read_rows("SELECT id, name FROM person ORDER BY id", nullptr,
                       count_rows, seen) == 1 &&
              seen[0] == 1 && seen[1] == 2,
          "reader visits the row");

    // 5. scalar
    sqlite3_int64 count = db.scalar_int("SELECT COUNT(*) FROM person");
    check(count == 1, "scalar counts rows");

    // 5b. scalar as text (the C out struct doubles as the output, so a text
    // result needs a destination buffer)
    std::string name;
    std::string scalar_err;
    int type = 0;
    bool ok = db.scalar_text("SELECT name FROM person WHERE id = 42", nullptr,
                             name, &type, &scalar_err);
    check(ok && type == SQLITE_CORE_TYPE_TEXT && name == "Ada",
          "scalar reads text");

    // 6. reader to json
    int rows = 0;
    std::string err;
    std::string json = db.reader_json("SELECT id, name FROM person ORDER BY id",
                                      nullptr, &rows, &err);
    check(rows == 1 && json.rfind("[", 0) == 0, "reader to json");
    std::printf("  json: %s\n", json.c_str());

    // 7. json in
    check(db.insert_json("person",
                         "[{\"id\": 2, \"name\": \"Line1\"}, {\"id\": 3, \"name\": \"Line2\"}]") == 2,
          "insert_json batch");

    // 8. blob round trip (chunked streaming write + read back)
    int written = -1;
    int rc_blob = db.insert_blob("person", "data", 2, fixture, 0, &written, &err);
    std::printf("  (insert_blob rc=%d written=%d err=%s)\n", rc_blob, written,
                err.c_str());
    check(rc_blob == static_cast<int>(size) && written == static_cast<int>(size),
          "insert_blob streams the file");

    int rc_get = 0;
    int copied = 0;
    std::vector<unsigned char> back = db.get_blob(
        "SELECT data FROM person WHERE id = 2", &rc_get, &copied);
    check(rc_get == OK && copied == static_cast<int>(size) && back == bytes,
          "get_blob reads it back");
    check(back == bytes, "byte-for-byte round trip");

    check(db.last_insert_rowid() > 0, "last_insert_rowid");

    // 9. transactions
    check(db.commit() == false, "commit without transaction fails");
    check(db.begin(), "begin");
    db.non_query("INSERT INTO person(name) VALUES ('Temp')");
    check(db.rollback(), "rollback");
    check(db.scalar_int("SELECT COUNT(*) FROM person") == 3,
          "rollback discarded the row");

    // 10. oversize update is reported (column smaller than the file)
    {
        std::ofstream out(oversize, std::ios::binary);
        std::vector<unsigned char> big(size * 2, 0);
        out.write(reinterpret_cast<const char *>(big.data()),
                  static_cast<std::streamsize>(big.size()));
    }
    int written2 = -1;
    int rc_over = db.update_blob("person", "data", 2, oversize, 0, &written2, &err);
    check(rc_over == BLOB_TOO_SMALL, "update_blob refuses to exceed the column");
    std::remove(oversize.c_str());

    // 11. error paths
    int rc_missing = 0;
    Connection missing_db = Connection::open_readonly(missing, &rc_missing);
    check(!missing_db.open() && rc_missing == CANTOPEN,
          "open missing file fails");
    check(db.non_query("SELECT 1; SELECT 2") == MISUSE,
          "multi-statement rejected");

    // 12. connection info + string helpers of the frozen surface.
    // Flag-style functions return 1 on success (see docs/behavior.md), and
    // error_message mirrors the engine's last message, so it is read after a
    // deliberate failure.
    std::string fail_err;
    check(db.command("CREATE TABLE person(id INTEGER PRIMARY KEY)", &fail_err) ==
              SQLITE_ERROR,
          "duplicate create fails");
    check(!db.error_message().empty(), "error_message reports the last failure");
    check(!db.is_readonly(), "is_readonly false on a read-write handle");
    check(db.set_busy_timeout(1000) == 1, "set_busy_timeout");
    check(std::string(libsqlite::static_string(sqlite_core_param_clean_prefix(":name"))) == "name",
          "param_clean_prefix strips the prefix");
    check(take_string(sqlite_core_string_concat("lib", "sqlite")) == "libsqlite",
          "string_concat is released with string_free");
    check(take_string(sqlite_core_string_format("%s=%d", "n", 42)) == "n=42",
          "string_format");

    // 13. close
    check(db.close() == 1, "close");
    check(!db.open(), "handle is released after close");

    for (const std::string *f : {&db_file, &fixture}) {
        std::remove(f->c_str());
    }
    std::printf("\n%d checks, %d failures\n", g_total, g_failures);
    return g_failures;
}