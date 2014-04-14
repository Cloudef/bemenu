#include "internal.h"

#define _XOPEN_SOURCE
#include <wchar.h> /* wcswidth */
#undef _XOPEN_SOURCE

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

/**
 * Portable strdup.
 *
 * @param string C "string" to copy.
 * @return Copy of the given C "string".
 */
char* _bmStrdup(const char *string)
{
    assert(string);

    size_t len = strlen(string);
    if (len == 0)
        return NULL;

    void *copy = calloc(1, len + 1);
    if (copy == NULL)
        return NULL;

    return (char *)memcpy(copy, string, len);
}

/**
 * Replaces next token in string with '\0' and returns position for the replaced token.
 *
 * @param string C "string" where token will be replaced.
 * @param outNext Reference to position of next delimiter, or 0 if none.
 * @return Position of the replaced token.
 */
size_t _bmStripToken(char *string, const char *token, size_t *outNext)
{
    size_t len = strcspn(string, token);

    if (outNext)
        *outNext = len + (string[len] != 0);

    string[len] = 0;
    return len;
}

/**
 * Portable case-insensitive strcmp.
 *
 * @param hay C "string" to match against.
 * @param needle C "string" to match.
 * @return Less than, equal to or greater than zero if hay is lexicographically less than, equal to or greater than needle.
 */
int _bmStrupcmp(const char *hay, const char *needle)
{
    size_t len, len2;

    if ((len = strlen(hay)) != (len2 = strlen(needle)))
        return hay[len] - needle[len2];

    return _bmStrnupcmp(hay, needle, len);
}

/**
 * Portable case-insensitive strncmp.
 *
 * @param hay C "string" to match against.
 * @param needle C "string" to match.
 * @return Less than, equal to or greater than zero if hay is lexicographically less than, equal to or greater than needle.
 */
int _bmStrnupcmp(const char *hay, const char *needle, size_t len)
{
    size_t i = 0;
    unsigned char a = 0, b = 0;

    const unsigned char *p1 = (const unsigned char*)hay;
    const unsigned char *p2 = (const unsigned char*)needle;

    for (i = 0; len > 0; --len, ++i)
        if ((a = toupper(*p1++)) != (b = toupper(*p2++)))
            return a - b;

    return a - b;
}

/**
 * Portable case-insensitive strstr.
 *
 * @param hay C "string" to substring against.
 * @param needle C "string" to substring.
 */
char* _bmStrupstr(const char *hay, const char *needle)
{
    size_t i, r = 0, p = 0, len, len2;

    if ((len = strlen(hay)) < (len2 = strlen(needle)))
        return NULL;

    if (!_bmStrnupcmp(hay, needle, len2))
        return (char*)hay;

    for (i = 0; i < len; ++i) {
        if (p == len2)
            return (char*)hay + r;

        if (toupper(hay[i]) == toupper(needle[p++])) {
            if (!r)
                r = i;
        } else {
            if (r)
                i = r;
            r = p = 0;
        }
    }

    return (p == len2 ? (char*)hay + r : NULL);
}

/**
 * Determite columns needed to display UTF8 string.
 *
 * @param string C "string" to determite.
 * @return Number of columns, or -1 on failure.
 */
int _bmUtf8StringScreenWidth(const char *string)
{
    assert(string);

    char *mstr = _bmStrdup(string);
    if (!mstr)
        return strlen(string);

    char *s;
    for (s = mstr; *s; ++s) if (*s == '\t') *s = ' ';

    int num_char = mbstowcs(NULL, mstr, 0) + 1;
    wchar_t *wstring = malloc((num_char + 1) * sizeof (wstring[0]));

    if (mbstowcs(wstring, mstr, num_char) == (size_t)(-1)) {
        free(wstring);
        int len = strlen(mstr);
        free(mstr);
        return len;
    }

    int length = wcswidth(wstring, num_char);
    free(wstring);
    free(mstr);
    return length;
}

/**
 * Figure out how many bytes to shift to next UTF8 rune.
 *
 * @param string C "string" which contains the runes.
 * @param start Offset where to figure out next rune. (cursor)
 * @return Number of bytes to next UTF8 rune.
 */
size_t _bmUtf8RuneNext(const char *string, size_t start)
{
    assert(string);

    size_t len = strlen(string), i = start;
    if (len == 0 || len <= i || !*string)
        return 0;

    while (++i < len && (string[i] & 0xc0) == 0x80);
    return i - start;
}

/**
 * Figure out how many bytes to shift to previous UTF8 rune.
 *
 * @param string C "string" which contains the runes.
 * @param start Offset where to figure out previous rune. (cursor)
 * @return Number of bytes to previous UTF8 rune.
 */
