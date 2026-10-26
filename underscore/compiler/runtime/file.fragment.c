/*
 * Underscore native runtime fragment: file
 *
 * Source contract: docs/libSyntaxList._, the `file` section.
 * Intended to be emitted into the generated C translation unit by codegen.
 * The source language sees small integer file handles rather than FILE *.
 *
 * Implemented operations:
 *   file.open(path, mode)      existing file only; returns handle or -1
 *   file.create(path)          create/truncate; returns handle or -1
 *   file.close(handle)         0 on success, -1 for invalid handle/failure
 *   file.read(handle)          line without CR/LF; NULL at EOF/error
 *   file.write(handle, text)   writes bytes, no newline; 0 on success, -1 on failure
 *   file.delete(path)          remove path; 0 on success, -1 on failure
 *   file.rename(path, newPath) rename path; 0 on success, -1 on failure
 *   file.gpath(handle)         path stored for the handle, or empty string
 *   file.gsize(handle)         byte size, preserving the current stream position;
 *                             -1 on invalid handle/seek failure
 *
 * `file.read` returns a per-handle buffer which is overwritten by the next
 * read on that same handle and becomes invalid after close. Copy it if it must
 * survive. This is a text/line API, not binary input. The fixed table and
 * buffers make storage bounded and explicit for the early runtime.
 */

#include "../global.h"

#define UC_FILE_SLOTS 64
#define UC_FILE_PATH_CAPACITY 1024
#define UC_FILE_LINE_CAPACITY 4096

typedef struct UcFileSlot {
    FILE *stream;
    char path[UC_FILE_PATH_CAPACITY];
    char line[UC_FILE_LINE_CAPACITY];
} UcFileSlot;

static UcFileSlot uc_files[UC_FILE_SLOTS];

static int uc_file_valid_handle(int handle) {
    return handle >= 0 && handle < UC_FILE_SLOTS && uc_files[handle].stream != NULL;
}

static int uc_file_free_slot(void) {
    for (int index = 0; index < UC_FILE_SLOTS; ++index) {
        if (uc_files[index].stream == NULL) {
            return index;
        }
    }
    return -1;
}

static int uc_file_store_stream(int slot, FILE *stream, const char *path) {
    if (slot < 0 || slot >= UC_FILE_SLOTS || stream == NULL || path == NULL) {
        return -1;
    }

    size_t path_length = strlen(path);
    if (path_length >= sizeof(uc_files[slot].path)) {
        return -1;
    }

    uc_files[slot].stream = stream;
    memcpy(uc_files[slot].path, path, path_length + 1);
    uc_files[slot].line[0] = '\0';
    return slot;
}

/* Open may truncate an existing file if requested mode is "w", but must not
 * create a missing file. Check existence separately from the requested mode. */
static int uc_file_open(const char *path, const char *mode) {
    if (path == NULL || mode == NULL || path[0] == '\0' || mode[0] == '\0') {
        return -1;
    }

    size_t path_length = strlen(path);
    if (path_length >= UC_FILE_PATH_CAPACITY) {
        return -1;
    }

    int slot = uc_file_free_slot();
    if (slot < 0) {
        return -1;
    }

    FILE *probe = fopen(path, "rb");
    if (probe == NULL) {
        return -1;
    }
    (void)fclose(probe);

    FILE *stream = fopen(path, mode);
    if (stream == NULL) {
        return -1;
    }
    if (uc_file_store_stream(slot, stream, path) < 0) {
        (void)fclose(stream);
        return -1;
    }
    return slot;
}

static int uc_file_create(const char *path) {
    if (path == NULL || path[0] == '\0' || strlen(path) >= UC_FILE_PATH_CAPACITY) {
        return -1;
    }

    int slot = uc_file_free_slot();
    if (slot < 0) {
        return -1;
    }

    FILE *stream = fopen(path, "wb+");
    if (stream == NULL) {
        return -1;
    }
    if (uc_file_store_stream(slot, stream, path) < 0) {
        (void)fclose(stream);
        return -1;
    }
    return slot;
}

static int uc_file_close(int handle) {
    if (!uc_file_valid_handle(handle)) {
        return -1;
    }

    FILE *stream = uc_files[handle].stream;
    uc_files[handle].stream = NULL;
    uc_files[handle].path[0] = '\0';
    uc_files[handle].line[0] = '\0';
    return fclose(stream) == 0 ? 0 : -1;
}

static char *uc_file_read(int handle) {
    if (!uc_file_valid_handle(handle)) {
        return NULL;
    }

    UcFileSlot *slot = &uc_files[handle];
    if (fgets(slot->line, (int)sizeof(slot->line), slot->stream) == NULL) {
        slot->line[0] = '\0';
        return NULL;
    }

    size_t length = strlen(slot->line);
    while (length > 0 && (slot->line[length - 1] == '\n' ||
                          slot->line[length - 1] == '\r')) {
        slot->line[--length] = '\0';
    }
    return slot->line;
}

static int uc_file_write(int handle, const char *contents) {
    if (!uc_file_valid_handle(handle) || contents == NULL) {
        return -1;
    }

    size_t length = strlen(contents);
    if (fwrite(contents, 1, length, uc_files[handle].stream) != length) {
        return -1;
    }
    return ferror(uc_files[handle].stream) ? -1 : 0;
}

static int uc_file_delete(const char *path) {
    return path == NULL ? -1 : (remove(path) == 0 ? 0 : -1);
}

static int uc_file_rename(const char *path, const char *new_path) {
    if (path == NULL || new_path == NULL) {
        return -1;
    }
    return rename(path, new_path) == 0 ? 0 : -1;
}

static const char *uc_file_gpath(int handle) {
    return uc_file_valid_handle(handle) ? uc_files[handle].path : "";
}

static long long uc_file_gsize(int handle) {
    if (!uc_file_valid_handle(handle)) {
        return -1;
    }

    FILE *stream = uc_files[handle].stream;
    long original_position = ftell(stream);
    if (original_position < 0 || fseek(stream, 0L, SEEK_END) != 0) {
        return -1;
    }

    long end_position = ftell(stream);
    int restore_result = fseek(stream, original_position, SEEK_SET);
    if (end_position < 0 || restore_result != 0) {
        return -1;
    }
    return (long long)end_position;
}
