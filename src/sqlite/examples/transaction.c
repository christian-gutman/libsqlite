/*
 * transaction.c — explicit BEGIN / COMMIT / ROLLBACK control, including
 * the failure paths required by docs/behavior.md.
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

static const char *DB_FILE = "transaction.db";

static sqlite3 *open_fresh(void)
{
    remove(DB_FILE);
    int rc = 0;
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    if (db == NULL)
        return NULL;
    char err[128] = "";
    sqlite_core_execute_command(db,
        "CREATE TABLE kv(k TEXT PRIMARY KEY, v TEXT)", err, sizeof(err));
    return db;
}

static int count_rows(sqlite3 *db)
{
    sqlite_core_param_t out;
    memset(&out, 0, sizeof(out));
    out.type = SQLITE_CORE_TYPE_INT;
    char err[128] = "";
    if (sqlite_core_execute_scalar(db, "SELECT COUNT(*) FROM kv",
                                   NULL, 0, &out, err, sizeof(err)) == SQLITE_OK)
        return (int)out.value.iVal;
    return -1;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("transaction example\n\n");

    sqlite3 *db = open_fresh();
    CHECK(db != NULL, "database opened");
    if (db == NULL)
        return 1;

    char err[256] = "";

    /* --- Happy path: BEGIN IMMEDIATE ... COMMIT --------------------- */
    CHECK(sqlite_core_begin_transaction(db, err, sizeof(err)) == 1, "begin");
    CHECK(sqlite_core_execute_non_query(
              db, "INSERT INTO kv VALUES ('a', '1')", NULL, 0, err, sizeof(err)) == 1,
          "insert inside transaction");
    CHECK(sqlite_core_commit_transaction(db, err, sizeof(err)) == 1, "commit");
    CHECK(count_rows(db) == 1, "committed row is visible");

    /* --- Guard rails from the behavior contract ---------------------- */
    CHECK(sqlite_core_commit_transaction(db, err, sizeof(err)) == -1,
          "commit without an active transaction fails");
    CHECK(sqlite_core_rollback_transaction(db, err, sizeof(err)) == -1,
          "rollback without an active transaction fails");
    CHECK(sqlite_core_begin_transaction(db, err, sizeof(err)) == 1, "begin again");
    CHECK(sqlite_core_begin_transaction(db, err, sizeof(err)) == -1,
          "nested begin fails");
    CHECK(sqlite_core_rollback_transaction(db, err, sizeof(err)) == 1, "rollback");

    /* --- Error inside the transaction: roll it back by hand --------- */
    CHECK(sqlite_core_begin_transaction(db, err, sizeof(err)) == 1, "begin");
    CHECK(sqlite_core_execute_non_query(
              db, "INSERT INTO kv VALUES ('b', '2')", NULL, 0, err, sizeof(err)) == 1,
          "insert inside transaction");
    CHECK(sqlite_core_rollback_transaction(db, err, sizeof(err)) == 1, "rollback");
    CHECK(count_rows(db) == 1, "rolled-back row is gone");

    /* --- Guard against transactions in command text ----------------- */
    CHECK(sqlite_core_execute_command(db, "BEGIN", err, sizeof(err)) >= 0,
          "transaction text is still possible via execute_command (low level)");
    CHECK(sqlite_core_commit_transaction(db, err, sizeof(err)) == 1, "commit that one");

    CHECK(sqlite_core_close(db) == 1, "database closed");

    remove(DB_FILE);

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures;
}