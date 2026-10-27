#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int uc_io_out_int(long long value) { return printf("%lld\n", value); }
static int uc_io_out_fp(double value) { return printf("%.9g\n", value); }
static int uc_io_out_str(const char *value) { return printf("%s\n", value != NULL ? value : "(null)"); }
static char uc_io_input_buffer[4096];
static char *uc_io_in(void) { size_t n; if (fgets(uc_io_input_buffer, (int)sizeof(uc_io_input_buffer), stdin) == NULL) { uc_io_input_buffer[0] = '\0'; return uc_io_input_buffer; } n = strlen(uc_io_input_buffer); while (n && (uc_io_input_buffer[n-1] == '\n' || uc_io_input_buffer[n-1] == '\r')) uc_io_input_buffer[--n] = '\0'; return uc_io_input_buffer; }




int main(void)
{
    int32_t packetCount = 100LL;
    double precision = 1e-06;
    uc_io_out_int((long long)packetCount);
    uc_io_out_fp((double)precision);
    return 0;
}

