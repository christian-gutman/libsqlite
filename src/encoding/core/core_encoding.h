#ifndef CORE_ENCODING_H
#define CORE_ENCODING_H


// ============================================================================
// CONSTANTES DE ENCODING
// ============================================================================

#define ENCODING_UNKNOWN 0
#define ENCODING_ASCII 1
#define ENCODING_UTF8 2
#define ENCODING_ANSI 3
#define ENCODING_UTF16LE 4
#define ENCODING_UTF16BE 5

#ifdef __cplusplus
extern "C"
{
#endif

    #include "../../core.h"

    CORE_API int StrDetectBOM(const unsigned char *bytes, int len, int *startOffset);

    CORE_API int StrHasValidUtf8Sequence(const unsigned char *bytes, int len, int *charLen);

    CORE_API int  StrDetectEncodingLen(const char *str, int len, int *contentOffset);

    CORE_API int  StrDetectEncoding(const char *str);

    CORE_API wchar_t *StrAutoToUnicode(const char *inputStr, int len, int *outLen);

    CORE_API char  *StrAutoToAnsi(const char *inputStr, int len, int *outLen);

    CORE_API char  *StrAutoToUtf8(const char *inputStr, int len, int *outLen);

    CORE_API char  *UnicodeToAnsiC(const wchar_t *wstr, int len, int *outLen);

    CORE_API int  utf8_to_ansi(const char *utf8_src, int src_len, char *ansi_target, int max_target_len);
    CORE_API int  ansi_to_utf8(const char *ansi_src, int src_len, char *utf8_target, int max_target_len);

    // Helper para convertir wchar_t* a char*
    static char *WideToAnsi(const wchar_t *wstr)
    {
        if (!wstr)
            return NULL; // Protección contra nulos

        int size = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
        if (size <= 0)
            return NULL;

        char *str = (char *)malloc(size);
        if (!str)
            return NULL; // Protección si el sistema se queda sin RAM

        WideCharToMultiByte(CP_ACP, 0, wstr, -1, str, size, NULL, NULL);
        return str;
    }

    // Helper para convertir char* a wchar_t*
    static wchar_t *AnsiToWide(const char *str)
    {
        if (!str)
            return NULL; // Protección contra nulos

        int size = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
        if (size <= 0)
            return NULL;

        wchar_t *wstr = (wchar_t *)malloc(size * sizeof(wchar_t));
        if (!wstr)
            return NULL; // Protección si el sistema se queda sin RAM

        MultiByteToWideChar(CP_ACP, 0, str, -1, wstr, size);
        return wstr;
    }
    CORE_API char *UnicodeToUtf8(const wchar_t *wstr, int len, int *outLen);

#ifdef __cplusplus
}
#endif

#endif // CORE_ENCODING_H
