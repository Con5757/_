#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int uc_io_out_int(long long value) { return printf("%lld\n", value); }
static int uc_io_out_fp(double value) { return printf("%.9g\n", value); }
static int uc_io_out_str(const char *value) { return printf("%s\n", value != NULL ? value : "(null)"); }
static char uc_io_input_buffer[4096];
static char *uc_io_in(void) { size_t n; if (fgets(uc_io_input_buffer, (int)sizeof(uc_io_input_buffer), stdin) == NULL) { uc_io_input_buffer[0] = '\0'; return uc_io_input_buffer; } n = strlen(uc_io_input_buffer); while (n && (uc_io_input_buffer[n-1] == '\n' || uc_io_input_buffer[n-1] == '\r')) uc_io_input_buffer[--n] = '\0'; return uc_io_input_buffer; }


int logs_addValues(int  left, int  right);
void logs_writeLog(char *  message);


int logs_addValues(int  left, int  right)
{
    return (left + right);
}

void logs_writeLog(char *  message)
{
    uc_io_out_str((const char *)message);
    return;
}

int main(void)
{
    logs_writeLog("manifest keyword loaded");
    int result = logs_addValues(19LL, 23LL);
    uc_io_out_int((long long)result);
    return 0;
}

