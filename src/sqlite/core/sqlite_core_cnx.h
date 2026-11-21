#ifndef SQLITE_CORE_CNX_H
#define SQLITE_CORE_CNX_H

#include "sqlite_core.h"

sqlite3* sqlite_core_open_ex(
    const char *uripath,
    int flags,
    int *out_rc);

SQLITE_CORE_API sqlite3* sqlite_core_open_readonly(
    const char *uripath,
    int *out_rc);

SQLITE_CORE_API sqlite3* sqlite_core_open_or_create(
    const char *uripath,
    int *out_rc);
    
SQLITE_CORE_API sqlite3* sqlite_core_open(
    const char *uripath,
    int *out_rc);

SQLITE_CORE_API sqlite3* sqlite_core_open_uri_readonly(const char *uripath, int *out_rc);

SQLITE_CORE_API int sqlite_core_close(sqlite3 *db);

SQLITE_CORE_API int sqlite_core_is_readonly(sqlite3 *db);

SQLITE_CORE_API int sqlite_core_set_busy_timeout(sqlite3 *db, int ms);

#endif // SQLITE_CORE_CNX_H
