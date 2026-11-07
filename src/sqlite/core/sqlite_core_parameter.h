#ifndef SQLITE_CORE_PARAMETER_H
#define SQLITE_CORE_PARAMETER_H

#include "sqlite_core.h" 

CORE_API const char *SqlLite_Core_CleanPrefix(const char *name);

void SqlLite_Core_BindParameters(
    sqlite3_stmt *stmt,
    SqlLiteCoreParam *params,
    int paramCount);

void SqlLite_Core_ProcessOutputParameters(
    sqlite3_stmt *stmt,
    SqlLiteCoreParam *params,
    int paramCount);
    
#endif // SQLITE_CORE_PARAMETER_H