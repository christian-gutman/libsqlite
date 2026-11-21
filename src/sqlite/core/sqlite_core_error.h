#ifndef SQLITE_CORE_ERROR_H
#define SQLITE_CORE_ERROR_H

#include "sqlite_core.h"
 
SQLITE_CORE_API int sqlite_core_error_code(sqlite3 *db);
SQLITE_CORE_API int sqlite_core_error_code_extended(sqlite3 *db);
SQLITE_CORE_API const char* sqlite_core_error_message(sqlite3 *db);

#endif // SQLITE_CORE_ERROR_H