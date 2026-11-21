#include "sqlite_compat.h"

/*
 * Encoding detection, absorbed from the former external libstring dependency.
 * Pure C: no Windows API calls, works on every supported platform.
 */

/* Validates if the byte range starts with a strict, well-formed UTF-8
 * sequence (no overlong encodings, no surrogates, no out-of-range code
 * points). Returns 1 and stores the sequence length in *charLen. */
int sqlite_core_has_valid_utf8_sequence(const unsigned char *bytes, int len, int *charLen)
{
    unsigned char b0 = bytes[0];
    int expectedLen = 0;
    unsigned int codepoint = 0;

    if (b0 <= 0x7F)
    {
        /* ASCII (1 byte) */
        *charLen = 1;
        return 1;
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
        /* Invalid lead byte (10xxxxxx continuation or 11111xxx) */
        return 0;
    }

    if (len < expectedLen)
        return 0;

    /* Validate continuation bytes and assemble the code point */
    for (int i = 1; i < expectedLen; i++)
    {
        if ((bytes[i] & 0xC0) != 0x80)
            return 0;
        codepoint = (codepoint << 6) | (bytes[i] & 0x3F);
    }

    /* Reject overlong encodings */
    if (expectedLen == 2 && codepoint < 0x80)
        return 0;
    if (expectedLen == 3 && codepoint < 0x800)
        return 0;
    if (expectedLen == 4 && codepoint < 0x10000)
        return 0;

    /* Reject UTF-16 surrogates (U+D800 - U+DFFF) */
    if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
        return 0;

    /* Reject code points outside the valid Unicode range */
    if (codepoint > 0x10FFFF)
        return 0;

    *charLen = expectedLen;
    return 1;
}

int sqlite_core_detect_bom(const unsigned char *bytes, int len, int *startOffset)
{
    if (startOffset)
        *startOffset = 0;

    if (!bytes || len <= 0)
        return ENCODING_UNKNOWN;

    /* UTF-8 BOM: EF BB BF */
    if (len >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
    {
        if (startOffset)
            *startOffset = 3;
        return ENCODING_UTF8;
    }
    /* UTF-16LE BOM: FF FE */
    if (len >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE)
    {
        if (startOffset)
            *startOffset = 2;
        return ENCODING_UTF16LE;
    }
    /* UTF-16BE BOM: FE FF */
    if (len >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF)
    {
        if (startOffset)
            *startOffset = 2;
        return ENCODING_UTF16BE;
    }

    return ENCODING_UNKNOWN;
}

int sqlite_core_detect_encoding_len(const char *str, int len, int *contentOffset)
{
    if (contentOffset)
        *contentOffset = 0;

    if (!str || len <= 0)
        return ENCODING_UNKNOWN;

    const unsigned char *bytes = (const unsigned char *)str;

    /* 1. BOM detection */
    int bomOffset = 0;
    int bomEnc = sqlite_core_detect_bom(bytes, len, &bomOffset);
    if (bomEnc != ENCODING_UNKNOWN)
    {
        if (contentOffset)
            *contentOffset = bomOffset;
        return bomEnc;
    }

    /* 2. BOM-less UTF-16 heuristic: many interleaved NUL bytes */
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

    /* 3. Strict UTF-8 validation byte by byte */
    int hasHighByte = 0;
    int i = 0;
    while (i < len)
    {
        if (bytes[i] == 0x00)
        {
            i++;
            continue;
        }

        int charLen = 0;
        if (!sqlite_core_has_valid_utf8_sequence(bytes + i, len - i, &charLen))
        {
            /* Byte invalid for UTF-8 -> assume ANSI */
            return ENCODING_ANSI;
        }

        if (bytes[i] > 0x7F)
            hasHighByte = 1;
        i += charLen;
    }

    return hasHighByte ? ENCODING_UTF8 : ENCODING_ASCII;
}

int sqlite_core_detect_encoding(const char *str)
{
    if (!str)
        return ENCODING_UNKNOWN;
    int len = (int)strlen(str);
    return sqlite_core_detect_encoding_len(str, len, NULL);
}