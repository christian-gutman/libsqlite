/*
 * Regression tests for the bug-fix milestone.
 * One test per previously verified bug, plus .NET parity checks from
 * docs/behavior.md. Runs with CTest; exit code = number of failures.
 */

#include "sqlite_core.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                             \
    do                                                               \
    {                                                                \
        g_checks++;                                                  \
        if (cond)                                                    \
        {                                                            \
            printf("  ok   - %s\n", msg);                            \
        }                                                            \
        else                                                         \
        {                                                            \
            g_failures++;                                            \
            printf("  FAIL - %s\n", msg);                            \
        }                                                            \
    } while (0)

static const char *DB_FILE = "regression.db";

static void fresh_db(void)
{
    remove(DB_FILE);
}

/* Prepares a scalar output param that can receive a TEXT value:
 * TEXT results are copied into the caller's buffer (ptr/max_len). */
static void out_param_init(sqlite_core_param_t *p, char *buf, size_t buf_size)
{
    memset(p, 0, sizeof(*p));
    p->value.blob_or_text.ptr = buf;
    p->value.blob_or_text.max_len = (int)buf_size;
}

static int table_exists(sqlite3 *db, const char *name)
{
    sqlite_core_param_t out;
    memset(&out, 0, sizeof(out));
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='%s'", name);
    int rc = sqlite_core_execute_scalar(db, sql, NULL, 0, &out, NULL, 0);
    return (rc == SQLITE_OK && out.type == SQLITE_CORE_TYPE_INT && out.value.iVal == 1) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
static void test_isreadonly(void)
{
    printf("IsReadonly (was inverted)\n");
    fresh_db();
    int rc = 0;
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    CHECK(db != NULL && rc == SQLITE_OK, "open_or_create succeeds");
    CHECK(sqlite_core_is_readonly(db) == 0, "writable database reports 0 (not 1)");
    sqlite_core_close(db);

    sqlite3 *ro = sqlite_core_open_readonly(DB_FILE, &rc);
    CHECK(ro != NULL && rc == SQLITE_OK, "open_readonly succeeds");
    CHECK(sqlite_core_is_readonly(ro) == 1, "read-only database reports 1 (not 0)");

    /* writes must actually fail on the read-only handle */
    int wrc = sqlite_core_execute_non_query(ro, "CREATE TABLE t(x)", NULL, 0, NULL, 0);
    CHECK(wrc != SQLITE_OK && wrc != SQLITE_MISUSE, "write on read-only handle fails");
    sqlite_core_close(ro);
}

/* ------------------------------------------------------------------ */
static void test_scalar_int64_and_rows(void)
{
    printf("ExecuteScalar / ExecuteNonQuery parity\n");
    fresh_db();
    int rc = 0;
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    CHECK(db != NULL, "db opened");

    CHECK(sqlite_core_execute_non_query(db, "CREATE TABLE big(v INTEGER)", NULL, 0, NULL, 0) >= 0,
          "CREATE TABLE executes");

    /* 64-bit scalar value (used to truncate to 32 bits) */
    CHECK(sqlite_core_execute_non_query(db, "INSERT INTO big VALUES (9007199254740993)", NULL, 0, NULL, 0) == 1,
          "INSERT reports 1 affected row");
    sqlite_core_param_t out;
    memset(&out, 0, sizeof(out));
    rc = sqlite_core_execute_scalar(db, "SELECT v FROM big", NULL, 0, &out, NULL, 0);
    CHECK(rc == SQLITE_OK && out.type == SQLITE_CORE_TYPE_INT && out.value.iVal == 9007199254740993LL,
          "ExecuteScalar returns the exact 64-bit value");

    /* empty result set -> NULL (.NET parity) */
    memset(&out, 0, sizeof(out));
    rc = sqlite_core_execute_scalar(db, "SELECT v FROM big WHERE 1=0", NULL, 0, &out, NULL, 0);
    CHECK(rc == SQLITE_OK && out.type == SQLITE_CORE_TYPE_NULL, "empty result set yields NULL");

    /* transaction text is rejected (.NET parity) */
    int brc = sqlite_core_execute_non_query(db, "BEGIN", NULL, 0, NULL, 0);
    CHECK(brc == SQLITE_MISUSE, "plain BEGIN text is rejected");

    sqlite_core_close(db);
}

/* ------------------------------------------------------------------ */
static void test_insert_json(void)
{
    printf("Insert_Json (was dropping the last char of every key/value)\n");
    fresh_db();
    int rc = 0;
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    CHECK(sqlite_core_execute_non_query(db,
              "CREATE TABLE personas(id INTEGER PRIMARY KEY, nombre TEXT, ciudad TEXT, saldo REAL)",
              NULL, 0, NULL, 0) >= 0, "CREATE TABLE personas");

    const char *json =
        "[{\"id\":1,\"nombre\":\"Zúrico Pérez\",\"ciudad\":\"Málaga\",\"saldo\":1250.5},"
        "{\"id\":2,\"nombre\":\"Ana\",\"ciudad\":\"Bogotá\",\"saldo\":-3.25}]";

    int rows = sqlite_core_insert_json(db, "personas", json, NULL, 0);
    CHECK(rows == 2, "two rows inserted");

    char nombre_buf[256];
    sqlite_core_param_t out;
    out_param_init(&out, nombre_buf, sizeof(nombre_buf));
    rc = sqlite_core_execute_scalar(db, "SELECT nombre FROM personas WHERE id=1", NULL, 0, &out, NULL, 0);
    /* TEXT out is converted to ANSI on Windows, kept as UTF-8 elsewhere */
#if defined(_WIN32)
    const char *expected_name = "Z\xFArico P\xE9rez";
#else
    const char *expected_name = "Z\xC3\xBArico P\xC3\xA9rez";
#endif
    CHECK(rc == SQLITE_OK && out.type == SQLITE_CORE_TYPE_TEXT &&
          strcmp(out.value.blob_or_text.ptr, expected_name) == 0,
          "first row value round-trips exactly (last char kept)");

    double saldo = 0;
    rc = sqlite_core_execute_scalar(db, "SELECT saldo FROM personas WHERE id=2", NULL, 0, &out, NULL, 0);
    saldo = (rc == SQLITE_OK && out.type == SQLITE_CORE_TYPE_DOUBLE) ? out.value.dVal : 0;
    CHECK(saldo == -3.25, "REAL value is faithful");

    /* duplicate PK on the second row rolls the whole batch back */
    int rows2 = sqlite_core_insert_json(db, "personas",
        "[{\"id\":10,\"nombre\":\"Nueva\"},{\"id\":1,\"nombre\":\"Dup\"}]", NULL, 0);
    CHECK(rows2 < 0, "constraint violation is reported as an error");
    rc = sqlite_core_execute_scalar(db, "SELECT COUNT(*) FROM personas WHERE id=10", NULL, 0, &out, NULL, 0);
    CHECK(rc == SQLITE_OK && out.value.iVal == 0, "failed batch rolled back completely");

    /* not-an-array input is rejected, no injection happens */
    int rows3 = sqlite_core_insert_json(db, "personas", "{\"id\":1}", NULL, 0);
    CHECK(rows3 < 0, "object instead of array is rejected");
    int rows4 = sqlite_core_insert_json(db, "personas", "[]", NULL, 0);
    CHECK(rows4 == 0, "empty array inserts nothing");

    sqlite_core_close(db);
}

/* ------------------------------------------------------------------ */
static void test_insert_json_injection(void)
{
    printf("Insert_Json injection guard (table/column names)\n");
    fresh_db();
    int rc = 0;
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    CHECK(sqlite_core_execute_non_query(db, "CREATE TABLE app_data(id INTEGER, v TEXT)", NULL, 0, NULL, 0) >= 0,
          "CREATE TABLE app_data");

    int rows = sqlite_core_insert_json(db, "app_data", "[{\"id\":1,\"v\":\"legit\"}]", NULL, 0);
    CHECK(rows == 1, "legit insert works");

    /* hostile table name must not execute the injected statement */
    int bad = sqlite_core_insert_json(db, "x\"); DROP TABLE app_data; --", "[{\"id\":2,\"v\":\"pwn\"}]", NULL, 0);
    CHECK(bad < 0, "hostile table name fails with an error");
    CHECK(table_exists(db, "app_data"), "app_data survives the hostile table name");

    /* hostile column key must not break the statement structure */
    int bad2 = sqlite_core_insert_json(db, "app_data", "[{\"id\":3,\"v\":\"a\"}); DROP TABLE app_data; --\"}]", NULL, 0);
    CHECK(bad2 < 0 || table_exists(db, "app_data"), "hostile column key does not drop the table");
    CHECK(table_exists(db, "app_data"), "app_data survives the hostile column key");

    sqlite_core_param_t out;
    memset(&out, 0, sizeof(out));
    rc = sqlite_core_execute_scalar(db, "SELECT COUNT(*) FROM app_data", NULL, 0, &out, NULL, 0);
    CHECK(rc == SQLITE_OK && out.value.iVal == 1, "only the legit row was inserted");

    sqlite_core_close(db);
}

/* ------------------------------------------------------------------ */
static void test_insert_json_transaction_guard(void)
{
    printf("Insert_Json transaction guard (used to steal the caller's transaction)\n");
    fresh_db();
    int rc = 0;
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    CHECK(sqlite_core_execute_non_query(db, "CREATE TABLE tx_t(v TEXT)", NULL, 0, NULL, 0) >= 0,
          "CREATE TABLE tx_t");

    /* Start an ambient transaction, then call Insert_Json: it must join it */
    CHECK(sqlite_core_begin_transaction(db, NULL, 0) == 1, "ambient transaction started");
    int rows = sqlite_core_insert_json(db, "tx_t", "[{\"v\":\"a\"},{\"v\":\"b\"}]", NULL, 0);
    CHECK(rows == 2, "Insert_Json joins the ambient transaction");

    /* Rollback must still undo everything (Insert_Json did not commit) */
    CHECK(sqlite_core_rollback_transaction(db, NULL, 0) == 1, "rollback succeeds");
    sqlite_core_param_t out;
    memset(&out, 0, sizeof(out));
    rc = sqlite_core_execute_scalar(db, "SELECT COUNT(*) FROM tx_t", NULL, 0, &out, NULL, 0);
    CHECK(rc == SQLITE_OK && out.value.iVal == 0, "ambient rollback undid the batch (no stolen commit)");

    /* Without an ambient transaction Insert_Json commits by itself */
    rows = sqlite_core_insert_json(db, "tx_t", "[{\"v\":\"c\"}]", NULL, 0);
    CHECK(rows == 1, "standalone Insert_Json works");
    rc = sqlite_core_execute_scalar(db, "SELECT COUNT(*) FROM tx_t", NULL, 0, &out, NULL, 0);
    CHECK(rc == SQLITE_OK && out.value.iVal == 1, "standalone Insert_Json committed");

    /* Nested BeginTransaction is rejected */
    CHECK(sqlite_core_begin_transaction(db, NULL, 0) == 1, "begin");
    CHECK(sqlite_core_begin_transaction(db, NULL, 0) != 1, "nested begin rejected");
    CHECK(sqlite_core_rollback_transaction(db, NULL, 0) == 1, "rollback");
    CHECK(sqlite_core_rollback_transaction(db, NULL, 0) != 1, "stray rollback rejected");

    sqlite_core_close(db);
}

/* ------------------------------------------------------------------ */
static void test_json_roundtrip(void)
{
    printf("ExecuteReaderToJSON + escaping (was emitting invalid JSON)\n");
    fresh_db();
    int rc = 0;
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    CHECK(sqlite_core_execute_non_query(db,
              "CREATE TABLE rt(a INTEGER, b TEXT, c REAL, d TEXT)",
              NULL, 0, NULL, 0) >= 0, "CREATE TABLE rt");

    /* tricky values: quotes, backslashes, newline+tab (via char()), unicode, NULL */
    CHECK(sqlite_core_execute_non_query(db, "INSERT INTO rt VALUES (1, 'quote\" and \\ back', 3.5, 'línea1' || char(10) || 'línea2' || char(9) || 'tab')",
              NULL, 0, NULL, 0) >= 0, "INSERT tricky row");
    CHECK(sqlite_core_execute_non_query(db, "INSERT INTO rt VALUES (9007199254740993, 'x', 0, NULL)",
              NULL, 0, NULL, 0) >= 0, "INSERT row with NULL");

    char *json = NULL;
    int rows = sqlite_core_execute_reader_to_json(db, "SELECT a, b, c, d FROM rt ORDER BY a", NULL, 0, &json, NULL, 0);
    CHECK(rows == 2 && json != NULL, "reader to json returns 2 rows");
    if (json)
    {
        CHECK(strstr(json, "quote\\\" and \\\\ back") != NULL, "quotes and backslashes are escaped");
        CHECK(strstr(json, "línea1\nlínea2") == NULL, "raw control character is not emitted");
        CHECK(strstr(json, "nea1\\nl") != NULL, "newline is escaped as \\n");
        CHECK(strstr(json, "nea2\\tt") != NULL, "tab is escaped as \\t");
        CHECK(strstr(json, "9007199254740993") != NULL, "64-bit integer survives in JSON");
        CHECK(strstr(json, ": null") != NULL, "NULL emitted as JSON null");
        CHECK(json[0] == '[' && json[strlen(json) - 1] == ']', "document is wrapped in an array");

        /* round-trip: feed the JSON straight back into another table */
        CHECK(sqlite_core_execute_non_query(db, "CREATE TABLE rt2(a INTEGER, b TEXT, c REAL, d TEXT)",
                  NULL, 0, NULL, 0) >= 0, "CREATE TABLE rt2");
        int back = sqlite_core_insert_json(db, "rt2", json, NULL, 0);
        CHECK(back == 2, "generated JSON is valid input for Insert_Json");

        char b_buf[512];
        sqlite_core_param_t out;
        out_param_init(&out, b_buf, sizeof(b_buf));
        rc = sqlite_core_execute_scalar(db, "SELECT b FROM rt2 WHERE a=1", NULL, 0, &out, NULL, 0);
        CHECK(rc == SQLITE_OK && out.type == SQLITE_CORE_TYPE_TEXT &&
              out.value.blob_or_text.ptr != NULL &&
              strcmp(out.value.blob_or_text.ptr, "quote\" and \\ back") == 0,
              "escaped value round-trips byte-exact");
        char d_buf[512];
        out_param_init(&out, d_buf, sizeof(d_buf));
        rc = sqlite_core_execute_scalar(db, "SELECT d FROM rt2 WHERE a=1", NULL, 0, &out, NULL, 0);
#if defined(_WIN32)
        const char *expected_d = "l\xEDnea1\nl\xEDnea2\ttab";
#else
        const char *expected_d = "l\xC3\xADnea1\nl\xC3\xADnea2\ttab";
#endif
        CHECK(rc == SQLITE_OK && out.value.blob_or_text.ptr != NULL &&
              strcmp(out.value.blob_or_text.ptr, expected_d) == 0,
              "newline/tab value round-trips byte-exact");
        rc = sqlite_core_execute_scalar(db, "SELECT d FROM rt2 WHERE a=9007199254740993", NULL, 0, &out, NULL, 0);
        CHECK(rc == SQLITE_OK && out.type == SQLITE_CORE_TYPE_NULL, "null round-trips as NULL");

        sqlite_core_string_free(json);
    }

    /* empty result set -> "[]" */
    char *empty = NULL;
    int empty_rows = sqlite_core_execute_reader_to_json(db, "SELECT a FROM rt WHERE 1=0", NULL, 0, &empty, NULL, 0);
    CHECK(empty_rows == 0 && empty != NULL && strcmp(empty, "[]") == 0, "empty result set yields []");
    if (empty)
        sqlite_core_string_free(empty);

    sqlite_core_close(db);
}

/* ------------------------------------------------------------------ */
static void test_getblob(void)
{
    printf("GetBlob (was not compiled at all)\n");
    fresh_db();
    int rc = 0;
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    CHECK(sqlite_core_execute_non_query(db, "CREATE TABLE files(name TEXT, data BLOB)", NULL, 0, NULL, 0) >= 0,
          "CREATE TABLE files");

    unsigned char blob[8] = {0x00, 0x01, 0xFE, 0xFF, 0x10, 0x7F, 0x80, 0x42};
    char hex[64];
    snprintf(hex, sizeof(hex), "x'%02X%02X%02X%02X%02X%02X%02X%02X'",
             blob[0], blob[1], blob[2], blob[3], blob[4], blob[5], blob[6], blob[7]);
    char sql[256];
    snprintf(sql, sizeof(sql), "INSERT INTO files VALUES ('a', %s)", hex);
    CHECK(sqlite_core_execute_non_query(db, sql, NULL, 0, NULL, 0) >= 0, "INSERT blob");

    unsigned char dest[8] = {0};
    int copied = 0;
    rc = sqlite_core_get_blob(db, "SELECT data FROM files WHERE name='a'", dest, sizeof(dest), &copied);
    CHECK(rc == SQLITE_OK && copied == 8 && memcmp(dest, blob, 8) == 0, "blob round-trips exactly");

    unsigned char small[4] = {0};
    copied = 0;
    rc = sqlite_core_get_blob(db, "SELECT data FROM files WHERE name='a'", small, sizeof(small), &copied);
    CHECK(rc == SQLITE_CORE_BUFFER_TOO_SMALL && copied == 4, "small buffer reports BUFFER_TOO_SMALL and copies what fits");

    sqlite_core_close(db);
}

/* ------------------------------------------------------------------ */
static void test_long_string_no_stack_overflow(void)
{
    printf("Long string conversion (was _alloca -> stack overflow risk)\n");
    const int cap = 400000;
    char *src = (char *)malloc((size_t)cap + 1);
    CHECK(src != NULL, "test buffer allocated");
    if (src)
    {
        /* Build a long, valid UTF-8 string: "A" + U+00E9 (0xC3 0xA9) repeatedly.
         * The old _alloca implementation would need ~250 KB of stack here. */
        int pos = 0;
        while (pos + 3 <= cap - 1)
        {
            src[pos++] = 'A';
            src[pos++] = (char)0xC3;
            src[pos++] = (char)0xA9;
        }
        src[pos] = '\0';
        size_t src_len = (size_t)pos;

        char *ansi = sqlite_core_string_utf8_to_ansi(src);
        CHECK(ansi != NULL, "long UTF-8 -> ANSI conversion succeeds on the heap");
        if (ansi)
        {
            /* every U+00E9 becomes one 0xE9 byte plus alternating 'A' */
            CHECK(strlen(ansi) == src_len - src_len / 3, "converted length is correct");
            char *utf8 = sqlite_core_string_ansi_to_utf8(ansi);
            CHECK(utf8 != NULL, "long ANSI -> UTF-8 conversion succeeds");
            if (utf8)
                CHECK(strcmp(utf8, src) == 0, "long string round-trips exactly");
            sqlite_core_string_free(utf8);
            sqlite_core_string_free(ansi);
        }
        free(src);
    }
}

/* ------------------------------------------------------------------ */
static void test_ansi_sql_and_uri(void)
{
    printf("ANSI SQL + URI readonly open\n");
    fresh_db();
    int rc = 0;
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    CHECK(sqlite_core_execute_non_query(db, "CREATE TABLE t(v TEXT)", NULL, 0, NULL, 0) >= 0, "create");

    /* accented SQL text through ExecuteNonQuery (conversion was missing here) */
    int rows = sqlite_core_execute_non_query(db, "INSERT INTO t VALUES ('Café')", NULL, 0, NULL, 0);
    CHECK(rows == 1, "ANSI SQL executes on ExecuteNonQuery");

    char v_buf[256];
    sqlite_core_param_t out;
    out_param_init(&out, v_buf, sizeof(v_buf));
    rc = sqlite_core_execute_scalar(db, "SELECT v FROM t", NULL, 0, &out, NULL, 0);
#if defined(_WIN32)
    CHECK(rc == SQLITE_OK && out.type == SQLITE_CORE_TYPE_TEXT &&
          out.value.blob_or_text.len == 4 &&
          memcmp(out.value.blob_or_text.ptr, "Caf\xE9", 4) == 0, "scalar text is returned in ANSI");
#else
    CHECK(rc == SQLITE_OK && out.type == SQLITE_CORE_TYPE_TEXT &&
          strcmp(out.value.blob_or_text.ptr, "Caf\xC3\xA9") == 0, "scalar text is returned in UTF-8");
#endif
    sqlite_core_close(db);

    /* URI read-only: no global sqlite3_config side effect anymore */
    sqlite3 *u = sqlite_core_open_uri_readonly("file:regression.db?mode=ro", &rc);
    CHECK(u != NULL && rc == SQLITE_OK, "URI read-only open succeeds");
    if (u)
    {
        CHECK(sqlite_core_is_readonly(u) == 1, "URI connection is read-only");
        int w = sqlite_core_execute_non_query(u, "INSERT INTO t VALUES ('x')", NULL, 0, NULL, 0);
        CHECK(w != SQLITE_OK && w != SQLITE_MISUSE, "write fails on URI read-only");
        sqlite_core_close(u);
    }

    /* and the same process keeps working with regular connections after a
     * URI open (the removed global config used to leak process-wide state) */
    sqlite3 *again = sqlite_core_open(DB_FILE, &rc);
    CHECK(again != NULL && rc == SQLITE_OK, "regular open still works after URI usage");
    if (again)
        sqlite_core_close(again);
}

/* ------------------------------------------------------------------ */
static void test_version(void)
{
    printf("Version strings\n");
    CHECK(strcmp(sqlite_core_version(), "1.0.0") == 0, "library version is 1.0.0");
    CHECK(strstr(sqlite_core_sqlite_version(), "3.54") != NULL, "engine version exposed separately");
}

/* ------------------------------------------------------------------ */
static void test_encoding_detect(void)
{
    printf("Absorbed encoding detection\n");
    CHECK(sqlite_core_detect_encoding("plain ascii") == ENCODING_ASCII, "ascii detected");
    CHECK(sqlite_core_detect_encoding("caf\xC3\xA9") == ENCODING_UTF8, "utf-8 detected");
    CHECK(sqlite_core_detect_encoding("caf\xE9") == ENCODING_ANSI, "ansi detected");
    CHECK(sqlite_core_detect_encoding("") == ENCODING_UNKNOWN, "empty string reports unknown");
}

/* ------------------------------------------------------------------ */
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0); /* unbuffered: see progress even on crashes */
    printf("libsqlite3 regression tests\n");
    printf("library %s / sqlite %s\n\n", sqlite_core_version(), sqlite_core_sqlite_version());

    test_isreadonly();
    test_scalar_int64_and_rows();
    test_insert_json();
    test_insert_json_injection();
    test_insert_json_transaction_guard();
    test_json_roundtrip();
    test_getblob();
    test_long_string_no_stack_overflow();
    test_ansi_sql_and_uri();
    test_version();
    test_encoding_detect();

    remove(DB_FILE);

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures;
}