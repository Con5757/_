#include "imports.h"

#define IMPORT_PATH_SIZE (MAX_NAME * 16)
#define MAX_IMPORTED_FILES 256
#define MAX_IMPORT_DEPTH 128

    typedef struct ImportedFile {
  char path[IMPORT_PATH_SIZE];
  int state; /* 1 = loading; 2 = fully expanded */
} ImportedFile;

static ImportedFile importedFiles[MAX_IMPORTED_FILES];
static int importedFileCount = 0;

static _Noreturn void importError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  fputs("import error: ", stderr);
  vfprintf(stderr, format, args);
  fputc('\n', stderr);
  va_end(args);
  exit(EXIT_FAILURE);
}

static void copyPath(char destination[IMPORT_PATH_SIZE], const char *source) {
  int length = snprintf(destination, IMPORT_PATH_SIZE, "%s",
                        source != NULL ? source : "");
  if (length < 0 || length >= IMPORT_PATH_SIZE) {
    importError("path is too long for this compiler");
  }
}

static void directoryOf(const char *path, char output[IMPORT_PATH_SIZE]) {
  const char *slash;
  const char *backslash;
  size_t length;

  copyPath(output, path);

  slash = strrchr(output, '/');
  backslash = strrchr(output, '\\');

  if (backslash != NULL && (slash == NULL || backslash > slash)) {
    slash = backslash;
  }

  if (slash == NULL) {
    copyPath(output, ".");
    return;
  }

  length = (size_t)(slash - output);
  if (length == 0) {
    length = 1; /* Preserve a leading root slash. */
  }

  output[length] = '\0';
}

static void joinPath(char output[IMPORT_PATH_SIZE], const char *left,
                     const char *right) {
  size_t length = strlen(left);
  const char *separator =
      (length > 0 && (left[length - 1] == '/' || left[length - 1] == '\\'))
          ? ""
          : "/";

  int written =
      snprintf(output, IMPORT_PATH_SIZE, "%s%s%s", left, separator, right);

  if (written < 0 || written >= IMPORT_PATH_SIZE) {
    importError("resolved path is too long");
  }
}

static int fileExists(const char *path) {
  FILE *file = fopen(path, "rb");

  if (file == NULL) {
    return 0;
  }

  fclose(file);
  return 1;
}

static int makeModulePath(char output[IMPORT_PATH_SIZE], const char *base,
                          const char *root, const char *module) {
  char directory[IMPORT_PATH_SIZE];
  char filename[MAX_NAME + 4];

  int written = snprintf(filename, sizeof(filename), "%s._", module);

  if (written < 0 || (size_t)written >= sizeof(filename)) {
    importError("module name `%s` is too long", module);
  }

  joinPath(directory, base, root);
  joinPath(output, directory, filename);

  return fileExists(output);
}

static int resolveModulePath(char output[IMPORT_PATH_SIZE], const char *root,
                             const char *module, const char *projectDirectory,
                             const char *executableDirectory) {
  if (strcmp(root, "std") != 0) {
    if (makeModulePath(output, projectDirectory, root, module)) {
      return 1;
    }
    return 0;
  }

  /*
   * `std` is special: prefer the standard library beside the compiler,
   * then support the source-tree layouts used while developing it.
   */
  if (makeModulePath(output, executableDirectory, "../std", module))
    return 1;
  if (makeModulePath(output, executableDirectory, "std", module))
    return 1;
  if (makeModulePath(output, projectDirectory, "underscore/std", module))
    return 1;
  if (makeModulePath(output, projectDirectory, "std", module))
    return 1;
  if (makeModulePath(output, ".", "../std", module))
    return 1;
  if (makeModulePath(output, ".", "std", module))
    return 1;

  return 0;
}

static void appendNode(ASTNode **first, ASTNode **last, ASTNode *node) {
  if (node == NULL) {
    return;
  }

  node->next = NULL;

  if (*first == NULL) {
    *first = node;
  } else {
    (*last)->next = node;
  }

  *last = node;
}

static void appendNodeChain(ASTNode **first, ASTNode **last, ASTNode *chain) {
  while (chain != NULL) {
    ASTNode *next = chain->next;
    appendNode(first, last, chain);
    chain = next;
  }
}

static int containsImport(ASTNode *node) {
  for (ASTNode *currentNode = node; currentNode != NULL;
       currentNode = currentNode->next) {
    if (currentNode->type == AST_INCLUDE) {
      return 1;
    }

    if (containsImport(currentNode->child) ||
        containsImport(currentNode->right) ||
        containsImport(currentNode->third) ||
        containsImport(currentNode->body)) {
      return 1;
    }
  }

  return 0;
}

