#ifndef SQLITE_CORE_FIELD_H
#define SQLITE_CORE_FIELD_H

#include "sqlite_core.h"

/* sqlite_core_get_blob: the destination buffer did not fit the BLOB
 * (maxLen bytes were copied). */
#define SQLITE_CORE_BUFFER_TOO_SMALL (-2)

SQLITE_CORE_API int sqlite_core_get_blob(
    sqlite3 *db,
    const char *sql,
    void *pDestBuffer,
    int maxLen,
    int *pOutCopied);

#endif // SQLITE_CORE_FIELD_H