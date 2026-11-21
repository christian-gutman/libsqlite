/*
 * API and .NET parity tests (docs/behavior.md).
 * One CTest target; exit code = number of failures.
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

static const char *DB_FILE = "api_test.db";

/* ------------------------------------------------------------------ */
static void test_param_helpers(void)
{
    printf("Parameter helpers\n");
    CHECK(sqlite_core_param_clean_prefix(":id") != NULL &&
          strcmp(sqlite_core_param_clean_prefix(":id"), "id") == 0, "':id' cleans to 'id'");
    CHECK(strcmp(sqlite_core_param_clean_prefix("@param"), "param") == 0, "'@param' cleans to 'param'");
    CHECK(strcmp(sqlite_core_param_clean_prefix("?q"), "q") == 0, "'?q' cleans to 'q'");
    CHECK(strcmp(sqlite_core_param_clean_prefix("plain"), "plain") == 0, "'plain' stays 'plain'");
    CHECK(strcmp(sqlite_core_param_clean_prefix(":"), ":") == 0, "single ':' stays ':'");
    CHECK(sqlite_core_param_clean_prefix(NULL) == NULL, "NULL cleans to NULL");

    char *joined = sqlite_core_string_concat("lib", "sqlite");
    CHECK(joined != NULL && strcmp(joined, "libsqlite") == 0, "concat joins");
    sqlite_core_string_free(joined);

    char *onlyone = sqlite_core_string_concat(NULL, "solo");
    CHECK(onlyone != NULL && strcmp(onlyone, "solo") == 0, "concat tolerates NULL left");
    sqlite_core_string_free(onlyone);

    char *fmt = sqlite_core_string_format("%d-%s", 7, "x");
    CHECK(fmt != NULL && strcmp(fmt, "7-x") == 0, "format formats");
    sqlite_core_string_free(fmt);
    sqlite_core_string_free(NULL); /* must be a no-op */
    CHECK(1, "free(NULL) is safe");
}

/* ------------------------------------------------------------------ */
static void test_named_parameters(void)
{
    printf("Named parameters (.NET parity)\n");
    remove(DB_FILE);
    int rc = 0;
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    CHECK(db != NULL, "db opened");
    if (!db) return;

    CHECK(sqlite_core_execute_non_query(db, "CREATE TABLE t(id INTEGER PRIMARY KEY, name TEXT)", NULL, 0, NULL, 0) >= 0,
          "create table");

    /* bind by name, case-insensitive by default (named 'Name' vs ':name') */
    sqlite_core_param_t params[2];
    memset(params, 0, sizeof(params));

    params[0].name = "id";
    params[0].type = SQLITE_CORE_TYPE_INT;
    params[0].Direction = SQLITE_CORE_DIRECTION_INPUT;
    params[0].value.iVal = 42;

    params[1].name = "Name"; /* different case than ':name' below */
    params[1].type = SQLITE_CORE_TYPE_TEXT;
    params[1].Direction = SQLITE_CORE_DIRECTION_INPUT;
    params[1].value.blob_or_text.ptr = "Widget";
    params[1].value.blob_or_text.len = -1; /* NUL-terminated */

    int rows = sqlite_core_execute_non_query(db, "INSERT INTO t(id, name) VALUES (:id, :name)",
                                             params, 2, NULL, 0);
    CHECK(rows == 1, "named parameters bind (case-insensitive)");

    /* parameter missing from SQL is simply not bound (.NET parity) */
    params[0].value.iVal = 43;
    rows = sqlite_core_execute_non_query(db, "INSERT INTO t(name) VALUES (:name)", params, 2, NULL, 0);
    CHECK(rows == 1, "extra parameter is ignored, binding by name");

    /* scalar with a bound parameter */
    sqlite_core_param_t cnt;
    memset(&cnt, 0, sizeof(cnt));
    rc = sqlite_core_execute_scalar(db, "SELECT COUNT(*) FROM t WHERE id = :id", params, 1, &cnt, NULL, 0);
    CHECK(rc == SQLITE_OK && cnt.type == SQLITE_CORE_TYPE_INT && cnt.value.iVal == 1,
          "parameter binding works in execute_scalar");

    /* output parameters are populated from RETURNING columns by name */
    char outname[64] = "";
    memset(params, 0, sizeof(params));
    params[0].name = "name";
    params[0].type = SQLITE_CORE_TYPE_TEXT;
    params[0].Direction = SQLITE_CORE_DIRECTION_INPUT;
    params[0].value.blob_or_text.ptr = "Returned";
    params[0].value.blob_or_text.len = -1;

    params[1].name = "id";
    params[1].Direction = SQLITE_CORE_DIRECTION_OUTPUT;

    rows = sqlite_core_execute_non_query(
        db, "INSERT INTO t(name) VALUES (:name) RETURNING id", params, 2, NULL, 0);
    CHECK(rows == 1, "insert with RETURNING executes");
    CHECK(params[1].type == SQLITE_CORE_TYPE_INT && params[1].value.iVal > 0,
          "output parameter receives the RETURNING value");

    sqlite_core_close(db);
}