static int isBuiltinCall(const char *name) {
  return strcmp(name, "out") == 0 || strcmp(name, "std.out") == 0 ||
         strcmp(name, "std::out") == 0 || strcmp(name, "in") == 0 ||
         strcmp(name, "std.in") == 0 || strcmp(name, "io.out") == 0 ||
         strcmp(name, "io.in") == 0 || strcmp(name, "io.errout") == 0 ||
         strcmp(name, "io.flush") == 0 || strcmp(name, "io.strout") == 0 ||
         strcmp(name, "io.charin") == 0 || strcmp(name, "io.nl") == 0;
}

static void qualifyCalls(ASTNode *node, const char *moduleName) {
  for (ASTNode *currentNode = node; currentNode != NULL;
       currentNode = currentNode->next) {
    if (currentNode->type == AST_CALL && !isBuiltinCall(currentNode->name) &&
        strchr(currentNode->name, '.') == NULL &&
        strstr(currentNode->name, "::") == NULL) {
      char qualified[MAX_NAME];

      int written = snprintf(qualified, sizeof(qualified), "%s.%s", moduleName,
                             currentNode->name);

      if (written < 0 || (size_t)written >= sizeof(qualified)) {
        importError(
            "qualified call name `%s.%s` exceeds the compiler's name limit",
            moduleName, currentNode->name);
      }

      snprintf(currentNode->name, sizeof(currentNode->name), "%s", qualified);
    }

    qualifyCalls(currentNode->child, moduleName);
    qualifyCalls(currentNode->right, moduleName);
    qualifyCalls(currentNode->third, moduleName);
    qualifyCalls(currentNode->body, moduleName);
  }
}

static void qualifyImportedMethod(ASTNode *node, const char *moduleName) {
  Method *method = node->method;
  char qualified[MAX_NAME];
  char *replacement;
  int written;

  if (method == NULL) {
    return;
  }

  if (method->isMain) {
    importError("an imported file (`%s._`) cannot define `main`", moduleName);
  }

  written =
      snprintf(qualified, sizeof(qualified), "%s.%s", moduleName, method->name);

  if (written < 0 || (size_t)written >= sizeof(qualified)) {
    importError(
        "qualified function name `%s.%s` exceeds the compiler's name limit",
        moduleName, method->name);
  }

  replacement = (char *)malloc(strlen(qualified) + 1);

  if (replacement == NULL) {
    importError("out of memory while qualifying an imported function");
  }

  strcpy(replacement, qualified);

  free(method->name);
  method->name = replacement;

  snprintf(node->name, sizeof(node->name), "%s", qualified);

  qualifyCalls(method->body, moduleName);
}

static ASTNode *expandProgram(ASTNode *program, const char *projectDirectory,
                              const char *executableDirectory,
                              const char *moduleName, int importedSource,
                              int depth);

static void loadModule(const char *root, const char *module,
                       const char *namespaceName, const char *projectDirectory,
                       const char *executableDirectory, ASTNode **first,
                       ASTNode **last, int depth);

static void loadManifestFolder(const char *folder, const char *projectDirectory,
                               const char *executableDirectory, ASTNode **first,
                               ASTNode **last, int depth) {
  char folderPath[IMPORT_PATH_SIZE];
  char manifestPath[IMPORT_PATH_SIZE];
  char manifestName[MAX_NAME + 8];
  FILE *manifest;
  char line[2048];
  int inFiles = 0;

  joinPath(folderPath, projectDirectory, folder);

  if (snprintf(manifestName, sizeof(manifestName), "%s.ufg", folder) >=
      (int)sizeof(manifestName)) {
    importError("manifest folder name `%s` is too long", folder);
  }

  joinPath(manifestPath, folderPath, manifestName);

  manifest = fopen(manifestPath, "r");

  if (manifest == NULL) {
    importError("could not open manifest `%s`", manifestPath);
  }

  while (fgets(line, sizeof(line), manifest) != NULL) {
    char *cursor = line;

    while (*cursor == ' ' || *cursor == '\t') {
      ++cursor;
    }

    if (*cursor == '?') {
      inFiles = strncmp(cursor, "?files", 6) == 0 &&
                (cursor[6] == '\0' || cursor[6] == '\r' || cursor[6] == '\n' ||
                 cursor[6] == ' ' || cursor[6] == '\t');
      continue;
    }

    if (!inFiles) {
      continue;
    }

    char *quote = strchr(cursor, '"');

    if (quote == NULL) {
      continue; /* Allow `null` and blank lines. */
    }

    char *endQuote = strchr(quote + 1, '"');

    if (endQuote == NULL) {
      fclose(manifest);
      importError("unterminated filename in `%s`", manifestPath);
    }

    *endQuote = '\0';

    const char *filename = quote + 1;
    const char *slash = strrchr(filename, '/');
    const char *backslash = strrchr(filename, '\\');

    if (slash != NULL || backslash != NULL) {
      fclose(manifest);
      importError(
          "manifest entries must be filenames in the package folder: `%s`",
          filename);
    }

    size_t length = strlen(filename);

    if (length < 3 || strcmp(filename + length - 2, "._") != 0) {
      fclose(manifest);
      importError("manifest file entry must end in `._`: `%s`", filename);
    }

    char module[MAX_NAME + 1];

    if (length - 2 >= sizeof(module)) {
      fclose(manifest);
      importError("module name in manifest is too long: `%s`", filename);
    }

    memcpy(module, filename, length - 2);
    module[length - 2] = '\0';

    /*
     * The physical module is loaded from the manifest's package folder.
     * Its public namespace remains the package name, not the filename.
     */
    loadModule(folder, module, folder, projectDirectory, executableDirectory,
               first, last, depth + 1);
  }

  fclose(manifest);
}

