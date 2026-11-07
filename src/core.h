#ifndef CORE_H
#define CORE_H
// #pragma once

#include <stddef.h>
#include <wchar.h>
#include <limits.h>
#include <malloc.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <malloc.h>
#define str_compare _stricmp
#define str_n_compare _strnicmp
#else
#include <strings.h>
#include <unistd.h>
#define str_compare strcasecmp
#define str_n_compare strncasecmp
#endif


#ifndef CORE_API
#if defined(_WIN32) || defined(CORE_BUILD_DLL) || defined(__CYGWIN__)
#define CORE_API __declspec(dllexport)
#else
#if __GNUC__ >= 4
#define CORE_API __attribute__((visibility("default")))
#else
#define CORE_API
#endif
#endif
#endif

#if defined(VFP_FLL)
#define CORE_CALL __fastcall
#else
#define CORE_CALL __stdcall
#endif

#define CORE_EXPORT CORE_API CORE_CALL

// 5. FUNCIONES AUXILIARES (Definidas después de tener size_t)
inline static char *strcpyn(char *des, const char *src, size_t n)
{
    if (n)
        strncpy(des, src, n - 1)[n - 1] = 0;
    return des;
}

#define strncpy(des, src, n) strcpyn(des, src, n)

#define CORE_VERSION_MAJOR 3
#define CORE_VERSION_MINOR 54
#define CORE_VERSION_PATCH 1
#define CORE_REVISION 0
#define CORE_SUBREVISION 0
#define CORE_VERSION_STR "3.54.1.0" // 3.54.1
#define CORE_VERNUM 0x35410

#ifdef __cplusplus
extern "C"
{
#endif

    void Core_InternalInit(void);
    void Core_InternalExit(void);

    const char *Core_Version();

#ifdef __cplusplus
}
#endif

#endif // CORE_H
