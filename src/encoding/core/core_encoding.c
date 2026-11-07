#include "core_encoding.h"


// Valida si una secuencia de bytes es UTF-8 estricto y válido
int StrHasValidUtf8Sequence(const unsigned char *bytes, int len, int *charLen)
{
    unsigned char b0 = bytes[0];
    int expectedLen = 0;
    unsigned int codepoint = 0;

    if (b0 <= 0x7F)
    {
        // ASCII (1 byte)
        *charLen = 1;
        return TRUE;
    }
    else if ((b0 & 0xE0) == 0xC0)
    {
        expectedLen = 2;
        codepoint = b0 & 0x1F;
    }
    else if ((b0 & 0xF0) == 0xE0)
    {
        expectedLen = 3;
        codepoint = b0 & 0x0F;
    }
    else if ((b0 & 0xF8) == 0xF0)
    {
        expectedLen = 4;
        codepoint = b0 & 0x07;
    }
    else
    {
        // Byte de inicio inválido (10xxxxxx o 11111xxx)
        return 0;
    }

    if (len < expectedLen)
        return 0;

    // Validar bytes de continuación y construir codepoint
    for (int i = 1; i < expectedLen; i++)
    {
        if ((bytes[i] & 0xC0) != 0x80)
            return 0;
        codepoint = (codepoint << 6) | (bytes[i] & 0x3F);
    }

    // Rechazar overlong encodings
    if (expectedLen == 2 && codepoint < 0x80)
        return 0;
    if (expectedLen == 3 && codepoint < 0x800)
        return 0;
    if (expectedLen == 4 && codepoint < 0x10000)
        return 0;

    // Rechazar surrogates UTF-16 (U+D800 - U+DFFF)
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
        return 0;

    // Rechazar codepoints fuera del rango Unicode válido
    if (codepoint > 0x10FFFF)
        return 0;

    *charLen = expectedLen;
    return 1;
}

