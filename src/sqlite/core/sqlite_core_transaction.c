#include "sqlite_core_transaction.h"

/* Begins a transaction (BEGIN IMMEDIATE) on the connection.
 * Fails if a transaction is already active. */
int sqlite_core_begin_transaction(sqlite3 *db, char *err_buf, int err_buf_max) {
    /* [txexpected = 1]: to start a new transaction, autocommit must be on
     * (no active transaction). A nested BEGIN is rejected with -1. */
    return sqlite_core_execute_transaction(db, "BEGIN IMMEDIATE TRANSACTION;", 1, err_buf, err_buf_max);
}

/* Commits the active transaction. Fails when no transaction is open. */
int sqlite_core_commit_transaction(sqlite3 *db, char *err_buf, int err_buf_max) {
    /* [txexpected = 0]: autocommit must be off (transaction active). A
     * stray COMMIT is rejected with -1. */
    return sqlite_core_execute_transaction(db, "COMMIT TRANSACTION;", 0, err_buf, err_buf_max);
}

/* Rolls back the active transaction. Fails when no transaction is open. */
int sqlite_core_rollback_transaction(sqlite3 *db, char *err_buf, int err_buf_max) {
    /* [txexpected = 0]: same guard as Commit. */
    return sqlite_core_execute_transaction(db, "ROLLBACK TRANSACTION;", 0, err_buf, err_buf_max);
}