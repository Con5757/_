#include "manifest.h"

#include "../diagnostic/diagnostic.h"
#include "../lex/lex.h"
#include "../parse/parse.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <sys/stat.h>
#define PATH_SEPARATOR '\\'
#else
#include <dirent.h>
#include <sys/stat.h>
#define PATH_SEPARATOR '/'
#endif

#define MANIFEST_PATH_CAPACITY 4096
#define MANIFEST_FILE_CAPACITY 512
#define MANIFEST_NAME_CAPACITY 256

typedef struct ManifestFileList {
  char names[MANIFEST_FILE_CAPACITY][MANIFEST_NAME_CAPACITY];
  size_t count;
} ManifestFileList;

static int pathFormat(char *output, size_t capacity, const char *left,
                      const char *right) {
  size_t length;
  int written;
  if (output == NULL || capacity == 0 || left == NULL || right == NULL) {
    return 0;
  }
  length = strlen(left);
  written = snprintf(output, capacity, "%s%s%s", left,
                     length > 0 && (left[length - 1] == '/' ||
                                    left[length - 1] == '\\')
                         ? ""
#ifdef _WIN32
                         : "\\",
#else
                         : "/",
#endif
                     right);
  return written >= 0 && (size_t)written < capacity;
}

static int copyText(char *destination, size_t capacity, const char *source) {
  int written;
  if (destination == NULL || capacity == 0 || source == NULL) return 0;
  written = snprintf(destination, capacity, "%s", source);
  return written >= 0 && (size_t)written < capacity;
}

static void directoryOf(const char *path, char *directory, size_t capacity) {
  const char *slash;
  const char *backslash;
  size_t length;
  if (!copyText(directory, capacity, path)) {
    directory[0] = '\0';
    return;
  }
  slash = strrchr(directory, '/');
  backslash = strrchr(directory, '\\');
  if (backslash != NULL && (slash == NULL || backslash > slash)) slash = backslash;
  if (slash == NULL) {
    (void)copyText(directory, capacity, ".");
    return;
  }
  length = (size_t)(slash - directory);
  if (length == 0) length = 1;
  directory[length] = '\0';
}

static const char *baseNameOf(const char *path) {
  const char *slash = strrchr(path, '/');
  const char *backslash = strrchr(path, '\\');
  if (backslash != NULL && (slash == NULL || backslash > slash)) slash = backslash;
  return slash == NULL ? path : slash + 1;
}

static int endsWithModuleExtension(const char *name) {
  size_t length = strlen(name);
  return length >= 2 && name[length - 2] == '.' && name[length - 1] == '_';
}

static int compareNames(const void *left, const void *right) {
  const char *const *a = (const char *const *)left;
  const char *const *b = (const char *const *)right;
  return strcmp(*a, *b);
}

static int addManifestFile(ManifestFileList *files, const char *name) {
  size_t length;
  if (files == NULL || name == NULL || !endsWithModuleExtension(name)) return 1;
  length = strlen(name);
  if (length == 0 || length >= MANIFEST_NAME_CAPACITY) return 0;
  if (files->count >= MANIFEST_FILE_CAPACITY) return 0;
  for (size_t i = 0; i < files->count; ++i) {
    if (strcmp(files->names[i], name) == 0) return 1;
  }
  memcpy(files->names[files->count++], name, length + 1);
  return 1;
}

