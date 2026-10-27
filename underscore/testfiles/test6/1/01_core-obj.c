#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int uc_io_out_int(long long value) { return printf("%lld\n", value); }
static int uc_io_out_fp(double value) { return printf("%.9g\n", value); }
static int uc_io_out_str(const char *value) { return printf("%s\n", value != NULL ? value : "(null)"); }
static char uc_io_input_buffer[4096];
static char *uc_io_in(void) { size_t n; if (fgets(uc_io_input_buffer, (int)sizeof(uc_io_input_buffer), stdin) == NULL) { uc_io_input_buffer[0] = '\0'; return uc_io_input_buffer; } n = strlen(uc_io_input_buffer); while (n && (uc_io_input_buffer[n-1] == '\n' || uc_io_input_buffer[n-1] == '\r')) uc_io_input_buffer[--n] = '\0'; return uc_io_input_buffer; }


int add(int  left, int  right);
int clampSmall(int  value);


int add(int  left, int  right)
{
    return (left + right);
}

int clampSmall(int  value)
{
    if (value < 0LL) {
        return 0LL;
    }
    if (value > 10LL) {
        return 10LL;
    }
    return value;
}

int main(void)
{
    int left = 7LL;
    int right = 5LL;
    int sum = add(left, right);
    float ratio = 0.75;
    char label[33] = "core smoke test";
    const int limit = 12LL;
    (sum += 1LL);
    (sum -= 1LL);
    (sum *= 2LL);
    (sum /= 2LL);
    (sum %= 13LL);
    uc_io_out_int((long long)sum);
    uc_io_out_fp((double)ratio);
    uc_io_out_str((const char *)label);
    if (((sum == limit) && !0)) {
        uc_io_out_str((const char *)"if branch");
    } else if (sum > limit) {
        uc_io_out_str((const char *)"elif branch");
    } else {
        uc_io_out_str((const char *)"else branch");
    }
    int count = 0LL;
    while ((count < 3LL)) {
        (count++);
        if (count == 2LL) {
            continue;
        }
        uc_io_out_int((long long)count);
    }
    for (int i = 0LL; (i < 3LL); (i++)) {
        uc_io_out_int((long long)i);
    }
    switch (count) {
        case 3LL:
            uc_io_out_str((const char *)"switch case");
            break;
        default:
            uc_io_out_str((const char *)"default case");
            break;
    }
    uc_io_out_int((long long)clampSmall(14LL));
    return 0;
}

