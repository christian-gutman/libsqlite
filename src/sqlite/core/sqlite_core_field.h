#ifndef SQLITE_CORE_FIELD_H
#define SQLITE_CORE_FIELD_H

#include "sqlite_core.h"

/* sqlite_core_get_blob: the destination buffer did not fit the BLOB
 * (maxLen bytes were copied). */
#define SQLITE_CORE_BUFFER_TOO_SMALL (-2)

/* sqlite_core_update_blob: the existing column is smaller than the file
 * being streamed into it (nothing was written). */
#define SQLITE_CORE_BLOB_TOO_SMALL (-3)

SQLITE_CORE_API int sqlite_core_get_blob(
    sqlite3 *db,
    const char *sql,
    void *pDestBuffer,
    int maxLen,
    int *pOutCopied);

/* Writes the contents of a file into a BLOB column, streaming it through
 * the incremental blob I/O API in chunks (no full-buffer binding, so the
 * memory footprint is constant regardless of the file size).
 *
 * rowid == 0 inserts a new row; rowid > 0 updates that row when it exists
 * and creates it with that rowid otherwise (upsert).
 *
 * Returns the number of bytes written (>= 0) on success, or:
 *   SQLITE_MISUSE          invalid arguments
 *   SQLITE_CANTOPEN        the file could not be opened/read
 *   SQLITE_CORE_BLOB_TOO_SMALL  the file is bigger than an existing blob
 *   -1 / SQLite codes      engine or allocation failure (see err_buf)
 */
SQLITE_CORE_API int sqlite_core_insert_blob(
    sqlite3 *db,
    const char *table,
    const char *column,
    sqlite3_int64 rowid,
    const char *file_path,
    int chunk_size,
    int *p_out_written,
    char *err_buf,
    int err_buf_max);

/* Same streaming write onto an existing row (rowid required). */
SQLITE_CORE_API int sqlite_core_update_blob(
    sqlite3 *db,
    const char *table,
    const char *column,
    sqlite3_int64 rowid,
    const char *file_path,
    int chunk_size,
    int *p_out_written,
    char *err_buf,
    int err_buf_max);

#endif // SQLITE_CORE_FIELD_H