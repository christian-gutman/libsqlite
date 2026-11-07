#ifndef SQLITE_CORE_TRANSACTION_H
#define SQLITE_CORE_TRANSACTION_H

#include "sqlite_core.h"
#include "sqlite_core_command.h"
    
CORE_API int SqlLite_Core_BeginTransaction(sqlite3 *db, char *err_buf, int err_buf_max);
CORE_API int SqlLite_Core_Commit(sqlite3 *db, char *err_buf, int err_buf_max);
CORE_API int SqlLite_Core_Rollback(sqlite3 *db, char *err_buf, int err_buf_max);

#endif // SQLITE_CORE_TRANSACTION_H