static int listModuleFiles(const char *directory, ManifestFileList *files) {
  memset(files, 0, sizeof(*files));
#ifdef _WIN32
  char pattern[MANIFEST_PATH_CAPACITY];
  struct _finddata_t entry;
  intptr_t handle;
  int written = snprintf(pattern, sizeof(pattern), "%s\\*._", directory);
  if (written < 0 || (size_t)written >= sizeof(pattern)) {
    fprintf(stderr, "manifest: directory path is too long\n");
    return 0;
  }
  handle = _findfirst(pattern, &entry);
  if (handle == -1) {
    /* An empty directory is a valid manifest target. */
    struct _stat status;
    if (_stat(directory, &status) != 0 || (status.st_mode & _S_IFDIR) == 0) {
      fprintf(stderr, "manifest: target directory does not exist: %s\n", directory);
      return 0;
    }
    return 1;
  }
  do {
    if (!addManifestFile(files, entry.name)) {
      _findclose(handle);
      fprintf(stderr, "manifest: too many or overlong module filenames\n");
      return 0;
    }
  } while (_findnext(handle, &entry) == 0);
  _findclose(handle);
#else
  DIR *stream = opendir(directory);
  struct dirent *entry;
  if (stream == NULL) {
    fprintf(stderr, "manifest: could not open target directory '%s': %s\n",
            directory, strerror(errno));
    return 0;
  }
  while ((entry = readdir(stream)) != NULL) {
    struct stat status;
    char fullPath[MANIFEST_PATH_CAPACITY];
    if (!endsWithModuleExtension(entry->d_name)) continue;
    if (!pathFormat(fullPath, sizeof(fullPath), directory, entry->d_name)) {
      closedir(stream);
      fprintf(stderr, "manifest: file path is too long\n");
      return 0;
    }
    if (stat(fullPath, &status) != 0 || !S_ISREG(status.st_mode)) continue;
    if (!addManifestFile(files, entry->d_name)) {
      closedir(stream);
      fprintf(stderr, "manifest: too many or overlong module filenames\n");
      return 0;
    }
  }
  closedir(stream);
#endif
  /* Stable output makes generated manifests easy to review in version control. */
  if (files->count > 1) {
    /* The fixed-width rows are contiguous; compare via temporary pointers. */
    const char *names[MANIFEST_FILE_CAPACITY];
    for (size_t i = 0; i < files->count; ++i) names[i] = files->names[i];
    qsort(names, files->count, sizeof(names[0]), compareNames);
    char sorted[MANIFEST_FILE_CAPACITY][MANIFEST_NAME_CAPACITY];
    for (size_t i = 0; i < files->count; ++i) {
      if (!copyText(sorted[i], sizeof(sorted[i]), names[i])) return 0;
    }
    for (size_t i = 0; i < files->count; ++i) {
      memcpy(files->names[i], sorted[i], strlen(sorted[i]) + 1);
    }
  }
  return 1;
}

static char *readSource(const char *path) {
  FILE *stream = fopen(path, "rb");
  long size;
  char *buffer;
  size_t readCount;
  if (stream == NULL) return NULL;
  if (fseek(stream, 0, SEEK_END) != 0 || (size = ftell(stream)) < 0 ||
      fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return NULL;
  }
  buffer = (char *)malloc((size_t)size + 1);
  if (buffer == NULL) {
    fclose(stream);
    return NULL;
  }
  readCount = fread(buffer, 1, (size_t)size, stream);
  if (readCount != (size_t)size && ferror(stream)) {
    free(buffer);
    fclose(stream);
    return NULL;
  }
  buffer[readCount] = '\0';
  fclose(stream);
  return buffer;
}

static void writeType(FILE *output, Type type) {
  const char *prefix = type.kind == TYPE_INT ? "int" :
                       type.kind == TYPE_FP ? "fp" :
                       type.kind == TYPE_STR ? "str" : "void";
  fprintf(output, "%s", prefix);
  if (type.width > 0) fprintf(output, "%d", type.width);
}

static void writeModifiers(FILE *output, int flags) {
  if ((flags & DECL_FLAG_GLOBAL) != 0) fputs("global ", output);
  if ((flags & DECL_FLAG_STATIC) != 0) fputs("static ", output);
  if ((flags & DECL_FLAG_CONST) != 0) fputs("const ", output);
  if ((flags & DECL_FLAG_PUBLIC) != 0) fputs("public ", output);
  if ((flags & DECL_FLAG_PRIVATE) != 0) fputs("private ", output);
  if ((flags & DECL_FLAG_HIDDEN) != 0) fputs("hidden ", output);
}

static void writeDeclaration(FILE *output, const ASTNode *node) {
  Type type = node->declaredType;
  writeModifiers(output, node->storageFlags);
  writeType(output, type);
  if ((node->storageFlags & DECL_FLAG_DOUBLE_POINTER) != 0) fputs(" -->", output);
  else if ((node->storageFlags & DECL_FLAG_POINTER) != 0) fputs(" ->", output);
  else fputc(' ', output);
  fputs(node->name, output);
  if (node->type == AST_ARRAY_DECL) {
    if (type.kind == TYPE_STR && node->typeSize > 0) fprintf(output, "[%d]", node->typeSize);
    if (node->arrayCount > 0) fprintf(output, "<%d>", node->arrayCount);
  }
  fputc('\n', output);
}

