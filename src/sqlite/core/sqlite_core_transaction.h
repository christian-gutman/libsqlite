#ifndef SQLITE_CORE_TRANSACTION_H
#define SQLITE_CORE_TRANSACTION_H

#include "sqlite_core.h"
#include "sqlite_core_command.h"
    
SQLITE_CORE_API int sqlite_core_begin_transaction(sqlite3 *db, char *err_buf, int err_buf_max);
SQLITE_CORE_API int sqlite_core_commit_transaction(sqlite3 *db, char *err_buf, int err_buf_max);
SQLITE_CORE_API int sqlite_core_rollback_transaction(sqlite3 *db, char *err_buf, int err_buf_max);

#endif // SQLITE_CORE_TRANSACTION_H