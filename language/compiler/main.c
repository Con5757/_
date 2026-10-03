#include "codegen/codegen.h"
#include "global.h"
#include "lex/lex.h"
#include "parse/parse.h"
#include "semantic/semantic.h"

int debugTokens = 0;
int debugObjs = 0;
int debugProgress = 0;

char *readFile(const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    printf("could not open '%s'\n", filename);
    exit(1);
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *buffer = malloc(size + 1);
  if (buffer == NULL) {
    printf("couldnt allocated source memory");
    fclose(file);
    exit(1);
  }

  fread(buffer, 1, size, file);
  buffer[size] = '\0';

  fclose(file);
  return buffer;
}
void progress(const char *message) {
#if DEBUG_PROGRESS
  int width = 30;

  for (int percent = 0; percent <= 100; percent++) {
    int filled = (percent * width) / 100;

    printf("\r[");

    for (int i = 0; i < width; i++) {
      if (i < filled)
        printf("#");
      else
        printf("-");
    }

    printf("] %3d%% %s", percent, message);

    fflush(stdout);
  }
#endif
}
void clearProgress(void) {
#if DEBUG_PROGRESS
  printf("\r%*s\r", 80, "");
  fflush(stdout);
#endif
}
void printTokens() {
  for (int i = 0; i < tokenCount; i++) {
    printf("TOKEN: %-15d TEXT: %s\n", tokens[i].type, tokens[i].text);
  }
}
void parseFlags(int argc, char **argv) {
  for (int i = 2; i < argc; i++) {

    if (strcmp(argv[i], "--tokens") == 0) {
      debugTokens = 1;
    }

    else if (strcmp(argv[i], "--objs") == 0) {
      debugObjs = 1;
    }

    else if (strcmp(argv[i], "--progress") == 0) {
      debugProgress = 1;
    }

    else if (strcmp(argv[i], "--help") == 0) {
      printf("Usage: compiler <source file> [flags]\n\n");
      printf("Flags:\n");
      printf("  --tokens       Print lexer tokens\n");
      printf("  --objs         Print generated C\n");
      printf("  --progress     Show compiler progress\n");
      printf("  --help         Show this help\n");
      exit(0);
    }

    else {
      printf("unknown flag '%s'\n", argv[i]);
      exit(1);
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("usage: compiler <source file>\n");
    return 1;
  }

  char *source = readFile(argv[1]);

  progress("Lexing");
  lex(source);

#if DEBUG_TOKENS
  clearProgress();
  printTokens();
#endif

  progress("Parsing");
  currentToken = 0;
  ASTNode *program = parseProgram();

  progress("Semantic analysis");
  checkProgram(program);

  compileProgram(program, argv[1]);

  free(source);
  return 0;
}