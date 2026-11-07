#ifndef SQLITE_CORE_COMMAND_H
#define SQLITE_CORE_COMMAND_H

#include "sqlite_core.h" 

int SqlLite_Core_IsForbiddenTransactionalText(const char *sql);

CORE_API int SqlLite_Core_ExecuteCommand(
    sqlite3 *db,
    const char *sql_command,
    char *err_buf,
    int err_buf_max);
    
CORE_API int SqlLite_Core_ExecuteTransaction(
    sqlite3 *db,
    const char *sql_command,
    int txexpected,
    char *err_buf,
    int err_buf_max);

CORE_API int SqlLite_Core_ExecuteNonQuery(
    sqlite3 *db,
    const char *sql,
    SqlLiteCoreParam *params,
    int paramCount,
    char *err_buf,
    int err_buf_max);

CORE_API int SqlLite_Core_ExecuteScalar(
    sqlite3 *db,
    const char *sql,
    SqlLiteCoreParam *params,
    int paramCount,
    SqlLiteCoreParam *out_scalar_result,
    char *err_buf,
    int err_buf_max);

CORE_API int SqlLite_Core_ExecuteReader(
    sqlite3 *db,
    const char *sql,
    SqlLiteCoreParam *params,
    int paramCount,
    SqlLiteCoreRowCallback reader_callback,
    void *pUserContext,
    char *err_buf, int err_buf_max);



#endif // SQLITE_CORE_COMMAND_H