int StrDetectBOM(const unsigned char *bytes, int len, int *startOffset)
{
    if (!bytes || len <= 0)
    {
        if (startOffset)
            *startOffset = 0;
        return ENCODING_UNKNOWN;
    }

    // UTF-8 BOM: EF BB BF
    if (len >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
    {
        if (startOffset)
            *startOffset = 3;
        return ENCODING_UTF8;
    }
    // UTF-16LE BOM: FF FE
    if (len >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
    {
        if (startOffset)
            *startOffset = 2;
        return ENCODING_UTF16LE;
    }
    // UTF-16BE BOM: FE FF
    if (len >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF)
    {
        if (startOffset)
            *startOffset = 2;
        return ENCODING_UTF16BE;
    }

    if (startOffset)
        *startOffset = 0;
    return ENCODING_UNKNOWN;
}

 
int StrDetectEncodingLen(const char *str, int len, int *contentOffset)
{
    if (contentOffset)
        *contentOffset = 0;
    if (!str || len <= 0)
        return ENCODING_UNKNOWN;

    const unsigned char *bytes = (const unsigned char *)str;

    // 1. Detectar BOM
    int bomOffset = 0;
    int bomEnc = StrDetectBOM(bytes, len, &bomOffset);
    if (bomEnc != ENCODING_UNKNOWN)
    {
        if (contentOffset)
            *contentOffset = bomOffset;
        return bomEnc;
    }

    // 2. Heurística rápida para UTF-16 sin BOM (muchos bytes nulos intercalados)
    if (len >= 4)
    {
        int nullEven = 0, nullOdd = 0;
        int sampleLen = (len > 200) ? 200 : len;
        for (int i = 0; i < sampleLen - 1; i += 2)
        {
            if (bytes[i] == 0x00)
                nullEven++;
            if (bytes[i + 1] == 0x00)
                nullOdd++;
        }
        int half = sampleLen / 2;
        if (nullOdd > half / 2 && nullEven < half / 4)
            return ENCODING_UTF16LE;
        if (nullEven > half / 2 && nullOdd < half / 4)
            return ENCODING_UTF16BE;
    }

    // 3. Validar UTF-8 estricto byte a byte
    BOOL hasHighByte = FALSE;
    int i = 0;
    while (i < len)
    {
        if (bytes[i] == 0x00)
        {
            i++;
            continue;
        }

        int charLen = 0;
        if (!StrHasValidUtf8Sequence(bytes + i, len - i, &charLen))
        {
            // Byte inválido para UTF-8 -> ANSI
            return ENCODING_ANSI;
        }

        if (bytes[i] > 0x7F)
            hasHighByte = TRUE;
        i += charLen;
    }

    return hasHighByte ? ENCODING_UTF8 : ENCODING_ASCII;
}

// Versión legacy (string terminado en \0) - ya no hay ambigüedad de nombre
int StrDetectEncoding(const char *str)
{
    if (!str)
        return ENCODING_UNKNOWN;
    int len = (int)strlen(str);
    return StrDetectEncodingLen(str, len, NULL);
}







// Convierte cualquier encoding a Unicode (wchar_t*)
wchar_t *StrAutoToUnicode(const char *inputStr, int len, int *outLen)
{
    if (outLen)
        *outLen = 0;
    if (!inputStr)
        return NULL;
    if (len <= 0)
        len = (int)strlen(inputStr);
    if (len <= 0)
        return NULL;

    int contentOffset = 0;
    int enc = StrDetectEncodingLen(inputStr, len, &contentOffset);

    const char *src = inputStr + contentOffset;
    int srcLen = len - contentOffset;

    switch (enc)
    {
    case ENCODING_UTF16LE:
    {
        // Ya es UTF-16LE, solo copiar. Protegemos contra srcLen impar.
        int alignedLen = srcLen - (srcLen % 2);
        wchar_t *result = (wchar_t *)malloc(alignedLen + sizeof(wchar_t));
        if (!result)
            return NULL;
        memcpy(result, src, alignedLen);
        result[alignedLen / sizeof(wchar_t)] = L'\0';
        if (outLen)
            *outLen = alignedLen / sizeof(wchar_t);
        return result;
    }

    case ENCODING_UTF16BE:
    {
        // UTF-16BE -> swap bytes -> UTF-16LE. Protegemos contra srcLen impar.
        int alignedLen = srcLen - (srcLen % 2);
        wchar_t *result = (wchar_t *)malloc(alignedLen + sizeof(wchar_t));
        if (!result)
            return NULL;
        for (int i = 0; i < alignedLen; i += 2)
        {
            ((unsigned char *)result)[i] = ((unsigned char *)src)[i + 1];
            ((unsigned char *)result)[i + 1] = ((unsigned char *)src)[i];
        }
        result[alignedLen / sizeof(wchar_t)] = L'\0';
        if (outLen)
            *outLen = alignedLen / sizeof(wchar_t);
        return result;
    }

    case ENCODING_UTF8:
    {
        // UTF-8 -> UTF-16LE
        int wLen = MultiByteToWideChar(CP_UTF8, 0, src, srcLen, NULL, 0);
        if (wLen <= 0)
            return NULL;

        wchar_t *result = (wchar_t *)malloc((wLen + 1) * sizeof(wchar_t));
        if (!result)
            return NULL;

        MultiByteToWideChar(CP_UTF8, 0, src, srcLen, result, wLen);
        result[wLen] = L'\0';
        if (outLen)
            *outLen = wLen;
        return result;
    }

    case ENCODING_ASCII:
    case ENCODING_ANSI:
    {
        // ANSI -> UTF-16LE
        int wLen = MultiByteToWideChar(CP_THREAD_ACP, 0, src, srcLen, NULL, 0);
        if (wLen <= 0)
            return NULL;

        wchar_t *result = (wchar_t *)malloc((wLen + 1) * sizeof(wchar_t));
        if (!result)
            return NULL;

        MultiByteToWideChar(CP_THREAD_ACP, 0, src, srcLen, result, wLen);
        result[wLen] = L'\0';
        if (outLen)
            *outLen = wLen;
        return result;
    }

    default:
        return NULL;
    }
}

char *UnicodeToUtf8(const wchar_t *wstr, int len, int *outLen) {
    if (!wstr) return NULL;

    if (len < 0) {
        len = (int)wcslen(wstr);
    }

    if (len == 0) {
        char *emptyStr = (char *)calloc(1, 1);
        if (outLen) *outLen = 0;
        return emptyStr;
    }
 
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, len, NULL, 0, NULL, NULL);
    if (size <= 0) return NULL;

    char *str = (char *)malloc(size + 1);
    if (str) {
        WideCharToMultiByte(CP_UTF8, 0, wstr, len, str, size, NULL, NULL);
        str[size] = '\0';
        if (outLen) *outLen = size;
    }
    return str;
}
char *UnicodeToAnsiC(const wchar_t *wstr, int len, int *outLen)
{
    if (!wstr) 
        return NULL;

    // 1. Si len es -1, calculamos automáticamente la longitud de la cadena ancha
    if (len < 0) {
        len = (int)wcslen(wstr);
    }

    // Si la cadena está vacía tras la validación, devolvemos un string vacío alocado
    if (len == 0) {
        char *emptyStr = (char *)calloc(1, 1);
        if (outLen) *outLen = 0;
        return emptyStr;
    }

    // 2. Descubrir el tamaño exacto requerido en BYTES (Excluyendo el terminador nulo)
    int size = WideCharToMultiByte(CP_THREAD_ACP, 0, wstr, len, NULL, 0, NULL, NULL);
    if (size <= 0) 
        return NULL;

    // 3. Alocar memoria exacta en el Heap para los caracteres más el espacio del '\0'
    char *str = (char *)malloc(size + 1);
    if (str)
    {
        // 4. Realizar la conversión física unmanaged
        WideCharToMultiByte(CP_THREAD_ACP, 0, wstr, len, str, size, NULL, NULL);
        
        // Garantizar de forma manual y segura la terminación nula en el índice exacto
        str[size] = '\0';
        
        // Devolver el tamaño neto de la cadena ANSI resultante para control de FoxPro
        if (outLen)
            *outLen = size;
    }
    return str;
}

char *StrAutoToAnsi(const char *inputStr, int len, int *outLen)
{
    if (len <= 0)
        len = (int)strlen(inputStr);
    if (len <= 0)
        return NULL;

    // Atajo rápido: ASCII puro no necesita ninguna conversión
    if (StrDetectEncoding(inputStr) == ENCODING_ASCII)
    {
        char *result = (char *)malloc((size_t)len + 1);
        if (result)
        {
            memcpy(result, inputStr, (size_t)len);
            result[len] = '\0';
            if (outLen)
                *outLen = len;
        }
        return result;
    }

    // 1. DETECCIÓN FINA: ¿es UTF-8 válido según Windows?
    int isUtf8 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, inputStr, len, NULL, 0);

    if (isUtf8 <= 0)
    {
        // No es UTF-8 válido -> ya está en ANSI. Duplicamos tal cual.
        char *result = (char *)malloc((size_t)len + 1);
        if (result)
        {
            memcpy(result, inputStr, (size_t)len);
            result[len] = '\0';
            if (outLen)
                *outLen = len;
        }
        return result;
    }

    // 2. UTF-8 -> UTF-16 -> ANSI
    wchar_t *wstr = (wchar_t *)malloc((size_t)isUtf8 * sizeof(wchar_t));
    if (!wstr)
        return NULL;

    MultiByteToWideChar(CP_UTF8, 0, inputStr, len, wstr, isUtf8);

    int ansiSize = WideCharToMultiByte(CP_THREAD_ACP, 0, wstr, isUtf8, NULL, 0, NULL, NULL);
    char *ansiStr = (char *)malloc((size_t)ansiSize + 1);

    if (ansiStr)
    {
        WideCharToMultiByte(CP_THREAD_ACP, 0, wstr, isUtf8, ansiStr, ansiSize, NULL, NULL);
        ansiStr[ansiSize] = '\0';
        if (outLen)
            *outLen = ansiSize;
    }

    free(wstr);
    return ansiStr;
}

