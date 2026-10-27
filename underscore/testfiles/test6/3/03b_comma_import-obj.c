#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int uc_io_out_int(long long value) { return printf("%lld\n", value); }
static int uc_io_out_fp(double value) { return printf("%.9g\n", value); }
static int uc_io_out_str(const char *value) { return printf("%s\n", value != NULL ? value : "(null)"); }
static char uc_io_input_buffer[4096];
static char *uc_io_in(void) { size_t n; if (fgets(uc_io_input_buffer, (int)sizeof(uc_io_input_buffer), stdin) == NULL) { uc_io_input_buffer[0] = '\0'; return uc_io_input_buffer; } n = strlen(uc_io_input_buffer); while (n && (uc_io_input_buffer[n-1] == '\n' || uc_io_input_buffer[n-1] == '\r')) uc_io_input_buffer[--n] = '\0'; return uc_io_input_buffer; }


float math_abs(float  x);
float math_min(float  a, float  b);
float math_max(float  a, float  b);
float math_sqrt(float  x);
float math_clamp(float  value, float  low, float  high);
float math_sign(float  value);
float math_lerp(float  start, float  end, float  amount);
int string_length(char *  text);
char * string_up(char *  text);
char * string_low(char *  text);
int string_equals(char *  left, char *  right);
int string_starts_with(char *  text, char *  prefix);
int string_ends_with(char *  text, char *  suffix);
int string_contains(char *  text, char *  needle);


float math_abs(float  x)
{
    if (x < 0) {
        return -x;
    }
    return x;
}

float math_min(float  a, float  b)
{
    if (a < b) {
        return a;
    }
    return b;
}

float math_max(float  a, float  b)
{
    if (a > b) {
        return a;
    }
    return b;
}

float math_sqrt(float  x)
{
    if (x < 0) {
        return -1;
    }
    if (((x == 0) || (x == 1))) {
        return x;
    }
    float guess = 1;
    if (x > 1) {
        (guess = x);
    }
    for (int i = 0LL; (i < 100LL); (i++)) {
        float next = (0.5 * (guess + (x / guess)));
        if (math_abs((next - guess)) < 1e-07) {
            return next;
        }
        (guess = next);
    }
    return guess;
}

float math_clamp(float  value, float  low, float  high)
{
    if (low > high) {
        return value;
    }
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

float math_sign(float  value)
{
    if (value < 0) {
        return -1;
    }
    if (value > 0) {
        return 1;
    }
    return 0;
}

float math_lerp(float  start, float  end, float  amount)
{
    return (start + ((end - start) * amount));
}

int string_length(char *  text)
{
    int i = 0LL;
    while ((text[i] != 0LL)) {
        (i++);
    }
    return i;
}

char * string_up(char *  text)
{
    int i = 0LL;
    while ((text[i] != 0LL)) {
        if (((text[i] >= 97LL) && (text[i] <= 122LL))) {
            (text[i] = (text[i] - 32LL));
        }
        (i++);
    }
    return text;
}

char * string_low(char *  text)
{
    int i = 0LL;
    while ((text[i] != 0LL)) {
        if (((text[i] >= 65LL) && (text[i] <= 90LL))) {
            (text[i] = (text[i] + 32LL));
        }
        (i++);
    }
    return text;
}

int string_equals(char *  left, char *  right)
{
    int i = 0LL;
    while ((((left[i] != 0LL) || (right[i] != 0LL)))) {
        if (left[i] != right[i]) {
            return 0LL;
        }
        (i++);
    }
    return 1LL;
}

int string_starts_with(char *  text, char *  prefix)
{
    int i = 0LL;
    while ((prefix[i] != 0LL)) {
        if (((text[i] == 0LL) || (text[i] != prefix[i]))) {
            return 0LL;
        }
        (i++);
    }
    return 1LL;
}

int string_ends_with(char *  text, char *  suffix)
{
    int textLength = string_length(text);
    int suffixLength = string_length(suffix);
    if (suffixLength > textLength) {
        return 0LL;
    }
    int offset = (textLength - suffixLength);
    int i = 0LL;
    while ((suffix[i] != 0LL)) {
        if (text[(offset + i)] != suffix[i]) {
            return 0LL;
        }
        (i++);
    }
    return 1LL;
}

int string_contains(char *  text, char *  needle)
{
    if (needle[0LL] == 0LL) {
        return 1LL;
    }
    int i = 0LL;
    while ((text[i] != 0LL)) {
        int j = 0LL;
        while ((((needle[j] != 0LL) && (text[(i + j)] == needle[j])))) {
            (j++);
        }
        if (needle[j] == 0LL) {
            return 1LL;
        }
        (i++);
    }
    return 0LL;
}

int main(void)
{
    float root = math_sqrt(25);
    int titleLength = string_length("combined import");
    uc_io_out_fp((double)root);
    uc_io_out_int((long long)titleLength);
    return 0;
}

