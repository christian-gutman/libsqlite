#include "sqlite_core_parameter.h"

const char *sqlite_core_param_clean_prefix(const char *name)
{
    if (name == NULL)
    {
        return NULL;
    }
    if (strlen(name) > 1)
    {
         if (name[0] == ':' || name[0] == '$' || name[0] == '@' || name[0] == '?')
        {
            return name + 1;
        }
    } 
    return name;
}

void sqlite_core_param_bind(
    sqlite3_stmt *stmt,
    sqlite_core_param_t *params,
    int paramCount)
{ 
    if (stmt == NULL || params == NULL || paramCount <= 0)
    {
        return;
    }

    int total_stmt_params = sqlite3_bind_parameter_count(stmt);

    for (int i = 1; i <= total_stmt_params; i++)
    {
        const char *name = sqlite3_bind_parameter_name(stmt, i);

        if (name == NULL)
            continue;

        const char *key_name = sqlite_core_param_clean_prefix(name);

        for (int j = 0; j < paramCount; j++)
        {
            if (params[j].name == NULL)
                continue;

            if (params[j].Direction == SQLITE_CORE_DIRECTION_OUTPUT)
                continue;

            int is_match = 0;

            if (params[j].no_insensitive == 1)
            {
                is_match = (strcmp(name, params[j].name) == 0 || strcmp(key_name, params[j].name) == 0);
            }
            else
            {
                is_match = (sqlite_core_str_compare(key_name, params[j].name) == 0 || sqlite_core_str_compare(name, params[j].name) == 0);
            }

            if (is_match)
            {
                switch (params[j].type)
                {
                case SQLITE_CORE_TYPE_NULL:
                    sqlite3_bind_null(stmt, i);
                    break;

                case SQLITE_CORE_TYPE_DOUBLE:
                    sqlite3_bind_double(stmt, i, params[j].value.dVal);
                    break;

                case SQLITE_CORE_TYPE_INT:
                    sqlite3_bind_int64(stmt, i, params[j].value.iVal);
                    break;

                case SQLITE_CORE_TYPE_TEXT:
                {
                    const char *pSrcText = params[j].value.blob_or_text.ptr;
                    int len_to_bind = params[j].value.blob_or_text.len;
                    int must_free = 0;

                    if (pSrcText != NULL)
                    {
                        if (sqlite_core_detect_encoding(pSrcText) == ENCODING_ANSI)
                        {
                            char *converted_text = sqlite_core_string_ansi_to_utf8(pSrcText);
                            if (converted_text != NULL)
                            {
                                pSrcText = converted_text;
                                len_to_bind = -1;
                                must_free = 1;
                            }
                        }
                    }
                    sqlite3_bind_text(stmt, i, pSrcText, len_to_bind, SQLITE_TRANSIENT);

                    if (must_free)
                    {
                        sqlite_core_string_free((void *)pSrcText);
                    }
                    break;
                }

                case SQLITE_CORE_TYPE_BLOB:
                    sqlite3_bind_blob(stmt, i, params[j].value.blob_or_text.ptr, params[j].value.blob_or_text.len, SQLITE_TRANSIENT);
                    break;
                }
                break;
            }
        }
    }
}
 
void sqlite_core_param_process_outputs(
    sqlite3_stmt *stmt,
    sqlite_core_param_t *params,
    int paramCount)
{
  
    if (stmt == NULL || params == NULL || paramCount <= 0)
    {
        return;
    }
 
    int cols = sqlite3_column_count(stmt);

  
    for (int j = 0; j < paramCount; j++)
    {
        if (params[j].Direction != SQLITE_CORE_DIRECTION_OUTPUT &&
            params[j].Direction != SQLITE_CORE_DIRECTION_INPUTOUTPUT)
        {
            continue;
        }

        if (params[j].name == NULL)
            continue;

        const char *key_name = sqlite_core_param_clean_prefix(params[j].name);

        for (int c = 0; c < cols; c++)
        {
            const char *col_name = sqlite3_column_name(stmt, c);
            if (col_name == NULL)
                continue;

            int is_match = 0;

            if (params[j].no_insensitive == 1)
            {
                is_match = (strcmp(col_name, params[j].name) == 0 || strcmp(col_name, key_name) == 0);
            }
            else
            {
                is_match = (sqlite_core_str_compare(col_name, key_name) == 0 || sqlite_core_str_compare(col_name, params[j].name) == 0);
            }

            if (is_match)
            {
                int type = sqlite3_column_type(stmt, c);
                switch (type)
                {
                case SQLITE_INTEGER:
                    params[j].type = SQLITE_CORE_TYPE_INT;
                    params[j].value.iVal = sqlite3_column_int64(stmt, c);
                    break;

                case SQLITE_FLOAT:
                    params[j].type = SQLITE_CORE_TYPE_DOUBLE;
                    params[j].value.dVal = sqlite3_column_double(stmt, c);
                    break;

                case SQLITE_TEXT:
                {
                    const char *txt = (const char *)sqlite3_column_text(stmt, c);
                    int len = sqlite3_column_bytes(stmt, c);
                    params[j].type = SQLITE_CORE_TYPE_TEXT;

                    const char *pSrcText = txt;
                    int must_free = 0;

                    if (txt != NULL)
                    {
                        if (sqlite_core_detect_encoding(txt) == ENCODING_UTF8)
                        {
                            char *converted_ansi = sqlite_core_string_utf8_to_ansi(txt);
                            if (converted_ansi != NULL)
                            {
                                pSrcText = converted_ansi;
                                len = (int)strlen(converted_ansi); 
                                must_free = 1;
                            }
                        }

                        if (params[j].value.blob_or_text.ptr &&
                            params[j].value.blob_or_text.max_len > len)
                        {
                            memcpy(params[j].value.blob_or_text.ptr, pSrcText, len);
                            params[j].value.blob_or_text.ptr[len] = '\0'; 
                            params[j].value.blob_or_text.len = len;
                        } 
                        if (must_free)
                        {
                            sqlite_core_string_free((void *)pSrcText);
                        }
                    }
                    break;
                }

                case SQLITE_NULL:
                    params[j].type = SQLITE_CORE_TYPE_NULL;
                    break;
                }
                break;
            }
        }
    }
}