static void writeDataType(FILE *output, const ASTNode *node) {
  if (node->type == AST_DATA) {
    fputs("    data", output);
    if (node->name[0] != '\0') fprintf(output, " %s", node->name);
    fputs(" {\n", output);
    for (const ASTNode *field = node->child; field != NULL; field = field->next) {
      fputs("        ", output);
      writeDeclaration(output, field);
    }
    fputs("    }\n", output);
  } else if (node->type == AST_SET) {
    fputs("    set { ", output);
    for (const ASTNode *item = node->child; item != NULL; item = item->next) {
      fputs(item->name, output);
      if (item->next != NULL) fputs(", ", output);
    }
    fputs(" }\n", output);
  } else if (node->type == AST_ALIAS) {
    fputs("    alias ", output);
    if (node->child != NULL && node->child->type == AST_SET) {
      fputs("set ", output);
      fputs(node->name, output);
    } else {
      writeType(output, node->declaredType);
      fprintf(output, " %s", node->name);
    }
    fputc('\n', output);
  }
}

static void writeModuleIndex(FILE *output, const char *directory,
                             const char *filename, int section) {
  char fullPath[MANIFEST_PATH_CAPACITY];
  char *source;
  ASTNode *program;
  char moduleName[MANIFEST_NAME_CAPACITY];
  size_t length = strlen(filename);
  if (length < 3 || length - 2 >= sizeof(moduleName)) return;
  memcpy(moduleName, filename, length - 2);
  moduleName[length - 2] = '\0';
  if (!pathFormat(fullPath, sizeof(fullPath), directory, filename)) return;
  source = readSource(fullPath);
  if (source == NULL) {
    fprintf(stderr, "manifest: could not read '%s'\n", fullPath);
    return;
  }
  diagnosticSetSource(fullPath, source);
  lex(source);
  /* This is a standalone indexing pass, not import expansion; reuse the AST
     arena after each file has been fully serialized. */
  astCount = 0;
  program = parseProgram();

  int printedVariables = 0;
  int printedStructs = 0;
  int printedFunctions = 0;
  int printedIncludes = 0;
  for (ASTNode *node = program->child; node != NULL; node = node->next) {
    if (section == 0 && (node->type == AST_INT_DECL || node->type == AST_FP_DECL ||
        node->type == AST_STR_DECL || node->type == AST_DECL ||
        node->type == AST_ARRAY_DECL || node->type == AST_VEC_DECL)) {
      if (!printedVariables) {
        fprintf(output, "@%s\n", moduleName);
        printedVariables = 1;
      }
      fputs("    ", output);
      if ((node->storageFlags & DECL_FLAG_GLOBAL) == 0) fputs("global ", output);
      writeDeclaration(output, node);
    } else if (section == 1 && (node->type == AST_DATA || node->type == AST_SET ||
               node->type == AST_ALIAS)) {
      if (!printedStructs) {
        fprintf(output, "@%s\n", moduleName);
        printedStructs = 1;
      }
      writeDataType(output, node);
    } else if (section == 2 && node->type == AST_METHOD && node->method != NULL) {
      Method *method = node->method;
      if (!printedFunctions) {
        fprintf(output, "@%s\n", moduleName);
        printedFunctions = 1;
      }
      fputs("    ", output);
      writeType(output, method->returnType);
      fprintf(output, " %s(", method->name != NULL ? method->name : "<unnamed>");
      for (int i = 0; i < method->paramCount; ++i) {
        if (i > 0) fputs(", ", output);
        writeType(output, method->params[i].type);
        fprintf(output, " %s", method->params[i].name);
      }
      fputc(')', output);
      if (method->flags[0] != '\0') fprintf(output, " <%s>", method->flags);
      fputc('\n', output);
    } else if (section == 3 && node->type == AST_INCLUDE) {
      if (!printedIncludes) {
        fprintf(output, "@%s\n", moduleName);
        printedIncludes = 1;
      }
      fprintf(output, "    ?%s", node->name);
      for (ASTNode *item = node->child; item != NULL; item = item->next) {
        fprintf(output, " with %s", item->name);
      }
      fputc('\n', output);
    }
  }
  free(source);
}

