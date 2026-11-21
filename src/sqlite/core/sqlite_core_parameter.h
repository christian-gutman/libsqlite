#ifndef SQLITE_CORE_PARAMETER_H
#define SQLITE_CORE_PARAMETER_H

#include "sqlite_core.h" 

SQLITE_CORE_API const char *sqlite_core_param_clean_prefix(const char *name);

void sqlite_core_param_bind(
    sqlite3_stmt *stmt,
    sqlite_core_param_t *params,
    int paramCount);

void sqlite_core_param_process_outputs(
    sqlite3_stmt *stmt,
    sqlite_core_param_t *params,
    int paramCount);
    
#endif // SQLITE_CORE_PARAMETER_H