/* ------------------------------------------------------------------ */
static int g_reader_rows = 0;
static int g_abort_after = 0;

static int counting_callback(sqlite3_stmt *stmt, int columns, int hasrows, void *ctx)
{
    (void)stmt;
    (void)columns;
    (void)ctx;
    if (hasrows == 0)
        return 0;
    g_reader_rows++;
    return (g_abort_after > 0 && g_reader_rows >= g_abort_after) ? 1 : 0;
}

static int empty_seen = 0;

static int empty_check_callback(sqlite3_stmt *stmt, int columns, int hasrows, void *ctx)
{
    (void)stmt;
    (void)columns;
    (void)ctx;
    if (hasrows == 0)
        empty_seen = 1;
    return 0;
}

static void test_reader_callback(void)
{
    printf("Reader callback (.NET parity)\n");
    remove(DB_FILE);
    int rc = 0;
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    if (!db) { CHECK(0, "db opened"); return; }
    CHECK(1, "db opened");

    sqlite_core_execute_non_query(db, "CREATE TABLE r(v INTEGER)", NULL, 0, NULL, 0);
    for (int i = 1; i <= 5; i++)
    {
        char sql[64];
        snprintf(sql, sizeof(sql), "INSERT INTO r VALUES (%d)", i);
        sqlite_core_execute_non_query(db, sql, NULL, 0, NULL, 0);
    }

    g_reader_rows = 0;
    g_abort_after = 0;
    int rows = sqlite_core_execute_reader(db, "SELECT v FROM r ORDER BY v", NULL, 0, counting_callback, NULL, NULL, 0);
    CHECK(rows == 5 && g_reader_rows == 5, "reader visits every row");

    g_reader_rows = 0;
    g_abort_after = 3;
    rows = sqlite_core_execute_reader(db, "SELECT v FROM r ORDER BY v", NULL, 0, counting_callback, NULL, NULL, 0);
    CHECK(rows == 3 && g_reader_rows == 3, "callback abort stops iteration");

    empty_seen = 0;
    rows = sqlite_core_execute_reader(db, "SELECT v FROM r WHERE 1=0", NULL, 0, empty_check_callback, NULL, NULL, 0);
    CHECK(rows == 0 && empty_seen == 1, "empty result set notifies the callback once");

    sqlite_core_close(db);
}

