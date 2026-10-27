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

int main(void)
{
    float root = math_sqrt(49);
    uc_io_out_fp((double)root);
    return 0;
}

