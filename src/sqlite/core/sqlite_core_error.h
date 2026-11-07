#ifndef SQLITE_CORE_ERROR_H
#define SQLITE_CORE_ERROR_H

#include "sqlite_core.h"
 
CORE_API int SqlLite_Core_GetLastErrorCode(sqlite3 *db);
CORE_API int SqlLite_Core_GetLastErrorCodeExtended(sqlite3 *db);
CORE_API const char* SqlLite_Core_GetLastErrorMsg(sqlite3 *db);

#endif // SQLITE_CORE_ERROR_H