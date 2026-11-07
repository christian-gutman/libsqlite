#include "sqlite_core_string.h"

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

char *SqlLite_Core_Concat(const char *str1, const char *str2)
{
    // Si alguno es NULL, devolvemos una copia formateada del otro para evitar fallos
    if (str1 == NULL && str2 == NULL)
        return NULL;
    if (str1 == NULL)
        return sqlite3_mprintf("%s", str2);
    if (str2 == NULL)
        return sqlite3_mprintf("%s", str1);

    // Mprintf se encarga de medir ambos strings y unirlos en un nuevo buffer dinámico
    return sqlite3_mprintf("%s%s", str1, str2);
}

void SqlLite_Core_FreeString(const char *input)
{
    if (input != NULL)
    {
        sqlite3_free((void *)input);
    }
}
 
char *SqlLite_Core_AnsiToUTF8(const char *ansi_str)
{
    if (ansi_str == NULL)
    {
        return NULL;
    }

    // Usar la función de detección previa
    int encoding = StrDetectEncoding(ansi_str);
    if (encoding == ENCODING_ASCII || encoding == ENCODING_UTF8)
    {
        return sqlite3_mprintf("%s", ansi_str);
    }

#if defined(_WIN32) || defined(_WIN64)
    // 1. Calcular el tamaño necesario para UTF-16
    int wide_len = MultiByteToWideChar(CP_ACP, 0, ansi_str, -1, NULL, 0);
    if (wide_len <= 0)
    {
        return NULL;
    }

    // Reservar en el stack (se libera sola al salir de la función)
    wchar_t *wide_str = (wchar_t *)_alloca(wide_len * sizeof(wchar_t));

    // Convertir de ANSI a UTF-16
    if (MultiByteToWideChar(CP_ACP, 0, ansi_str, -1, wide_str, wide_len) <= 0)
    {
        return NULL;
    }

    // 2. Calcular el tamaño necesario para UTF-8 final
    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, NULL, 0, NULL, NULL);
    if (utf8_len <= 0)
    {
        return NULL;
    }

    // Reservar memoria en el HEAP usando SQLite
    char *utf8_str = (char *)sqlite3_malloc(utf8_len);
    if (utf8_str == NULL)
    {
        return NULL;
    }

    // Convertir de UTF-16 a UTF-8 final
    if (WideCharToMultiByte(CP_UTF8, 0, wide_str, -1, utf8_str, utf8_len, NULL, NULL) <= 0)
    {
        sqlite3_free(utf8_str);
        return NULL;
    }
    
    return utf8_str;
#else 
    // CORREGIDO: En Linux/Mac/Android, asumimos que el sistema ya opera en UTF-8,
    // o duplicamos la cadena original de forma segura con el formato de SQLite.
    return sqlite3_mprintf("%s", ansi_str);    
#endif
}


char *SqlLite_Core_UTF8ToAnsi(const char *utf8_str)
{
    if (utf8_str == NULL)
    {
        return NULL;
    }

    // Si el texto es ASCII puro, no requiere ninguna conversión (ANSI y UTF-8 son idénticos)
    int encoding = StrDetectEncoding(utf8_str);
    if (encoding == ENCODING_ASCII)
    {
        return sqlite3_mprintf("%s", utf8_str);
    }

#if defined(_WIN32) || defined(_WIN64)
    // 1. Calcular tamaño necesario para pasar de UTF-8 a UTF-16 (WideChar)
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, NULL, 0);
    if (wide_len <= 0)
        return sqlite3_mprintf("%s", utf8_str); // Fallback seguro si falla

    wchar_t *wide_str = (wchar_t *)_alloca(wide_len * sizeof(wchar_t));
    if (MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wide_str, wide_len) <= 0)
    {
        return sqlite3_mprintf("%s", utf8_str);
    }

    // 2. Calcular tamaño necesario para pasar de UTF-16 a ANSI (Windows-1252)
    int ansi_len = WideCharToMultiByte(CP_ACP, 0, wide_str, -1, NULL, 0, NULL, NULL);
    if (ansi_len <= 0)
        return sqlite3_mprintf("%s", utf8_str);

    char *ansi_str = (char *)sqlite3_malloc(ansi_len);
    if (ansi_str == NULL)
        return NULL;

    if (WideCharToMultiByte(CP_ACP, 0, wide_str, -1, ansi_str, ansi_len, NULL, NULL) <= 0)
    {
        sqlite3_free(ansi_str);
        return sqlite3_mprintf("%s", utf8_str);
    }

    return ansi_str; // CORREGIDO: Devolvemos la cadena convertida a ANSI
#else
    // CORREGIDO: En entornos no-Windows (Linux/Mac) el estándar nativo ya es UTF-8,
    // así que simplemente clonamos la cadena.
    return sqlite3_mprintf("%s", utf8_str);
#endif
}
