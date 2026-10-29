/* Underscore random runtime: pseudo-random only, not cryptographically secure. */
#include <stdlib.h>

static void uc_random_seed(long long seed) { srand((unsigned int)seed); }
static int uc_random_next(void) { return rand(); }

/* Inclusive interval. Returns -1 for an invalid interval; modulo bias is possible. */
static long long uc_random_range(long long minimum, long long maximum) {
    if (maximum < minimum) return -1;
    unsigned long long span = (unsigned long long)maximum - (unsigned long long)minimum + 1ULL;
    if (span == 0ULL || span > (unsigned long long)RAND_MAX + 1ULL) return -1;
    return minimum + (long long)((unsigned long long)(unsigned int)rand() % span);
}

static double uc_random_unit(void) {
    return (double)rand() / ((double)RAND_MAX + 1.0);
}
