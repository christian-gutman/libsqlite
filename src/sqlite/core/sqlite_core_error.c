#include "sqlite_core_error.h"

int SqlLite_Core_GetLastErrorCode(sqlite3 *db)
{
    if (db == NULL)
    {
        return 0;
    }
    return sqlite3_errcode(db);
}
int SqlLite_Core_GetLastErrorCodeExtended(sqlite3 *db)
{
    if (db == NULL)
    {
        return 0;
    }
    return sqlite3_extended_errcode(db);
}

const char* SqlLite_Core_GetLastErrorMsg(sqlite3 *db)
{
    if (db == NULL)
    {
        return "";
    }
    return sqlite3_errmsg(db);
}
 