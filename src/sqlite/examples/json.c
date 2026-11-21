/*
 * json.c — reading a result set as JSON and inserting a JSON batch.
 * Both directions go through the embedded engine (SQLite JSON1), so
 * nested documents and escaped strings are handled natively.
 *
 * Every public call is validated: the process exits with the number of
 * failed checks. Build instructions: examples/README.md.
 */

#include "sqlite_core.h"

#include <stdio.h>
#include <string.h>

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond, msg)                                    \
    do                                                      \
    {                                                       \
        g_checks++;                                         \
        if (cond)                                           \
        {                                                   \
            printf("  ok   - %s\n", msg);                   \
        }                                                   \
        else                                                \
        {                                                   \
            g_failures++;                                   \
            printf("  FAIL - %s\n", msg);                   \
        }                                                   \
    } while (0)

static const char *DB_FILE = "json.db";

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("json example\n\n");

    remove(DB_FILE);

    int rc = 0;
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    CHECK(db != NULL && rc == SQLITE_OK, "database opened or created");
    if (db == NULL)
        return 1;

    char err[256] = "";
    CHECK(sqlite_core_execute_command(db,
              "CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT)", err, sizeof(err)) ==
              SQLITE_OK,
          "table created");

    /* --- Read a result set as a JSON array of objects ---------------- */
    CHECK(sqlite_core_execute_non_query(
              db, "INSERT INTO person(name) VALUES ('Ada')", NULL, 0, err, sizeof(err)) == 1,
          "row inserted");

    char *json = NULL;
    int rows = sqlite_core_execute_reader_to_json(
        db, "SELECT id, name FROM person ORDER BY id", NULL, 0, &json, err, sizeof(err));
    CHECK(rows == 1 && json != NULL, "result set serialized to JSON");
    if (json != NULL)
    {
        printf("json: %s\n", json);
        CHECK(json[0] == '[' && json[strlen(json) - 1] == ']',
              "output is a JSON array");
        sqlite_core_string_free(json); /* released with the library allocator */
        json = NULL;
    }

    /* Empty result set produces "[]" (not NULL). */
    rows = sqlite_core_execute_reader_to_json(
        db, "SELECT id FROM person WHERE 1 = 0", NULL, 0, &json, err, sizeof(err));
    CHECK(rows == 0 && json != NULL && strcmp(json, "[]") == 0,
          "empty set is []");
    if (json != NULL)
    {
        sqlite_core_string_free(json);
        json = NULL;
    }

    /* --- Insert a JSON array of objects ------------------------------- */
    const char *batch =
        "[{\"id\": 10, \"name\": \"Line1\"},"
        " {\"id\": 11, \"name\": \"Line2\"},"
        " {\"id\": 12, \"name\": \"Quote\\\"inside\"}]";

    rows = sqlite_core_insert_json(db, "person", batch, err, sizeof(err));
    CHECK(rows == 3, "json batch inserted");

    /* Values are parsed server-side: the escaped quote survives. */
    sqlite_core_param_t out;
    memset(&out, 0, sizeof(out));
    rc = sqlite_core_execute_scalar(
        db, "SELECT COUNT(*) FROM person WHERE name LIKE 'Quote%'", NULL, 0,
        &out, err, sizeof(err));
    CHECK(rc == SQLITE_OK && out.value.iVal == 1, "escaped string stored verbatim");

    /* --- Error path: invalid JSON returns a negative code ------------- */
    rows = sqlite_core_insert_json(db, "person", "not json", err, sizeof(err));
    CHECK(rows < 0 && err[0] != '\0', "invalid JSON reports an error");

    CHECK(sqlite_core_close(db) == 1, "database closed");

    remove(DB_FILE);

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures;
}