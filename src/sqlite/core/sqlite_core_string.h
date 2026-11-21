#ifndef SQLITE_CORE_STRING_H
#define SQLITE_CORE_STRING_H

#include "sqlite_core.h"
 
SQLITE_CORE_API char *sqlite_core_string_format(const char *format, ...);
 
SQLITE_CORE_API char *sqlite_core_string_concat(const char *str1, const char *str2);
 
SQLITE_CORE_API void sqlite_core_string_free(const char *input);
  
SQLITE_CORE_API char *sqlite_core_string_ansi_to_utf8(const char *ansi_str);

SQLITE_CORE_API char *sqlite_core_string_utf8_to_ansi(const char *utf8_str);

#endif // SQLITE_CORE_STRING_H
