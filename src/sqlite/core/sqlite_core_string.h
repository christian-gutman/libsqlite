#ifndef SQLITE_CORE_STRING_H
#define SQLITE_CORE_STRING_H

#include "sqlite_core.h"
 
CORE_API char *SqlLite_Core_Format(const char *format, ...);
 
CORE_API char *SqlLite_Core_Concat(const char *str1, const char *str2);
 
CORE_API void SqlLite_Core_FreeString(const char *input);
  
CORE_API char *SqlLite_Core_AnsiToUTF8(const char *ansi_str);

CORE_API char *SqlLite_Core_UTF8ToAnsi(const char *utf8_str);

#endif // SQLITE_CORE_STRING_H
