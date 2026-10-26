/* Underscore time runtime. ctime/asctime use shared buffers and include a newline. */
#include <time.h>

static long long uc_time_utime(void) {
    time_t value = time(NULL);
    return value == (time_t)-1 ? -1LL : (long long)value;
}

static char *uc_time_ctime(long long epoch_seconds) {
    time_t value = (time_t)epoch_seconds;
    return ctime(&value);
}

static char *uc_time_utctime(long long epoch_seconds) {
    time_t value = (time_t)epoch_seconds;
    struct tm *parts = gmtime(&value);
    return parts != NULL ? asctime(parts) : NULL;
}

/* Difference in seconds, defined as left - right. */
static long long uc_time_timedif(long long left, long long right) {
    return left - right;
}
