/* Underscore memory runtime: release heap-owned text/results and native pointers. */
#include <stdlib.h>

static void uc_memory_escape(void *pointer) { free(pointer); }