// CORREGIDO: mismo criterio que AutoToAnsi -> atajo para ASCII puro.
char *StrAutoToUtf8(const char *inputStr, int len, int *outLen)
{
    if (len <= 0)
        len = (int)strlen(inputStr);
    if (len <= 0)
        return NULL;

    // Atajo rápido: ASCII puro ya es UTF-8 válido, no hace falta convertir
    if (StrDetectEncoding(inputStr) == ENCODING_ASCII)
    {
        char *result = (char *)malloc((size_t)len + 1);
        if (result)
        {
            memcpy(result, inputStr, (size_t)len);
            result[len] = '\0';
            if (outLen)
                *outLen = len;
        }
        return result;
    }

    // 1. DETECCIÓN FINA: intentar interpretar como UTF-8 válido
    int isUtf8 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, inputStr, len, NULL, 0);

    if (isUtf8 > 0)
    {
        // Ya está en UTF-8. Duplicamos tal cual.
        char *result = (char *)malloc((size_t)len + 1);
        if (result)
        {
            memcpy(result, inputStr, (size_t)len);
            result[len] = '\0';
            if (outLen)
                *outLen = len;
        }
        return result;
    }

    // 2. ANSI -> UTF-16 -> UTF-8
    int wSize = MultiByteToWideChar(CP_THREAD_ACP, 0, inputStr, len, NULL, 0);
    if (wSize <= 0)
        return NULL;

    wchar_t *wstr = (wchar_t *)malloc((size_t)wSize * sizeof(wchar_t));
    if (!wstr)
        return NULL;

    MultiByteToWideChar(CP_THREAD_ACP, 0, inputStr, len, wstr, wSize);

    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wstr, wSize, NULL, 0, NULL, NULL);
    char *utf8Str = (char *)malloc((size_t)utf8Size + 1);

    if (utf8Str)
    {
        WideCharToMultiByte(CP_UTF8, 0, wstr, wSize, utf8Str, utf8Size, NULL, NULL);
        utf8Str[utf8Size] = '\0';
        if (outLen)
            *outLen = utf8Size;
    }

    free(wstr);
    return utf8Str;
}

