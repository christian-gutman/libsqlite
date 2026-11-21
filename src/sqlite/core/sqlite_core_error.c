#include "sqlite_core_error.h"

int sqlite_core_error_code(sqlite3 *db)
{
    if (db == NULL)
    {
        return 0;
    }
    return sqlite3_errcode(db);
}
int sqlite_core_error_code_extended(sqlite3 *db)
{
    if (db == NULL)
    {
        return 0;
    }
    return sqlite3_extended_errcode(db);
}

const char* sqlite_core_error_message(sqlite3 *db)
{
    if (db == NULL)
    {
        return "";
    }
    return sqlite3_errmsg(db);
}
 