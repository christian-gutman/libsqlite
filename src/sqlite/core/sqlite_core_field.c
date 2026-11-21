#include "sqlite_core_field.h"

/*
 * Reads the first BLOB value of a query result into a caller-owned buffer.
 *
 * Returns:
 *   SQLITE_OK       success (see *pOutCopied for the number of bytes copied)
 *   -2              buffer too small (maxLen bytes copied, value truncated)
 *   SQLITE_MISUSE   invalid arguments
 *   other SQLite    prepare/step error code
 */
int sqlite_core_get_blob(
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
    if (maxLen < 0)
        maxLen = 0;

    /* Same ANSI -> UTF-8 SQL conversion as every other Execute* entry */
    const char *final_sql = sql;
    char *converted_sql = sqlite_core_string_ansi_to_utf8(sql);
    if (converted_sql != NULL)
        final_sql = converted_sql;

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, final_sql, -1, &stmt, NULL);

    if (converted_sql != NULL)
        sqlite_core_string_free((void *)converted_sql);

    if (rc != SQLITE_OK)
    {
        return rc;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        const void *pInternalBlob = sqlite3_column_blob(stmt, 0);
        int blobLen = sqlite3_column_bytes(stmt, 0);

        if (blobLen > 0 && pInternalBlob != NULL)
        {
            int bytesToCopy = (blobLen > maxLen) ? maxLen : blobLen;

            memcpy(pDestBuffer, pInternalBlob, (size_t)bytesToCopy);

            *pOutCopied = bytesToCopy;

            if (blobLen > maxLen)
            {
                sqlite3_finalize(stmt);
                return SQLITE_CORE_BUFFER_TOO_SMALL;
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