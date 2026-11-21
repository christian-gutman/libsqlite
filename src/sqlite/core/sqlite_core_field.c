#include "sqlite_core_field.h"
#include "sqlite_core_internal.h"

#include <stdio.h>
#include <string.h>

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

/* ------------------------------------------------------------------ */
/* Chunked writes: streaming a file into a BLOB column                */
/* ------------------------------------------------------------------ */

/* Default I/O chunk: large enough to amortize the write, small enough to
 * keep the stack/heap footprint constant regardless of the file size. */
#define SQLITE_CORE_BLOB_CHUNK_DEFAULT (64 * 1024)

/* Copies an engine error message into the caller buffer (bounded). */
static void CopyFieldError(char *err_buf, int err_buf_max, sqlite3 *db)
{
    if (err_buf == NULL || err_buf_max <= 0)
        return;
    CopyErrorMessage(err_buf, err_buf_max, sqlite3_errmsg(db));
}

/* Runs a zeroblob statement so the destination row/column exists and is
 * already the right size, then streams the file into it.
 *
 * Returns the number of bytes written (>= 0), or a negative value:
 *   SQLITE_MISUSE           invalid arguments
 *   SQLITE_CANTOPEN         the file could not be opened
 *   -3 (BLOB_TOO_SMALL)     the existing column blob is smaller than the file
 *   other SQLite            engine error (message copied to err_buf)
 */
static int StreamFileToBlob(
    sqlite3 *db,
    const char *table,
    const char *column,
    sqlite3_int64 rowid,
    FILE *file,
    sqlite3_int64 file_size,
    int chunk_size,
    sqlite3_int64 *out_written,
    char *err_buf,
    int err_buf_max)
{
    *out_written = 0;

    /* flags = 1 means read/write: 0 opens the handle read-only and every
     * sqlite3_blob_write would fail with SQLITE_READONLY. */
    sqlite3_blob *blob = NULL;
    int rc = sqlite3_blob_open(db, "main", table, column, rowid, 1, &blob);
    if (rc != SQLITE_OK)
    {
        CopyFieldError(err_buf, err_buf_max, db);
        return rc;
    }

    int existing = sqlite3_blob_bytes(blob);
    if (file_size > (sqlite3_int64)existing)
    {
        sqlite3_blob_close(blob);
        if (err_buf && err_buf_max > 0)
        {
            sqlite3_snprintf(err_buf_max, err_buf,
                             "column %s.%s holds %d bytes; the file holds %lld bytes",
                             table, column, existing, (long long)file_size);
        }
        return SQLITE_CORE_BLOB_TOO_SMALL;
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        sqlite3_blob_close(blob);
        return SQLITE_CANTOPEN;
    }

    char *buffer = (char *)sqlite3_malloc((size_t)chunk_size);
    if (buffer == NULL)
    {
        sqlite3_blob_close(blob);
        return -1;
    }

    sqlite3_int64 written = 0;
    rc = SQLITE_OK;
    while (written < file_size)
    {
        size_t want = (size_t)((file_size - written < (sqlite3_int64)chunk_size)
                                   ? (file_size - written)
                                   : (sqlite3_int64)chunk_size);
        size_t got = fread(buffer, 1, want, file);
        if (got == 0 && want > 0)
        {
            rc = SQLITE_CANTOPEN;
            break;
        }

        /* Offset stays inside int: a single blob write is capped by the
         * column size, which our API also limits to int (see get_blob). */
        rc = sqlite3_blob_write(blob, buffer, (int)got, (int)written);
        if (rc != SQLITE_OK)
        {
            CopyFieldError(err_buf, err_buf_max, db);
            break;
        }
        written += (sqlite3_int64)got;
    }

    sqlite3_free(buffer);
    sqlite3_blob_close(blob);

    *out_written = written;
    return rc;
}

