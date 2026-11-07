#ifndef SQLITE_CORE_FIELD_H
#define SQLITE_CORE_FIELD_H

#include "sqlite_core.h"

int SqlLite_Core_GetBlob(
    sqlite3* db, 
    const char* sql, 
    void* pDestBuffer, 
    int maxLen, 
    int* pOutCopied);
 
#endif // SQLITE_CORE_FIELD_H