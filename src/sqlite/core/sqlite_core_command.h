#ifndef SQLITE_CORE_COMMAND_H
#define SQLITE_CORE_COMMAND_H

#include "sqlite_core.h" 

int sqlite_core_is_forbidden_transaction_text(const char *sql);

SQLITE_CORE_API int sqlite_core_execute_command(
    sqlite3 *db,
    const char *sql_command,
    char *err_buf,
    int err_buf_max);
    
SQLITE_CORE_API int sqlite_core_execute_transaction(
    sqlite3 *db,
    const char *sql_command,
    int txexpected,
    char *err_buf,
    int err_buf_max);

SQLITE_CORE_API int sqlite_core_execute_non_query(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    char *err_buf,
    int err_buf_max);

SQLITE_CORE_API int sqlite_core_execute_scalar(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    sqlite_core_param_t *out_scalar_result,
    char *err_buf,
    int err_buf_max);

SQLITE_CORE_API int sqlite_core_execute_reader(
    sqlite3 *db,
    const char *sql,
    sqlite_core_param_t *params,
    int paramCount,
    sqlite_core_row_callback_fn reader_callback,
    void *pUserContext,
    char *err_buf, int err_buf_max);



#endif // SQLITE_CORE_COMMAND_H