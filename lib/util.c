#define _XOPEN_SOURCE
#include <wchar.h> /* wcswidth */
#undef _XOPEN_SOURCE

#include "internal.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

/**
 * Portable strdup.
 *
 * @param string C "string" to copy.
 * @return Copy of the given C "string".
 */
char*
bm_strdup(const char *string)
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
 * Small wrapper around realloc.
 * Resizes the buffer.
 *
 * @param in_out_buffer Reference to the input buffer that will be modified on succesful resize.
 * @param in_out_size Current buffer size, will be modified with new size on succesful resize.
 * @param nsize New size to resize the buffer to.
 * @return true for succesful resize, false for failure.
 */
bool
bm_resize_buffer(char **in_out_buffer, size_t *in_out_size, size_t nsize)
{
    assert(in_out_buffer && in_out_size);

    if (nsize == 0 || nsize <= *in_out_size)
        return false;

    void *tmp;
    if (!(tmp = realloc(*in_out_buffer, nsize)))
        return false;

    *in_out_buffer = tmp;
    *in_out_size = nsize;
    return true;
}

/**
 * Formatted printf that returns allocated char array.
 *
 * @param fmt Format as C "string".
 * @return Copy of the formatted C "string".
 */
char*
bm_dprintf(const char *fmt, ...)
{
   assert(fmt);

   va_list args;
   va_start(args, fmt);
   size_t len = vsnprintf(NULL, 0, fmt, args) + 1;
   va_end(args);

   char *buffer;
   if (!(buffer = calloc(1, len)))
      return NULL;

   va_start(args, fmt);
   vsnprintf(buffer, len, fmt, args);
   va_end(args);
   return buffer;
}

/**
 * Formatted printf that reuses and grows buffer when neccessary.
 *
 * @param in_out_buffer Reference to buffer that holds the new formatted text.
 * @param in_out_len Reference to the length of current buffer and outs as resized length.
 * @param fmt Format as C "string".
 * @param va_list Argument list.
 * @return true if successful, false if failure.
 */
bool
bm_vrprintf(char **in_out_buffer, size_t *in_out_len, const char *fmt, va_list args)
{
    assert(in_out_buffer && in_out_len && fmt);

    va_list copy;
    va_copy(copy, args);

    size_t len = vsnprintf(NULL, 0, fmt, args) + 1;

    if ((!*in_out_buffer || *in_out_len < len) && !bm_resize_buffer(in_out_buffer, in_out_len, len))
        return false;

    vsnprintf(*in_out_buffer, len, fmt, copy);
    va_end(copy);
    return true;
}

/**
 * Replaces next token in string with '\0' and returns position for the replaced token.
 *
 * @param string C "string" where token will be replaced.
 * @param out_next Reference to position of next delimiter, or 0 if none.
 * @return Position of the replaced token.
 */
