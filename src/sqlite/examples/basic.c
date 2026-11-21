/*
 * basic.c — the smallest useful program with libsqlite:
 * open or create, run DDL, insert, read rows with a callback, close.
 *
 * Every public call is validated: the process exits with the number of
 * failed checks, so it can be used as a smoke test (it is registered as
 * one in CTest). Build instructions: examples/README.md.
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

static const char *DB_FILE = "basic.db";

/* Reader callback: one call per row (hasrows == 1). This example keeps
 * the callback minimal because the shared (DLL) build exports only the
 * sqlite_core_* surface of sqlite.def; reading individual columns
 * through the engine API needs a static link (see examples/README.md). */
static int count_row(sqlite3_stmt *stmt, int columns, int hasrows, void *ctx)
{
    (void)stmt;
    (void)columns;
    if (hasrows)
        (*(int *)ctx) += 1;
    return 0; /* return non-zero to abort iteration */
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("basic example\n");
    printf("library %s / engine %s\n\n",
           sqlite_core_version(), sqlite_core_sqlite_version());

    remove(DB_FILE);

    /* 1. Open (creates the file when missing). out_rc carries the engine
     *    result code when the handle itself is NULL. */
    int rc = 0;
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    CHECK(db != NULL && rc == SQLITE_OK, "database opened or created");
    if (db == NULL)
    {
        fprintf(stderr, "open failed: rc=%d\n", rc);
        return 1;
    }

    /* 2. DDL: execute_command runs one or more statements. */
    char err[256] = "";
    rc = sqlite_core_execute_command(
        db, "CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT)", err, sizeof(err));
    CHECK(rc == SQLITE_OK, "table created");

    /* 3. DML: execute_non_query returns the affected rows; on failure it
     *    fills err with the engine message and returns the SQLite code. */
    int rows = sqlite_core_execute_non_query(
        db, "INSERT INTO person(name) VALUES ('Ada')", NULL, 0, err, sizeof(err));
    CHECK(rows == 1, "one row inserted");
    rows = sqlite_core_execute_non_query(
        db, "INSERT INTO person(name) VALUES ('Grace')", NULL, 0, err, sizeof(err));
    CHECK(rows == 1, "second row inserted");

    /* 4. Read: one callback per row; the return value is the rows seen. */
    int seen = 0;
    rows = sqlite_core_execute_reader(
        db, "SELECT id, name FROM person ORDER BY id", NULL, 0,
        count_row, &seen, err, sizeof(err));
    CHECK(rows == 2 && seen == 2, "reader visited both rows");

    /* 4b. Print the rows: the same set serialized as a JSON array. */
    char *json = NULL;
    rows = sqlite_core_execute_reader_to_json(
        db, "SELECT id, name FROM person ORDER BY id", NULL, 0,
        &json, err, sizeof(err));
    CHECK(rows == 2 && json != NULL, "result set serialized to JSON");
    if (json != NULL)
    {
        printf("json: %s\n", json);
        sqlite_core_string_free(json);
    }

    /* 5. Close is idempotent and safe on NULL; 1 means success. */
    CHECK(sqlite_core_close(db) == 1, "database closed");

    remove(DB_FILE);

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures;
}