const int code_page = 1252; // CP_ACP

int utf8_to_ansi(const char *utf8_src, int src_len, char *ansi_target, int max_target_len)
{
    if (utf8_src == NULL || ansi_target == NULL || src_len <= 0 || max_target_len <= 0)
    {
        if (ansi_target != NULL && max_target_len > 0)
            *ansi_target = '\0';
        return 0;
    }

    // 1. Convertimos primero de UTF-8 a WideChar (UTF-16) intermedio
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, utf8_src, src_len, NULL, 0);
    if (wide_len <= 0)
    {
        *ansi_target = '\0';
        return 0;
    }

    wchar_t *wide_buf = (wchar_t *)malloc(wide_len * sizeof(wchar_t));
    if (wide_buf == NULL)
    {
        *ansi_target = '\0';
        return 0;
    }

    MultiByteToWideChar(CP_UTF8, 0, utf8_src, src_len, wide_buf, wide_len);

    // 2. Convertimos del WideChar intermedio a ANSI (CP_ACP = Code Page por defecto del sistema)
    int ansi_len = WideCharToMultiByte(code_page, 0, wide_buf, wide_len, NULL, 0, NULL, NULL);

    // Validamos que no supere el buffer máximo otorgado por FoxPro
    if (ansi_len >= max_target_len)
    {
        ansi_len = max_target_len - 1;
    }

    WideCharToMultiByte(code_page, 0, wide_buf, wide_len, ansi_target, ansi_len, NULL, NULL);
    ansi_target[ansi_len] = '\0';

    free(wide_buf);
    return ansi_len;
}

int ansi_to_utf8(const char *ansi_src, int src_len, char *utf8_target, int max_target_len)
{
    if (ansi_src == NULL || utf8_target == NULL || src_len <= 0 || max_target_len <= 0)
    {
        if (utf8_target != NULL && max_target_len > 0)
            *utf8_target = '\0';
        return 0;
    }

    // 1. Convertimos de ANSI a WideChar intermedio
    int wide_len = MultiByteToWideChar(code_page, 0, ansi_src, src_len, NULL, 0);
    if (wide_len <= 0)
    {
        *utf8_target = '\0';
        return 0;
    }

    wchar_t *wide_buf = (wchar_t *)malloc(wide_len * sizeof(wchar_t));
    if (wide_buf == NULL)
    {
        *utf8_target = '\0';
        return 0;
    }

    MultiByteToWideChar(code_page, 0, ansi_src, src_len, wide_buf, wide_len);

    // 2. Convertimos de WideChar a UTF-8 final
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide_buf, wide_len, NULL, 0, NULL, NULL);

    if (utf8_len >= max_target_len)
    {
        utf8_len = max_target_len - 1;
    }

    WideCharToMultiByte(CP_UTF8, 0, wide_buf, wide_len, utf8_target, utf8_len, NULL, NULL);
    utf8_target[utf8_len] = '\0';

    free(wide_buf);
    return utf8_len;
}

