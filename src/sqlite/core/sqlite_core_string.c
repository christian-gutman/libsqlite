#include "sqlite_core_string.h"

/*
 * String utilities. Returned buffers are allocated by the SQLite allocator
 * and must be released with SqlLite_Core_FreeString().
 */

/* printf-style formatter. Returns a heap string (free with FreeString). */
char *SqlLite_Core_Format(const char *format, ...)
{
    if (format == NULL)
    {
        return NULL;
    }
    va_list args;
    va_start(args, format);
    char *result = sqlite3_vmprintf(format, args);
    va_end(args);
    return result;
}

/* Concatenates two strings into a new heap buffer. NULL operands are
 * treated as empty strings. */
char *SqlLite_Core_Concat(const char *str1, const char *str2)
{
    if (str1 == NULL && str2 == NULL)
        return NULL;
    if (str1 == NULL)
        return sqlite3_mprintf("%s", str2);
    if (str2 == NULL)
        return sqlite3_mprintf("%s", str1);

    return sqlite3_mprintf("%s%s", str1, str2);
}

/* Releases any string returned by this library (SQLite allocator). */
void SqlLite_Core_FreeString(const char *input)
{
    if (input != NULL)
    {
        sqlite3_free((void *)input);
    }
}

/* Converts ANSI (Windows-1252 / active code page) text to UTF-8.
 * On non-Windows platforms input is assumed UTF-8 already and is copied. */
char *SqlLite_Core_AnsiToUTF8(const char *ansi_str)
{
    if (ansi_str == NULL)
    {
        return NULL;
    }

    /* Already ASCII or UTF-8: nothing to convert */
    int encoding = StrDetectEncoding(ansi_str);
    if (encoding == ENCODING_ASCII || encoding == ENCODING_UTF8)
    {
        return sqlite3_mprintf("%s", ansi_str);
    }

#if defined(_WIN32) || defined(_WIN64)
    /* 1. Measure the intermediate UTF-16 buffer */
    int wide_len = MultiByteToWideChar(CP_ACP, 0, ansi_str, -1, NULL, 0);
    if (wide_len <= 0)
    {
        return NULL;
    }

    /* 2. Heap-allocate the intermediate buffer (the old _alloca version
     *    could overflow the stack with long strings) */
    wchar_t *wide_str = (wchar_t *)sqlite3_malloc((size_t)wide_len * sizeof(wchar_t));
    if (wide_str == NULL)
    {
        return NULL;
    }

    if (MultiByteToWideChar(CP_ACP, 0, ansi_str, -1, wide_str, wide_len) <= 0)
    {
        sqlite3_free(wide_str);
        return NULL;
    }

    /* 3. Measure and produce the final UTF-8 buffer */
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, NULL, 0, NULL, NULL);
    if (utf8_len <= 0)
    {
        sqlite3_free(wide_str);
        return NULL;
    }

    char *utf8_str = (char *)sqlite3_malloc(utf8_len);
    if (utf8_str == NULL)
    {
        sqlite3_free(wide_str);
        return NULL;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, utf8_str, utf8_len, NULL, NULL) <= 0)
    {
        sqlite3_free(wide_str);
        sqlite3_free(utf8_str);
        return NULL;
    }

    sqlite3_free(wide_str);
    return utf8_str;
#else
    /* Linux/macOS are UTF-8 native: duplicate the string */
    return sqlite3_mprintf("%s", ansi_str);
#endif
}

/* Converts UTF-8 text to ANSI (Windows-1252 / active code page).
 * On non-Windows platforms the string is copied (UTF-8 native). */
char *SqlLite_Core_UTF8ToAnsi(const char *utf8_str)
{
    if (utf8_str == NULL)
    {
        return NULL;
    }

    /* Pure ASCII needs no conversion */
    int encoding = StrDetectEncoding(utf8_str);
    if (encoding == ENCODING_ASCII)
    {
        return sqlite3_mprintf("%s", utf8_str);
    }

#if defined(_WIN32) || defined(_WIN64)
    /* 1. UTF-8 -> UTF-16 */
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    if (wide_len <= 0)
        return sqlite3_mprintf("%s", utf8_str); /* safe fallback */

    /* 2. Heap-allocate the intermediate buffer (the old _alloca version
     *    could overflow the stack with long strings) */
    wchar_t *wide_str = (wchar_t *)sqlite3_malloc((size_t)wide_len * sizeof(wchar_t));
    if (wide_str == NULL)
        return NULL;

    if (MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wide_str, wide_len) <= 0)
    {
        sqlite3_free(wide_str);
        return sqlite3_mprintf("%s", utf8_str);
    }

    /* 3. UTF-16 -> ANSI */
    int ansi_len = WideCharToMultiByte(CP_ACP, 0, wide_str, -1, NULL, 0, NULL, NULL);
    if (ansi_len <= 0)
    {
        sqlite3_free(wide_str);
        return sqlite3_mprintf("%s", utf8_str);
    }

    char *ansi_str = (char *)sqlite3_malloc(ansi_len);
    if (ansi_str == NULL)
    {
        sqlite3_free(wide_str);
        return NULL;
    }

    if (WideCharToMultiByte(CP_ACP, 0, wide_str, -1, ansi_str, ansi_len, NULL, NULL) <= 0)
    {
        sqlite3_free(wide_str);
        sqlite3_free(ansi_str);
        return sqlite3_mprintf("%s", utf8_str);
    }

    sqlite3_free(wide_str);
    return ansi_str;
#else
    /* Linux/macOS are UTF-8 native: duplicate the string */
    return sqlite3_mprintf("%s", utf8_str);
#endif
}