size_t
bm_strip_token(char *string, const char *token, size_t *out_next)
{
    size_t len = strcspn(string, token);

    if (out_next)
        *out_next = len + (string[len] != 0);

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
int
bm_strupcmp(const char *hay, const char *needle)
{
    return bm_strnupcmp(hay, needle, strlen(hay));
}

/**
 * Portable case-insensitive strncmp.
 *
 * @param hay C "string" to match against.
 * @param needle C "string" to match.
 * @return Less than, equal to or greater than zero if hay is lexicographically less than, equal to or greater than needle.
 */
int
bm_strnupcmp(const char *hay, const char *needle, size_t len)
{
    const unsigned char *p1 = (const unsigned char*)hay;
    const unsigned char *p2 = (const unsigned char*)needle;

    unsigned char a = 0, b = 0;
    for (size_t i = 0; len > 0; --len, ++i)
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
char*
bm_strupstr(const char *hay, const char *needle)
{
    size_t r = 0, p = 0, len, len2;

    if ((len = strlen(hay)) < (len2 = strlen(needle)))
        return NULL;

    if (!bm_strnupcmp(hay, needle, len2))
        return (char*)hay;

    for (size_t i = 0; i < len; ++i) {
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
int32_t
bm_utf8_string_screen_width(const char *string)
{
    assert(string);

    char *mstr;
    if (!(mstr = bm_strdup(string)))
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

    int32_t length = wcswidth(wstring, num_char);
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
size_t
bm_utf8_rune_next(const char *string, size_t start)
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
size_t
bm_utf8_rune_prev(const char *string, size_t start)
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
size_t
bm_utf8_rune_width(const char *rune, uint32_t u8len)
{
    assert(rune);
    char mb[5] = { 0, 0, 0, 0, 0 };
    memcpy(mb, rune, (u8len > 4 ? 4 : u8len));
    return bm_utf8_string_screen_width(mb);
}

/**
 * Remove previous UTF8 rune from buffer.
 *
 * @param string Null terminated C "string".
 * @param start Start offset where to delete from. (cursor)
 * @param out_rune_width Reference to size_t, return number of columns for removed rune, or -1 on failure.
 * @return Number of bytes removed from buffer.
 */
size_t
bm_utf8_rune_remove(char *string, size_t start, size_t *out_rune_width)
{
    assert(string);

    if (out_rune_width)
        *out_rune_width = 0;

    size_t len = strlen(string), oldStart = start;
    if (len == 0 || len < start || !*string)
        return 0;

    start -= bm_utf8_rune_prev(string, start);

    if (out_rune_width)
        *out_rune_width = bm_utf8_rune_width(string + start, oldStart - start);

    memmove(string + start, string + oldStart, len - oldStart);
    string[len - (oldStart - start)] = 0;
    return (oldStart - start);
}

/**
 * Insert UTF8 rune to buffer.
 *
 * @param in_out_string Reference to buffer.
 * @param in_out_buf_size Reference to size of the buffer.
 * @param start Start offset where to insert to. (cursor)
 * @param rune Buffer to insert to string.
 * @param u8len Byte length of the rune.
 * @param out_rune_width Reference to size_t, return number of columns for inserted rune, or -1 on failure.
 * @return Number of bytes inserted to buffer.
 */
size_t
bm_utf8_rune_insert(char **in_out_string, size_t *in_out_buf_size, size_t start, const char *rune, uint32_t u8len, size_t *out_rune_width)
{
    assert(in_out_string);
    assert(in_out_buf_size);

    if (out_rune_width)
        *out_rune_width = 0;

    if (u8len == 1 && !isprint(*rune))
        return 0;

    size_t len = (*in_out_string ? strlen(*in_out_string) : 0);
    if (!*in_out_string && !(*in_out_string = calloc(1, (*in_out_buf_size = u8len + 1))))
        return 0;

    if (len + u8len >= *in_out_buf_size) {
        void *tmp;
        if (!(tmp = realloc(*in_out_string, (*in_out_buf_size * 2)))) {
            if (!(tmp = malloc((*in_out_buf_size * 2))))
                return 0;

            memcpy(tmp, *in_out_string, *in_out_buf_size);
            free(*in_out_string);
        }

        memset((char*)tmp + *in_out_buf_size, 0, *in_out_buf_size);
        *in_out_string = tmp;
        *in_out_buf_size *= 2;
    }

    char *str = *in_out_string + start;
    memmove(str + u8len, str, len - start);
    memcpy(str, rune, u8len);
    (*in_out_string)[len + u8len] = 0;

    if (out_rune_width)
        *out_rune_width = bm_utf8_rune_width(rune, u8len);
    return u8len;
}

/**
 * Insert unicode character to UTF8 buffer.
 *
 * @param in_out_string Reference to buffer.
 * @param in_out_buf_size Reference to size of the buffer.
 * @param start Start offset where to insert to. (cursor)
 * @param unicode Unicode character to insert.
 * @param out_rune_width Reference to size_t, return number of columns for inserted rune, or -1 on failure.
 * @return Number of bytes inserted to buffer.
 */
size_t
bm_unicode_insert(char **in_out_string, size_t *in_out_buf_size, size_t start, uint32_t unicode, size_t *out_rune_width)
{
    assert(in_out_string && in_out_buf_size);

    uint8_t u8len = ((unicode < 0x80) ? 1 : ((unicode < 0x800) ? 2 : ((unicode < 0x10000) ? 3 : 4)));
    char mb[5] = { 0, 0, 0, 0 };

    if (u8len == 1) {
        mb[0] = unicode;
    } else {
        size_t j;
        for (j = u8len; j > 1; --j) mb[j - 1] = 0x80 | (0x3F & (unicode >> ((u8len - j) * 6)));
        mb[0] = (uint8_t)(~0) << (8 - u8len);
        mb[0] |= (unicode >> (u8len * 6 - 6));
    }

    return bm_utf8_rune_insert(in_out_string, in_out_buf_size, start, mb, u8len, out_rune_width);
}

bool
bm_menu_item_is_selected(const struct bm_menu *menu, const struct bm_item *item)
{
    assert(menu);
    assert(item);

    uint32_t i, count;
    struct bm_item **items = bm_menu_get_selected_items(menu, &count);
    for (i = 0; i < count && items[i] != item; ++i);
    return (i < count);
}

/* vim: set ts=8 sw=4 tw=0 :*/
