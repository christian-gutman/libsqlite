#ifndef SQLITE_COMPAT_H
#define SQLITE_COMPAT_H

/*
 * Portability layer absorbed from the former external dependencies
 * (core-c-sdk / libstring). Contains only the pieces libsqlite3 uses:
 *
 *   - SQLITE_CORE_API          symbol export macro (DLL / shared object visibility)
 *   - sqlite_core_str_compare       case-insensitive full string comparison
 *   - sqlite_core_str_n_compare     case-insensitive bounded string comparison
 *   - sqlite_core_detect_encoding encoding detection family (pure C, portable)
 *
 * Everything here is standard C99/C11 plus the narrowest possible platform
 * shims, so the same sources build on Windows, Linux and macOS.
 */

#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <malloc.h>

    #define sqlite_core_str_compare   _stricmp
    #define sqlite_core_str_n_compare _strnicmp
#else
    #include <strings.h>
    #include <unistd.h>

    #define sqlite_core_str_compare   strcasecmp
    #define sqlite_core_str_n_compare strncasecmp

    /* MSVC-only integer parsing alias (strtoull is C99) */
    #define _strtoui64 strtoull
#endif

/* Symbol visibility: __declspec(dllexport) on Windows,
 * visibility("default") on GCC/Clang 4+, nothing elsewhere. */
#ifndef SQLITE_CORE_API
    #if defined(_WIN32)
        #define SQLITE_CORE_API __declspec(dllexport)
    #elif defined(__GNUC__) && __GNUC__ >= 4
        #define SQLITE_CORE_API __attribute__((visibility("default")))
    #else
        #define SQLITE_CORE_API
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Encoding detection results. Kept from the absorbed implementation. */
#define ENCODING_UNKNOWN 0
#define ENCODING_ASCII   1
#define ENCODING_UTF8    2
#define ENCODING_ANSI    3  /* Windows-1252 / legacy single byte */
#define ENCODING_UTF16LE 4
#define ENCODING_UTF16BE 5

/* Validates one strict UTF-8 sequence.
 * Returns 1 and sets *charLen when bytes[0..len) starts a valid sequence. */
int sqlite_core_has_valid_utf8_sequence(const unsigned char *bytes, int len, int *charLen);

/* Detects a leading BOM (UTF-8 / UTF-16LE / UTF-16BE).
 * Returns the encoding and sets *startOffset to the BOM length (0 if none). */
int sqlite_core_detect_bom(const unsigned char *bytes, int len, int *startOffset);

/* Detects the encoding of a byte range; skips the BOM if present. */
int sqlite_core_detect_encoding_len(const char *str, int len, int *contentOffset);

/* Detects the encoding of a NUL-terminated string. */
int sqlite_core_detect_encoding(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* SQLITE_COMPAT_H */