#include "codegen/codegen.h"
#include "config/manifest.h"
#include "diagnostic/diagnostic.h"
#include "global.h"
#include "imports/imports.h"
#include "lex/lex.h"
#include "parse/parse.h"
#include "semantic/semantic.h"

int debugTokens = 0;
int debugObjs = 0;
int debugProgress = 0;
static int checkOnly = 0;

char *readFile(const char *filename) {
  FILE *file = fopen(filename, "rb");
  long size;
  char *buffer;
  size_t bytesRead;

  if (file == NULL) {
    fprintf(stderr, "could not open '%s'\n", filename);
    exit(EXIT_FAILURE);
  }
  if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
      fseek(file, 0, SEEK_SET) != 0) {
    fprintf(stderr, "could not determine source size for '%s'\n", filename);
    fclose(file);
    exit(EXIT_FAILURE);
  }

  buffer = (char *)malloc((size_t)size + 1);
  if (buffer == NULL) {
    fprintf(stderr, "could not allocate memory for '%s'\n", filename);
    fclose(file);
    exit(EXIT_FAILURE);
  }

  bytesRead = fread(buffer, 1, (size_t)size, file);
  if (bytesRead != (size_t)size) {
    fprintf(stderr, "could not read source file '%s'\n", filename);
    free(buffer);
    fclose(file);
    exit(EXIT_FAILURE);
  }
  buffer[bytesRead] = '\0';
  fclose(file);
  return buffer;
}

void progress(const char *message) {
  if (!debugProgress) return;
  const int width = 30;
  for (int percent = 0; percent <= 100; percent++) {
    int filled = (percent * width) / 100;
    printf("\r[");
    for (int i = 0; i < width; i++) printf("%c", i < filled ? '#' : '-');
    printf("] %3d%% %s", percent, message);
    fflush(stdout);
  }
}

void clearProgress(void) {
  if (!debugProgress) return;
  printf("\r%*s\r", 80, "");
  fflush(stdout);
}

void printTokens(void) {
  for (int i = 0; i < tokenCount; i++) {
    printf("TOKEN: %-15d TEXT: %s\n", tokens[i].type, tokens[i].text);
  }
}

void printHelp(void) {
  printf("Usage: compiler <source file> [flags]\n\n");
  printf("Flags:\n");
  printf("  -tokens       Print lexer tokens\n");
  printf("  -objs         Print generated C\n");
  printf("  -progress     Show compiler progress\n");
  printf("  -check        Check syntax/imports/types without generating or running C\n");
  printf("  -help         Show this help\n");
  printf("  -cufg         Create a UFG manifest beside the source file\n");
  printf("  -cufg--p PATH Create a UFG manifest inside an existing directory\n");
  printf("\nSet UNDERSCORE_STDLIB to the directory containing std modules when the\n");
  printf("standard library is not located beside the compiler.\n");
}

void parseFlags(int argc, char **argv) {
  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], "-tokens") == 0) {
      debugTokens = 1;
    } else if (strcmp(argv[i], "-objs") == 0) {
      debugObjs = 1;
    } else if (strcmp(argv[i], "-progress") == 0) {
      debugProgress = 1;
    } else if (strcmp(argv[i], "-check") == 0) {
      checkOnly = 1;
    } else if (strcmp(argv[i], "-help") == 0) {
      printHelp();
      exit(EXIT_SUCCESS);
    } else {
      fprintf(stderr, "unknown flag '%s'\n", argv[i]);
      exit(EXIT_FAILURE);
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: compiler <source file> [flags]\n");
    return EXIT_FAILURE;
  }

  /* Handle manifest creation before normal compilation flags. */
  for (int i = 2; i < argc; ++i) {
    if (strcmp(argv[i], "-cufg") == 0) {
      return manifestCreate(argv[1], NULL, 0);
    }
    if (strcmp(argv[i], "-cufg--p") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr,
                "usage: compiler <source file> -cufg--p <existing-directory>\n");
        return 2;
      }
      return manifestCreate(argv[1], argv[i + 1], 1);
    }
  }

  parseFlags(argc, argv);
  char *source = readFile(argv[1]);
  diagnosticSetSource(argv[1], source);

  progress("Lexing");
  lex(source);
  if (debugTokens) {
    clearProgress();
    printf("\n");
    printTokens();
  }

  progress("Parsing");
  currentToken = 0;
  ASTNode *program = parseProgram();

  /* Imports share the parser's long-lived AST arena. Keep diagnostics pointed
     at the entry source after expansion; import source buffers are temporary. */
  program = expandImports(program, argv[1], argv[0]);
  diagnosticSetSource(argv[1], source);

  progress("Semantic analysis");
  checkProgram(program);

  if (checkOnly) {
    free(source);
    clearProgress();
    return EXIT_SUCCESS;
  }

  compileProgram(program, argv[1]);
  free(source);
  return EXIT_SUCCESS;
}
