#ifndef SQLITE_CORE_CNX_H
#define SQLITE_CORE_CNX_H

#include "sqlite_core.h"

sqlite3* SqlLite_Core_OpenEx(
    const char *uripath,
    int flags,
    int *out_rc);

CORE_API sqlite3* SqlLite_Core_Open_ReadOnly(
    const char *uripath,
    int *out_rc);

CORE_API sqlite3* SqlLite_Core_Open_Or_Create(
    const char *uripath,
    int *out_rc);
    
CORE_API sqlite3* SqlLite_Core_Open(
    const char *uripath,
    int *out_rc);

CORE_API sqlite3* SqlLite_Core_Open_Uri_ReadOnly(const char *uripath, int *out_rc);

CORE_API int SqlLite_Core_Close(sqlite3 *db);

CORE_API int SqlLite_Core_IsReadonly(sqlite3 *db);

CORE_API int SqlLite_Core_SetBusyTimeout(sqlite3 *db, int ms);

#endif // SQLITE_CORE_CNX_H
