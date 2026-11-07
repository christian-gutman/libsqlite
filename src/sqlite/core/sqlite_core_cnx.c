#include "sqlite_core_cnx.h"

#ifndef CORE_MAX_BUSY_TIMEOUT
// Límite máximo seguro: 30 segundos (30000 ms)
#define CORE_MAX_BUSY_TIMEOUT 30000
#endif

sqlite3 *SqlLite_Core_OpenEx(
    const char *uripath,
    int flags,
    int *out_rc)
{
    if (!uripath)
    {
        if (out_rc)
            *out_rc = SQLITE_MISUSE;
        return NULL;
    }

    int access_mode = flags & (SQLITE_OPEN_READONLY | SQLITE_OPEN_READWRITE);
    if (access_mode != SQLITE_OPEN_READONLY && access_mode != SQLITE_OPEN_READWRITE)
    {
        if (out_rc)
            *out_rc = SQLITE_MISUSE;
        return NULL;
    }

    if ((flags & SQLITE_OPEN_CREATE) && (access_mode == SQLITE_OPEN_READONLY))
    {
        if (out_rc)
            *out_rc = SQLITE_MISUSE;
        return NULL;
    }

    int flags_validmask = SQLITE_OPEN_READONLY

                          | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI | SQLITE_OPEN_MEMORY | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_PRIVATECACHE;

    if ((flags & ~flags_validmask) != 0)
    {
        if (out_rc)
            *out_rc = SQLITE_MISUSE;
        return NULL;
    }

    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(uripath, &db, flags, NULL);
    if (out_rc)
        *out_rc = rc;

    if (rc != SQLITE_OK)
    {
        if (db != NULL)
        {
            sqlite3_close_v2(db);
        }
        return NULL;
    }
    return db;
}

sqlite3 *SqlLite_Core_Open_ReadOnly(const char *uripath, int *out_rc)
{
    return SqlLite_Core_OpenEx(uripath, SQLITE_OPEN_READONLY, out_rc);
}

sqlite3 *SqlLite_Core_Open_Or_Create(const char *uripath, int *out_rc)
{
    return SqlLite_Core_OpenEx(uripath, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, out_rc);
}

sqlite3 *SqlLite_Core_Open(const char *uripath, int *out_rc)
{
    return SqlLite_Core_OpenEx(uripath, SQLITE_OPEN_READWRITE, out_rc);
}

sqlite3 *SqlLite_Core_Open_Uri_ReadOnly(const char *uripath, int *out_rc)
{
    sqlite3_config(SQLITE_CONFIG_URI, 1);
    return SqlLite_Core_OpenEx(uripath, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, out_rc);
}

int SqlLite_Core_Close(sqlite3 *db)
{
    if (db == NULL)
        return SQLITE_MISUSE;

    int rc = sqlite3_close_v2(db);
    return (rc == SQLITE_OK) ? 1 : 0;
}

int SqlLite_Core_IsReadonly(sqlite3 *db)
{
    if (db == NULL)
        return -1;

    int rc = sqlite3_db_readonly(db, "main");
    return (rc == SQLITE_OK) ? 1 : 0;
}

int SqlLite_Core_SetBusyTimeout(sqlite3 *db, int ms)
{
    if (db == NULL)
    {
        return SQLITE_MISUSE;
    }
    if (ms < 0)
    {
        ms = 0;
    }
    if (ms > CORE_MAX_BUSY_TIMEOUT)
    {
        ms = CORE_MAX_BUSY_TIMEOUT;
    }
    int rc = sqlite3_busy_timeout(db, ms);
    return (rc == SQLITE_OK) ? 1 : 0;
}