/* ------------------------------------------------------------------ */
static void test_error_paths(void)
{
    printf("Error paths\n");
    int rc = 0;

    sqlite3 *missing = sqlite_core_open_readonly("no_such_file_xyz.db", &rc);
    CHECK(missing == NULL && rc == SQLITE_CANTOPEN, "read-only open of a missing file fails");

    sqlite3 *bad = sqlite_core_open_ex("whatever.db", 0, &rc);
    CHECK(bad == NULL && rc == SQLITE_MISUSE, "invalid open flags are rejected");
    bad = sqlite_core_open_ex(NULL, SQLITE_OPEN_READWRITE, &rc);
    CHECK(bad == NULL && rc == SQLITE_MISUSE, "NULL path is rejected");

    CHECK(sqlite_core_execute_non_query(NULL, "SELECT 1", NULL, 0, NULL, 0) == SQLITE_MISUSE,
          "NULL db rejected by execute_non_query");
    CHECK(sqlite_core_execute_scalar(NULL, "SELECT 1", NULL, 0, NULL, NULL, 0) == SQLITE_MISUSE,
          "NULL db rejected by execute_scalar");
    CHECK(sqlite_core_execute_reader(NULL, "SELECT 1", NULL, 0, counting_callback, NULL, NULL, 0) == SQLITE_MISUSE,
          "NULL db rejected by execute_reader");
    CHECK(sqlite_core_close(NULL) == SQLITE_MISUSE, "NULL db rejected by close");
    CHECK(sqlite_core_is_readonly(NULL) == -1, "NULL db gives -1 from is_readonly");
    CHECK(sqlite_core_error_code(NULL) == 0, "NULL db gives 0 from error_code");

    remove(DB_FILE);
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    if (db)
    {
        char err[256] = "";
        int r = sqlite_core_execute_non_query(db, "SELECT 1; SELECT 2", NULL, 0, err, sizeof(err));
        CHECK(r == SQLITE_MISUSE, "multi-statement rejected by execute_non_query");
        CHECK(err[0] != '\0', "error buffer filled on misuse");

        err[0] = '\0';
        r = sqlite_core_execute_command(db, "CREATE TABLE a(x); CREATE TABLE b(y);", err, sizeof(err));
        CHECK(r == SQLITE_OK, "execute_command runs multiple statements");
        r = sqlite_core_execute_command(db, "CREATE TABLE a(x)", err, sizeof(err));
        CHECK(r == SQLITE_ERROR && err[0] != '\0', "duplicate table reported with message");

        /* busy timeout clamping: valid values succeed */
        CHECK(sqlite_core_set_busy_timeout(db, 0) == 1, "set_busy_timeout(0)");
        CHECK(sqlite_core_set_busy_timeout(db, 30000) == 1, "set_busy_timeout(30000)");
        CHECK(sqlite_core_set_busy_timeout(db, 999999) == 1, "set_busy_timeout clamps above max");
        CHECK(sqlite_core_set_busy_timeout(db, -5) == 1, "set_busy_timeout clamps below zero");
        CHECK(sqlite_core_set_busy_timeout(NULL, 100) == SQLITE_MISUSE, "set_busy_timeout rejects NULL");

        sqlite_core_close(db);
    }
    remove(DB_FILE);
}

/* ------------------------------------------------------------------ */
static void test_encoding_roundtrip(void)
{
    printf("Encoding round-trip\n");
    const char *utf8 = "Acento: \xC3\xA1\xC3\xA9\xC3\xAD\xC3\xB3\xC3\xBA \xE2\x82\xAC";
    char *ansi = sqlite_core_string_utf8_to_ansi(utf8);
    CHECK(ansi != NULL, "utf8 -> ansi converts");
    char *back = ansi ? sqlite_core_string_ansi_to_utf8(ansi) : NULL;
    CHECK(back != NULL && strcmp(back, utf8) == 0, "ansi -> utf8 round-trips exactly");
    sqlite_core_string_free(ansi);
    sqlite_core_string_free(back);

    CHECK(sqlite_core_string_utf8_to_ansi(NULL) == NULL, "NULL input rejected (utf8->ansi)");
    CHECK(sqlite_core_string_ansi_to_utf8(NULL) == NULL, "NULL input rejected (ansi->utf8)");
}

/* ------------------------------------------------------------------ */
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("libsqlite3 API tests\n");
    printf("library %s / sqlite %s\n\n", sqlite_core_version(), sqlite_core_sqlite_version());

    test_param_helpers();
    test_named_parameters();
    test_reader_callback();
    test_error_paths();
    test_encoding_roundtrip();

    remove(DB_FILE);

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures;
}