/* Underscore standard I/O runtime. Linked into every generated executable. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *__uo_input_buffer;
static size_t __uo_input_capacity;

static int __uo_reserve_input(size_t required) {
    if (required <= __uo_input_capacity) return 1;
    size_t next = __uo_input_capacity ? __uo_input_capacity : 128;
    while (next < required) {
        if (next > ((size_t)-1) / 2) return 0;
        next *= 2;
    }
    char *grown = (char *)realloc(__uo_input_buffer, next);
    if (!grown) return 0;
    __uo_input_buffer = grown;
    __uo_input_capacity = next;
    return 1;
}

/* Returns a runtime-owned line buffer; it remains valid until the next io.in(). */
char *__io_InCall(void) {
    size_t length = 0;
    int ch;
    if (!__uo_reserve_input(1)) return "";
    while ((ch = fgetc(stdin)) != EOF && ch != '\n') {
        if (!__uo_reserve_input(length + 2)) {
            __uo_input_buffer[length] = '\0';
            return __uo_input_buffer;
        }
        __uo_input_buffer[length++] = (char)ch;
    }
    if (length && __uo_input_buffer[length - 1] == '\r') --length;
    __uo_input_buffer[length] = '\0';
    return __uo_input_buffer;
}

void __io_OutCall(const char *message) {
    fputs(message ? message : "", stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

void __io_OutInt(long long value) {
    printf("%lld\n", value);
    fflush(stdout);
}

void __io_OutFloat(double value) {
    printf("%.15g\n", value);
    fflush(stdout);
}

void __io_OutBool(int value) {
    fputs(value ? "true\n" : "false\n", stdout);
    fflush(stdout);
}

void __io_OutPointer(const void *value) {
    printf("%p\n", value);
    fflush(stdout);
}

void __io_NlCall(void) {
    fputc('\n', stdout);
    fflush(stdout);
}
