#include "sqlite_core_field.h"

int SqlLite_Core_GetBlob(
    sqlite3 *db,
    const char *sql,
    void *pDestBuffer,
    int maxLen,
    int *pOutCopied)
{
    if (!db || !sql || !pDestBuffer || !pOutCopied)
    {
        return SQLITE_MISUSE;
    }

    *pOutCopied = 0;
    sqlite3_stmt *stmt = NULL;
    int rc = 0;

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        return rc;
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {

        int blobLen = sqlite3_column_bytes(stmt, 0);

        if (blobLen > 0)
        {
            const void *pInternalBlob = sqlite3_column_blob(stmt, 0);

            if (pInternalBlob)
            {
                int bytesToCopy = (blobLen > maxLen) ? maxLen : blobLen;

                memcpy(pDestBuffer, pInternalBlob, bytesToCopy);

                *pOutCopied = bytesToCopy;

                if (blobLen > maxLen)
                {
                    sqlite3_finalize(stmt);
                    return -2;
                }
            }
        }
        rc = SQLITE_OK;
    }
    else if (rc == SQLITE_DONE)
    {
        rc = SQLITE_OK;
    }
    sqlite3_finalize(stmt);
    return rc;
}