/* Creates the destination row so the blob column can be opened for
 * writing: INSERT when rowid is 0, upsert (UPDATE first, INSERT with the
 * given rowid when the row does not exist) when rowid is non-zero. */
static int EnsureBlobRow(
    sqlite3 *db,
    const char *table,
    const char *column,
    sqlite3_int64 rowid,
    sqlite3_int64 size,
    sqlite3_int64 *out_rowid,
    char *err_buf,
    int err_buf_max)
{
    char *qtable = QuoteIdentifierSqlite(table);
    char *qcolumn = QuoteIdentifierSqlite(column);
    if (qtable == NULL || qcolumn == NULL)
    {
        sqlite3_free(qtable);
        sqlite3_free(qcolumn);
        return -1;
    }

    sqlite3_stmt *stmt = NULL;
    int must_free = 0;
    int rc = SQLITE_OK;

    if (rowid > 0)
    {
        char *upsert = sqlite3_mprintf("UPDATE %s SET %s = zeroblob(?) WHERE rowid = ?",
                                       qtable, qcolumn);
        rc = PrepareUtf8Sql(db, upsert, &stmt, &must_free);
        sqlite3_free(upsert);
        if (rc == SQLITE_OK)
        {
            sqlite3_bind_int64(stmt, 1, size);
            sqlite3_bind_int64(stmt, 2, rowid);
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_DONE)
                rc = SQLITE_OK;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
        must_free = 0;

        if (rc != SQLITE_OK)
        {
            CopyFieldError(err_buf, err_buf_max, db);
            sqlite3_free(qtable);
            sqlite3_free(qcolumn);
            return rc;
        }

        if (sqlite3_changes(db) == 0)
        {
            char *ins = sqlite3_mprintf("INSERT INTO %s(rowid, %s) VALUES (?, zeroblob(?))",
                                        qtable, qcolumn);
            rc = PrepareUtf8Sql(db, ins, &stmt, &must_free);
            sqlite3_free(ins);
            if (rc == SQLITE_OK)
            {
                sqlite3_bind_int64(stmt, 1, rowid);
                sqlite3_bind_int64(stmt, 2, size);
                rc = sqlite3_step(stmt);
                if (rc == SQLITE_DONE)
                    rc = SQLITE_OK;
            }
            sqlite3_finalize(stmt);
            if (rc != SQLITE_OK)
            {
                CopyFieldError(err_buf, err_buf_max, db);
                sqlite3_free(qtable);
                sqlite3_free(qcolumn);
                return rc;
            }
        }
        *out_rowid = rowid;
    }
    else
    {
        char *ins = sqlite3_mprintf("INSERT INTO %s(%s) VALUES (zeroblob(?))",
                                    qtable, qcolumn);
        rc = PrepareUtf8Sql(db, ins, &stmt, &must_free);
        sqlite3_free(ins);
        if (rc == SQLITE_OK)
        {
            sqlite3_bind_int64(stmt, 1, size);
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_DONE)
                rc = SQLITE_OK;
        }
        sqlite3_finalize(stmt);
        if (rc != SQLITE_OK)
        {
            CopyFieldError(err_buf, err_buf_max, db);
            sqlite3_free(qtable);
            sqlite3_free(qcolumn);
            return rc;
        }
        *out_rowid = sqlite3_last_insert_rowid(db);
    }

    sqlite3_free(qtable);
    sqlite3_free(qcolumn);
    return SQLITE_OK;
}

/* Shared validation, file opening and chunk defaulting for both entry
 * points. On success the caller owns *out_file (fclose it). */
