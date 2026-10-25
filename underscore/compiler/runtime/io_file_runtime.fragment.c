#include "../global.h"

/* Small standard runtime used by io.* and file.* built-ins. */
static char io_nl[2] = "\n";
static char uc_io_input_buffer[4096];

static char *uc_io_in(void) {
    if (fgets(uc_io_input_buffer, sizeof(uc_io_input_buffer), stdin) == NULL) {
        uc_io_input_buffer[0] = '\0';
        return uc_io_input_buffer;
    }
    size_t n = strlen(uc_io_input_buffer);
    while (n > 0 && (uc_io_input_buffer[n - 1] == '\n' || uc_io_input_buffer[n - 1] == '\r'))
        uc_io_input_buffer[--n] = '\0';
    return uc_io_input_buffer;
}

static int uc_io_charin(void) {
    int first = getchar();
    if (first == EOF) return -1;
    /* An empty line is itself the input; do not consume the following line. */
    if (first == '\n') return '\n';
    int ch;
    do { ch = getchar(); } while (ch != '\n' && ch != EOF);
    return (unsigned char)first;
}

static int uc_io_errout_newline(void) { return fputc('\n', stderr) == EOF ? -1 : 0; }
static int uc_io_errout_int(long long value) { return fprintf(stderr, "%lld\n", value); }
static int uc_io_errout_fp(double value) { return fprintf(stderr, "%.9g\n", value); }
static int uc_io_errout_str(const char *value) { return fprintf(stderr, "%s\n", value ? value : "(null)"); }

#define UC_FILE_SLOTS 64
#define UC_FILE_PATH_SIZE 1024
#define UC_FILE_LINE_SIZE 4096
typedef struct {
    FILE *stream;
    char path[UC_FILE_PATH_SIZE];
    char line[UC_FILE_LINE_SIZE];
} UcFileSlot;
static UcFileSlot uc_files[UC_FILE_SLOTS];

static int uc_file_slot(int handle) {
    return handle >= 0 && handle < UC_FILE_SLOTS && uc_files[handle].stream != NULL;
}
static int uc_file_open(const char *path, const char *mode) {
    if (path == NULL || mode == NULL) return -1;
    FILE *exists = fopen(path, "rb");
    if (exists == NULL) return -1; /* open never creates a missing file */
    fclose(exists);
    FILE *stream = fopen(path, mode);
    if (stream == NULL) return -1;
    for (int i = 0; i < UC_FILE_SLOTS; ++i) {
        if (uc_files[i].stream == NULL) {
            uc_files[i].stream = stream;
            (void)snprintf(uc_files[i].path, sizeof(uc_files[i].path), "%s", path);
            uc_files[i].line[0] = '\0';
            return i;
        }
    }
    fclose(stream);
    return -1;
}
static int uc_file_create(const char *path) {
    if (path == NULL) return -1;
    FILE *stream = fopen(path, "wb+");
    if (stream == NULL) return -1;
    for (int i = 0; i < UC_FILE_SLOTS; ++i) {
        if (uc_files[i].stream == NULL) {
            uc_files[i].stream = stream;
            (void)snprintf(uc_files[i].path, sizeof(uc_files[i].path), "%s", path);
            uc_files[i].line[0] = '\0';
            return i;
        }
    }
    fclose(stream);
    return -1;
}
static int uc_file_close(int handle) {
    if (!uc_file_slot(handle)) return -1;
    int result = fclose(uc_files[handle].stream);
    uc_files[handle].stream = NULL;
    uc_files[handle].path[0] = '\0';
    uc_files[handle].line[0] = '\0';
    return result;
}
static char *uc_file_read(int handle) {
    if (!uc_file_slot(handle)) return NULL;
    if (fgets(uc_files[handle].line, sizeof(uc_files[handle].line), uc_files[handle].stream) == NULL) {
        uc_files[handle].line[0] = '\0';
        return uc_files[handle].line;
    }
    size_t n = strlen(uc_files[handle].line);
    while (n > 0 && (uc_files[handle].line[n - 1] == '\n' || uc_files[handle].line[n - 1] == '\r'))
        uc_files[handle].line[--n] = '\0';
    return uc_files[handle].line;
}
static int uc_file_write(int handle, const char *contents) {
    if (!uc_file_slot(handle) || contents == NULL) return -1;
    size_t length = strlen(contents);
    return fwrite(contents, 1, length, uc_files[handle].stream) == length ? 0 : -1;
}
static int uc_file_delete(const char *path) { return path == NULL ? -1 : remove(path); }
static int uc_file_rename(const char *path, const char *new_path) {
    return path == NULL || new_path == NULL ? -1 : rename(path, new_path);
}
static const char *uc_file_gpath(int handle) {
    return uc_file_slot(handle) ? uc_files[handle].path : "";
}
static long long uc_file_gsize(int handle) {
    if (!uc_file_slot(handle)) return -1;
    long current = ftell(uc_files[handle].stream);
    if (current < 0 || fseek(uc_files[handle].stream, 0, SEEK_END) != 0) return -1;
    long end = ftell(uc_files[handle].stream);
    (void)fseek(uc_files[handle].stream, current, SEEK_SET);
    return end < 0 ? -1 : (long long)end;
}
