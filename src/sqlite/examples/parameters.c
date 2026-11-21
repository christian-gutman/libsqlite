/*
 * parameters.c — named parameters, prefix cleaning, scalar reads and
 * output parameters via RETURNING (.NET-style parameter directions).
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

static const char *DB_FILE = "parameters.db";

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("parameters example\n\n");

    remove(DB_FILE);

    int rc = 0;
    sqlite3 *db = sqlite_core_open_or_create(DB_FILE, &rc);
    CHECK(db != NULL && rc == SQLITE_OK, "database opened or created");
    if (db == NULL)
        return 1;

    char err[256] = "";
    CHECK(sqlite_core_execute_command(db,
              "CREATE TABLE item(id INTEGER PRIMARY KEY, name TEXT, price REAL)",
              err, sizeof(err)) == SQLITE_OK,
          "table created");

    /* --- Binding by name (never by ordinal) ------------------------- */
    sqlite_core_param_t p[2];
    memset(p, 0, sizeof(p));

    p[0].name = "name";
    p[0].type = SQLITE_CORE_TYPE_TEXT;
    p[0].Direction = SQLITE_CORE_DIRECTION_INPUT;
    p[0].value.blob_or_text.ptr = "Widget";
    p[0].value.blob_or_text.len = -1; /* NUL-terminated */

    p[1].name = "price";
    p[1].type = SQLITE_CORE_TYPE_DOUBLE;
    p[1].Direction = SQLITE_CORE_DIRECTION_INPUT;
    p[1].value.dVal = 9.99;

    int rows = sqlite_core_execute_non_query(
        db, "INSERT INTO item(name, price) VALUES (:name, :price)",
        p, 2, err, sizeof(err));
    CHECK(rows == 1, "insert with named parameters");

    /* Extra parameters are ignored, like in .NET: binding is by name. */
    rows = sqlite_core_execute_non_query(
        db, "INSERT INTO item(name) VALUES (:name)", p, 2, err, sizeof(err));
    CHECK(rows == 1, "parameter missing from the SQL is ignored");

    /* --- Prefix cleaning: ":price" -> "price" ------------------------ */
    const char *clean = sqlite_core_param_clean_prefix(":price");
    CHECK(clean != NULL && strcmp(clean, "price") == 0, "clean_prefix strips the sigil");

    /* --- Scalar: first column of the first row ---------------------- */
    sqlite_core_param_t out;
    memset(&out, 0, sizeof(out));
    out.type = SQLITE_CORE_TYPE_INT;

    rc = sqlite_core_execute_scalar(
        db, "SELECT COUNT(*) FROM item", NULL, 0, &out, err, sizeof(err));
    CHECK(rc == SQLITE_OK && out.type == SQLITE_CORE_TYPE_INT &&
          out.value.iVal == 2, "scalar returns the row count");

    /* --- Output parameters: populated from RETURNING columns --------- */
    memset(p, 0, sizeof(p));
    p[0].name = "name";
    p[0].type = SQLITE_CORE_TYPE_TEXT;
    p[0].Direction = SQLITE_CORE_DIRECTION_INPUT;
    p[0].value.blob_or_text.ptr = "Returned";
    p[0].value.blob_or_text.len = -1;

    p[1].name = "id";
    p[1].Direction = SQLITE_CORE_DIRECTION_OUTPUT;

    rows = sqlite_core_execute_non_query(
        db, "INSERT INTO item(name) VALUES (:name) RETURNING id",
        p, 2, err, sizeof(err));
    CHECK(rows == 1, "insert with RETURNING executes");
    CHECK(p[1].type == SQLITE_CORE_TYPE_INT && p[1].value.iVal > 0,
          "output parameter receives the RETURNING value");

    /* --- Error path: a bad statement reports code + message --------- */
    rows = sqlite_core_execute_non_query(
        db, "INSERT INTO missing_table VALUES (1)", NULL, 0, err, sizeof(err));
    CHECK(rows != SQLITE_OK && err[0] != '\0', "errors report code and message");

    CHECK(sqlite_core_close(db) == 1, "database closed");

    remove(DB_FILE);

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures;
}