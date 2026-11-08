/*
 * Underscore native runtime fragment: string
 *
 * Source contract: docs/libSyntaxList._, the `string` section.
 * Intended to be emitted into the generated C translation unit by codegen.
 * Functions return C strings and are `static` to avoid exporting runtime
 * symbols from generated executables.
 *
 * Implemented operations:
 *   string.length(str)          byte length (excluding terminating NUL)
 *   string.copy(str)            heap-owned copy
 *   string.connect(str, str)    heap-owned concatenation
 *   string.slice(str, start,end) heap-owned [start, end) slice
 *   string.up(str)              ASCII uppercase in-place; returns same pointer
 *   string.low(str)             ASCII lowercase in-place; returns same pointer
 *   string.trim(str)             trim surrounding whitespace in-place
 *
 * Allocated return values from copy/connect/slice must eventually be released
 * with the language's memory deallocation API (memory.escape) once that API is
 * wired to free(). The mutating functions require writable, NUL-terminated
 * storage; do not pass a string literal to up/low/trim.
 *
 * `substr` and `replace` are deliberately not implemented here yet: the docs
 * list `substr(str, str)` without defining which string/delimiter behaviour is
 * intended, and list `replace()` without an argument signature. See README.
 */

#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static long long uc_string_length(const char *text) {
    if (text == NULL) {
        return -1;
    }
    size_t length = strlen(text);
    if (length > (size_t)LLONG_MAX) {
        return -1;
    }
    return (long long)length;
}

static char *uc_string_copy(const char *text) {
    if (text == NULL) {
        return NULL;
    }

    size_t length = strlen(text);
    if (length == SIZE_MAX) {
        return NULL;
    }

    char *copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static char *uc_string_connect(const char *left, const char *right) {
    if (left == NULL || right == NULL) {
        return NULL;
    }

    size_t left_length = strlen(left);
    size_t right_length = strlen(right);
    if (left_length > SIZE_MAX - right_length - 1) {
        return NULL;
    }

    size_t result_length = left_length + right_length;
    char *result = (char *)malloc(result_length + 1);
    if (result == NULL) {
        return NULL;
    }
    memcpy(result, left, left_length);
    memcpy(result + left_length, right, right_length + 1);
    return result;
}

/* Return a heap-owned slice with start inclusive and end exclusive. */
static char *uc_string_slice(const char *text, long long start, long long end) {
    if (text == NULL || start < 0 || end < start) {
        return NULL;
    }

    size_t length = strlen(text);
    if ((unsigned long long)end > (unsigned long long)length) {
        return NULL;
    }

    size_t slice_length = (size_t)(end - start);
    char *result = (char *)malloc(slice_length + 1);
    if (result == NULL) {
        return NULL;
    }
    memcpy(result, text + (size_t)start, slice_length);
    result[slice_length] = '\0';
    return result;
}

/* These transformations follow the current std/string._ intent to mutate. */
static char *uc_string_up(char *text) {
    if (text == NULL) {
        return NULL;
    }

    for (unsigned char *cursor = (unsigned char *)text; *cursor != '\0'; ++cursor) {
        if (*cursor >= (unsigned char)'a' && *cursor <= (unsigned char)'z') {
            *cursor = (unsigned char)(*cursor - (unsigned char)('a' - 'A'));
        }
    }
    return text;
}

static char *uc_string_low(char *text) {
    if (text == NULL) {
        return NULL;
    }

    for (unsigned char *cursor = (unsigned char *)text; *cursor != '\0'; ++cursor) {
        if (*cursor >= (unsigned char)'A' && *cursor <= (unsigned char)'Z') {
            *cursor = (unsigned char)(*cursor + (unsigned char)('a' - 'A'));
        }
    }
    return text;
}

/* Trim leading/trailing C-locale whitespace in place; return same pointer. */
static char *uc_string_trim(char *text) {
    if (text == NULL) {
        return NULL;
    }

    char *begin = text;
    while (*begin != '\0' && isspace((unsigned char)*begin)) {
        ++begin;
    }

    if (begin != text) {
        memmove(text, begin, strlen(begin) + 1);
    }

    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) {
        text[--length] = '\0';
    }
    return text;
}
