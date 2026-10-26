/*
 * Underscore native runtime fragment: io
 *
 * Source contract: docs/libSyntaxList._, the `io` section.
 * Intended to be emitted into the generated C translation unit by codegen.
 * It is not a standalone translation unit: its helpers are `static` so each
 * generated program gets its own small runtime state.
 *
 * Public source-language operations represented here:
 *   io.out(value)          formatted value followed by newline
 *   io.in()                read one line from stdin
 *   io.nl / io.nl()        the newline string
 *   io.strout(text, index) write one character, without a newline
 *   io.charin()            first character on an input line, or -1 at EOF
 *   io.errout()            newline on stderr
 *   io.errout(value)       formatted value on stderr, followed by newline
 *   io.flush()             flush stdout
 *
 * Return conventions for native helpers:
 *   output/flush helpers return the C I/O result (negative means failure);
 *   uc_io_in returns a runtime-owned buffer, overwritten by its next call;
 *   uc_io_charin returns an unsigned character as int, newline for an empty
 *   line, and -1 at EOF.
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>

static const char io_nl[2] = "\n";
#define UC_IO_INPUT_CAPACITY 4096
static char uc_io_input_buffer[UC_IO_INPUT_CAPACITY];

/* Read at most capacity - 1 bytes and remove the line ending, if present. */
static char *uc_io_in(void) {
    if (fgets(uc_io_input_buffer, (int)sizeof(uc_io_input_buffer), stdin) == NULL) {
        uc_io_input_buffer[0] = '\0';
        return uc_io_input_buffer;
    }

    size_t length = strlen(uc_io_input_buffer);
    while (length > 0 &&
           (uc_io_input_buffer[length - 1] == '\n' ||
            uc_io_input_buffer[length - 1] == '\r')) {
        uc_io_input_buffer[--length] = '\0';
    }
    return uc_io_input_buffer;
}

/* Consume the rest of this line so a following io.in() starts on a new line. */
static int uc_io_charin(void) {
    int first = getchar();
    if (first == EOF) {
        return -1;
    }
    if (first == '\n') {
        return '\n';
    }

    int ch;
    do {
        ch = getchar();
    } while (ch != '\n' && ch != EOF);

    return (unsigned char)first;
}

static int uc_io_out_int(long long value) {
    return fprintf(stdout, "%lld\n", value);
}

static int uc_io_out_fp(double value) {
    return fprintf(stdout, "%.9g\n", value);
}

static int uc_io_out_str(const char *value) {
    return fprintf(stdout, "%s\n", value != NULL ? value : "(null)");
}

static int uc_io_out_ptr(const void *value) {
    return fprintf(stdout, "%p\n", value);
}

static int uc_io_strout(const char *text, long long index) {
    if (text == NULL || index < 0) {
        return -1;
    }

    size_t length = strlen(text);
    if ((unsigned long long)index >= (unsigned long long)length) {
        return -1;
    }
    return putchar((unsigned char)text[(size_t)index]);
}

static int uc_io_errout_newline(void) {
    return fputc('\n', stderr) == EOF ? -1 : 0;
}

static int uc_io_errout_int(long long value) {
    return fprintf(stderr, "%lld\n", value);
}

static int uc_io_errout_fp(double value) {
    return fprintf(stderr, "%.9g\n", value);
}

static int uc_io_errout_str(const char *value) {
    return fprintf(stderr, "%s\n", value != NULL ? value : "(null)");
}

static int uc_io_flush(void) {
    return fflush(stdout);
}