size_t _bmUtf8RunePrev(const char *string, size_t start)
{
    assert(string);

    size_t len = strlen(string), i = start;
    if (i == 0 || len < start || !*string)
        return 0;

    while (--i > 0 && (string[i] & 0xc0) == 0x80);
    return start - i;
}

/**
 * Figure out how many columns are needed to display UTF8 rune.
 *
 * @param rune Buffer which contains the rune.
 * @param u8len Byte length of the rune.
 * @return Number of columns, or -1 on failure.
 */
size_t _bmUtf8RuneWidth(const char *rune, unsigned int u8len)
{
    assert(rune);
    char mb[5] = { 0, 0, 0, 0, 0 };
    memcpy(mb, rune, (u8len > 4 ? 4 : u8len));
    return _bmUtf8StringScreenWidth(mb);
}

/**
 * Remove previous UTF8 rune from buffer.
 *
 * @param string Null terminated C "string".
 * @param start Start offset where to delete from. (cursor)
 * @param outRuneWidth Reference to size_t, return number of columns for removed rune, or -1 on failure.
 * @return Number of bytes removed from buffer.
 */
size_t _bmUtf8RuneRemove(char *string, size_t start, size_t *outRuneWidth)
{
    assert(string);

    if (outRuneWidth)
        *outRuneWidth = 0;

    size_t len = strlen(string), oldStart = start;
    if (len == 0 || len < start || !*string)
        return 0;

    start -= _bmUtf8RunePrev(string, start);

    if (outRuneWidth)
        *outRuneWidth = _bmUtf8RuneWidth(string + start, oldStart - start);

    memmove(string + start, string + oldStart, len - oldStart);
    string[len - (oldStart - start)] = 0;
    return (oldStart - start);
}

/**
 * Insert UTF8 rune to buffer.
 *
 * @param inOutString Reference to buffer.
 * @param inOutBufSize Reference to size of the buffer.
 * @param start Start offset where to insert to. (cursor)
 * @param rune Buffer to insert to string.
 * @param u8len Byte length of the rune.
 * @param outRuneWidth Reference to size_t, return number of columns for inserted rune, or -1 on failure.
 * @return Number of bytes inserted to buffer.
 */
size_t _bmUtf8RuneInsert(char **inOutString, size_t *inOutBufSize, size_t start, const char *rune, unsigned int u8len, size_t *outRuneWidth)
{
    assert(inOutString);
    assert(inOutBufSize);

    if (outRuneWidth)
        *outRuneWidth = 0;

    if (u8len == 1 && !isprint(*rune))
        return 0;

    size_t len = (*inOutString ? strlen(*inOutString) : 0);
    if (!*inOutString && !(*inOutString = calloc(1, (*inOutBufSize = u8len + 1))))
        return 0;

    if (len + u8len >= *inOutBufSize) {
        void *tmp;
        if (!(tmp = realloc(*inOutString, (*inOutBufSize * 2)))) {
            if (!(tmp = malloc((*inOutBufSize * 2))))
                return 0;

            memcpy(tmp, *inOutString, *inOutBufSize);
            free(*inOutString);
        }

        memset(tmp + *inOutBufSize, 0, *inOutBufSize);
        *inOutString = tmp;
        *inOutBufSize *= 2;
    }

    char *str = *inOutString + start;
    memmove(str + u8len, str, len - start);
    memcpy(str, rune, u8len);

    if (outRuneWidth)
        *outRuneWidth = _bmUtf8RuneWidth(rune, u8len);
    return u8len;
}

/**
 * Insert unicode character to UTF8 buffer.
 *
 * @param inOutString Reference to buffer.
 * @param inOutBufSize Reference to size of the buffer.
 * @param start Start offset where to insert to. (cursor)
 * @param unicode Unicode character to insert.
 * @param outRuneWidth Reference to size_t, return number of columns for inserted rune, or -1 on failure.
 * @return Number of bytes inserted to buffer.
 */
size_t _bmUnicodeInsert(char **inOutString, size_t *inOutBufSize, size_t start, unsigned int unicode, size_t *outRuneWidth)
{
    assert(inOutString);
    assert(inOutBufSize);

    char u8len = ((unicode < 0x80) ? 1 : ((unicode < 0x800) ? 2 : ((unicode < 0x10000) ? 3 : 4)));
    char mb[5] = { 0, 0, 0, 0 };

    if (u8len == 1) {
        mb[0] = unicode;
    } else {
        size_t i, j;
        for (i = j = u8len; j > 1; --j) mb[j - 1] = 0x80 | (0x3F & (unicode >> ((i - j) * 6)));
        mb[0] = (~0) << (8 - i);
        mb[0] |= (unicode >> (i * 6 - 6));
    }

    return _bmUtf8RuneInsert(inOutString, inOutBufSize, start, mb, u8len, outRuneWidth);
}

/* vim: set ts=8 sw=4 tw=0 :*/