static int OpenBlobSource(
    sqlite3 *db,
    const char *table,
    const char *column,
    const char *file_path,
    int chunk_size,
    char *err_buf,
    int err_buf_max,
    FILE **out_file,
    sqlite3_int64 *out_size,
    int *out_chunk)
{
    if (err_buf && err_buf_max > 0)
        err_buf[0] = '\0';

    if (db == NULL || table == NULL || column == NULL || file_path == NULL)
        return SQLITE_MISUSE;

    *out_file = NULL;
    *out_size = 0;
    *out_chunk = (chunk_size > 0) ? chunk_size : SQLITE_CORE_BLOB_CHUNK_DEFAULT;

    FILE *file = fopen(file_path, "rb");
    if (file == NULL)
    {
        if (err_buf && err_buf_max > 0)
            sqlite3_snprintf(err_buf_max, err_buf, "cannot open file: %s", file_path);
        return SQLITE_CANTOPEN;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return SQLITE_CANTOPEN;
    }
    long size = ftell(file);
    if (size < 0)
    {
        fclose(file);
        return SQLITE_CANTOPEN;
    }

    *out_file = file;
    *out_size = (sqlite3_int64)size;
    return SQLITE_OK;
}

/* Inserts (or upserts) a file into a BLOB column, streaming it in chunks.
 * rowid == 0 inserts a new row; rowid > 0 updates that row when it exists
 * and creates it with that rowid otherwise (upsert). */
int sqlite_core_insert_blob(
    sqlite3 *db,
    const char *table,
    const char *column,
    sqlite3_int64 rowid,
    const char *file_path,
    int chunk_size,
    int *p_out_written,
    char *err_buf,
    int err_buf_max)
{
    FILE *file = NULL;
    sqlite3_int64 file_size = 0;
    int chunk = SQLITE_CORE_BLOB_CHUNK_DEFAULT;

    int rc = OpenBlobSource(db, table, column, file_path, chunk_size,
                            err_buf, err_buf_max, &file, &file_size, &chunk);
    if (rc != SQLITE_OK)
        return rc;

    sqlite3_int64 target_rowid = 0;
    rc = EnsureBlobRow(db, table, column, rowid, file_size,
                       &target_rowid, err_buf, err_buf_max);
    if (rc != SQLITE_OK)
    {
        fclose(file);
        return rc;
    }

    sqlite3_int64 written = 0;
    rc = StreamFileToBlob(db, table, column, target_rowid, file, file_size,
                          chunk, &written, err_buf, err_buf_max);
    fclose(file);

    if (rc != SQLITE_OK)
    {
        if (p_out_written != NULL)
            *p_out_written = 0;
        return rc;
    }

    if (p_out_written != NULL)
        *p_out_written = (int)written;
    return (int)written;
}

/* Updates an existing row's BLOB column with the file contents, chunked.
 * The column blob is resized on the fly when the file is larger only via
 * the insert (upsert) path; here a too-small column fails with
 * SQLITE_CORE_BLOB_TOO_SMALL (use UPDATE with a bound parameter, or
 * insert_blob, to grow it). */
int sqlite_core_update_blob(
    sqlite3 *db,
    const char *table,
    const char *column,
    sqlite3_int64 rowid,
    const char *file_path,
    int chunk_size,
    int *p_out_written,
    char *err_buf,
    int err_buf_max)
{
    if (err_buf && err_buf_max > 0)
        err_buf[0] = '\0';

    if (rowid <= 0)
        return SQLITE_MISUSE;

    FILE *file = NULL;
    sqlite3_int64 file_size = 0;
    sqlite3_int64 written = 0;
    int chunk = SQLITE_CORE_BLOB_CHUNK_DEFAULT;

    int rc = OpenBlobSource(db, table, column, file_path, chunk_size,
                            err_buf, err_buf_max, &file, &file_size, &chunk);
    if (rc != SQLITE_OK)
        return rc;

    rc = StreamFileToBlob(db, table, column, rowid, file, file_size,
                          chunk, &written, err_buf, err_buf_max);
    fclose(file);

    if (rc != SQLITE_OK)
    {
        if (p_out_written != NULL)
            *p_out_written = 0;
        return rc;
    }

    if (p_out_written != NULL)
        *p_out_written = (int)written;
    return (int)written;
}