static void loadModule(const char *root, const char *module,
                       const char *namespaceName, const char *projectDirectory,
                       const char *executableDirectory, ASTNode **first,
                       ASTNode **last, int depth) {
  char path[IMPORT_PATH_SIZE];
  char *source;
  ASTNode *fragment;
  ASTNode *expanded;
  int recordIndex = -1;

  if (depth > MAX_IMPORT_DEPTH) {
    importError(
        "maximum import depth (%d) exceeded while loading `?%s with %s`",
        MAX_IMPORT_DEPTH, root, module);
  }

  if (!resolveModulePath(path, root, module, projectDirectory,
                         executableDirectory)) {
    importError("could not find `?%s` file `%s._` (looked for `%s`)", root,
                module, path);
  }

  for (int i = 0; i < importedFileCount; ++i) {
    if (strcmp(importedFiles[i].path, path) == 0) {
      if (importedFiles[i].state == 1) {
        importError("circular import detected while loading `%s`", path);
      }

      return; /* Already expanded; do not duplicate its declarations. */
    }
  }

  if (importedFileCount >= MAX_IMPORTED_FILES) {
    importError("too many imported files (limit %d)", MAX_IMPORTED_FILES);
  }

  recordIndex = importedFileCount++;

  copyPath(importedFiles[recordIndex].path, path);
  importedFiles[recordIndex].state = 1;

  source = readFile(path);
  diagnosticSetSource(path, source);
  lex(source);

  /*
   * IMPORTANT:
   * parseProgram() must allocate into an AST arena that remains alive
   * across parsing the entrypoint and subsequent imported modules.
   * See manual-edits/01_parser.md for the required parser-side change.
   */
  fragment = parseProgram();

  expanded = expandProgram(fragment, projectDirectory, executableDirectory,
                           namespaceName, 1, depth + 1);

  appendNodeChain(first, last, expanded);

  importedFiles[recordIndex].state = 2;

  free(source);
}

static ASTNode *expandProgram(ASTNode *program, const char *projectDirectory,
                              const char *executableDirectory,
                              const char *moduleName, int importedSource,
                              int depth) {
  ASTNode *first = NULL;
  ASTNode *last = NULL;

  if (program == NULL || program->type != AST_PROGRAM) {
    importError("internal error: import expansion expected a program AST");
  }

  for (ASTNode *statement = program->child; statement != NULL;) {
    ASTNode *next = statement->next;
    statement->next = NULL;

    if (statement->type == AST_INCLUDE) {
      if (strcmp(statement->name, "manifest") == 0) {
        /*
         * ?manifest logs
         *
         * Reads:
         *   <projectDirectory>/logs/logs.ufg
         *
         * Loads listed implementation files from:
         *   <projectDirectory>/logs/
         *
         * Public namespace:
         *   logs.functionName(...)
         */
        for (ASTNode *file = statement->child; file != NULL;
             file = file->next) {
          loadManifestFolder(file->name, projectDirectory, executableDirectory,
                             &first, &last, depth + 1);
        }
      } else {
        /*
         * Ordinary imports, including:
         *   ?std with math
         *
         * The module name is also the namespace unless the caller
         * explicitly supplies a different namespace.
         */
        for (ASTNode *file = statement->child; file != NULL;
             file = file->next) {
          loadModule(statement->name, file->name, file->name, projectDirectory,
                     executableDirectory, &first, &last, depth + 1);
        }
      }
    } else {
      if (containsImport(statement)) {
        importError(
            "library imports must appear at top level (line %d, column %d)",
            statement->line, statement->column);
      }

      if (importedSource && statement->type == AST_METHOD) {
        qualifyImportedMethod(statement, moduleName);
      }

      appendNode(&first, &last, statement);
    }

    statement = next;
  }

  program->child = first;

  return first;
}

ASTNode *expandImports(ASTNode *program, const char *sourceFile,
                       const char *executablePath) {
  char projectDirectory[IMPORT_PATH_SIZE];
  char executableDirectory[IMPORT_PATH_SIZE];

  importedFileCount = 0;
  memset(importedFiles, 0, sizeof(importedFiles));

  directoryOf(sourceFile, projectDirectory);
  directoryOf(executablePath, executableDirectory);

  (void)expandProgram(program, projectDirectory, executableDirectory, NULL, 0,
                      0);

  return program;
}