static int manifestOutputPath(const char *sourceFile, const char *targetDirectory,
                              int pathMode, char *directory, size_t directoryCapacity,
                              char *outputPath, size_t outputCapacity) {
  char base[MANIFEST_NAME_CAPACITY];
  int written;
  if (pathMode) {
    const char *end;
    size_t length;
    if (targetDirectory == NULL || targetDirectory[0] == '\0' ||
        !copyText(directory, directoryCapacity, targetDirectory)) return 0;
    length = strlen(directory);
    while (length > 1 && (directory[length - 1] == '/' || directory[length - 1] == '\\')) {
      directory[--length] = '\0';
    }
    end = baseNameOf(directory);
    if (strcmp(end, ".") == 0 || strcmp(end, "..") == 0) end = "manifest";
    if (!copyText(base, sizeof(base), end)) return 0;
  } else {
    const char *filename;
    char stem[MANIFEST_NAME_CAPACITY];
    if (sourceFile == NULL || !copyText(directory, directoryCapacity, sourceFile)) return 0;
    directoryOf(sourceFile, directory, directoryCapacity);
    filename = baseNameOf(sourceFile);
    if (!copyText(stem, sizeof(stem), filename)) return 0;
    char *extension = strrchr(stem, '.');
    if (extension != NULL) *extension = '\0';
    if (!copyText(base, sizeof(base), stem)) return 0;
  }
  char filename[MANIFEST_NAME_CAPACITY + 8];
  written = snprintf(filename, sizeof(filename), "%s.ufg", base);
  if (written < 0 || (size_t)written >= sizeof(filename)) return 0;
  return pathFormat(outputPath, outputCapacity, directory, filename);
}

int manifestCreate(const char *sourceFile, const char *targetDirectory,
                   int pathMode) {
  char directory[MANIFEST_PATH_CAPACITY];
  char outputPath[MANIFEST_PATH_CAPACITY];
  ManifestFileList files;
  FILE *output;
  if (!manifestOutputPath(sourceFile, targetDirectory, pathMode, directory,
                          sizeof(directory), outputPath, sizeof(outputPath))) {
    fprintf(stderr, "manifest: invalid or overlong source/target path\n");
    return 1;
  }
  if (!listModuleFiles(directory, &files)) return 1;
  output = fopen(outputPath, "w");
  if (output == NULL) {
    fprintf(stderr, "manifest: cannot create '%s': %s\n", outputPath, strerror(errno));
    return 1;
  }

  fputs("; Generated by Underscore -cufg. Review before distributing.\n", output);
  fputs("?init\n\n?files\n", output);
  if (files.count == 0) fputs("    null\n", output);
  for (size_t i = 0; i < files.count; ++i) fprintf(output, "    \"%s\"\n", files.names[i]);
  fputs("\n?macros\n    null\n\n?includes\n", output);
  for (size_t i = 0; i < files.count; ++i) {
    char fullPath[MANIFEST_PATH_CAPACITY];
    char *source;
    ASTNode *program;
    if (!pathFormat(fullPath, sizeof(fullPath), directory, files.names[i])) continue;
    source = readSource(fullPath);
    if (source == NULL) continue;
    diagnosticSetSource(fullPath, source);
    lex(source);
    astCount = 0;
    program = parseProgram();
    for (ASTNode *node = program->child; node != NULL; node = node->next) {
      if (node->type == AST_INCLUDE) {
        char moduleName[MANIFEST_NAME_CAPACITY];
        size_t length = strlen(files.names[i]);
        if (length < 3 || length - 2 >= sizeof(moduleName)) continue;
        memcpy(moduleName, files.names[i], length - 2);
        moduleName[length - 2] = '\0';
        fprintf(output, "@%s\n    ?%s", moduleName, node->name);
        for (ASTNode *item = node->child; item != NULL; item = item->next)
          fprintf(output, " with %s", item->name);
        fputc('\n', output);
      }
    }
    free(source);
  }
  fputs("\n?variables\n", output);
  for (size_t i = 0; i < files.count; ++i) writeModuleIndex(output, directory, files.names[i], 0);
  fputs("\n?structs\n", output);
  for (size_t i = 0; i < files.count; ++i) writeModuleIndex(output, directory, files.names[i], 1);
  fputs("\n?functions\n", output);
  for (size_t i = 0; i < files.count; ++i) writeModuleIndex(output, directory, files.names[i], 2);
  fputs("\n?endit\n", output);
  if (fclose(output) != 0) {
    fprintf(stderr, "manifest: failed while writing '%s'\n", outputPath);
    return 1;
  }
  printf("created manifest: %s\n", outputPath);
  return 0;
}
