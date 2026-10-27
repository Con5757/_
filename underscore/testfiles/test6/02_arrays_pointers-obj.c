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
    int values[3] = {2LL, 15LL, 9LL};
    int first = values[0LL];
    int middle = values[1LL];
    int last = values[2LL];
    char title[25] = "Chapter One";
    uc_io_out_int((long long)first);
    uc_io_out_int((long long)middle);
    uc_io_out_int((long long)last);
    uc_io_out_str((const char *)title);
    int score = 20LL;
    int *pScore = &score;
    int observed = *pScore;
    uc_io_out_int((long long)observed);
    int *firstPointer = &score;
    int * *secondPointer = &firstPointer;
    int observedTwice = **secondPointer;
    uc_io_out_int((long long)observedTwice);
    return 0;
}

