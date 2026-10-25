#include "diagnostic.h"

static const char *diagnosticFilename = "<source>";
static const char *diagnosticSource = NULL;

void diagnosticSetSource(const char *filename, const char *source) {
  diagnosticFilename = filename != NULL ? filename : "<source>";
  diagnosticSource = source;
}

static const char *lineStartFor(int targetLine) {
  const char *cursor = diagnosticSource;
  int line = 1;
  if (cursor == NULL || targetLine < 1)
    return NULL;
  while (*cursor != '\0' && line < targetLine) {
    if (*cursor++ == '\n')
      line++;
  }
  return line == targetLine ? cursor : NULL;
}

static void printSourceContext(int line, int column, const char *label) {
  if (diagnosticSource == NULL || line < 1 || column < 1)
    return;
  const char *start = lineStartFor(line);
  if (start == NULL)
    return;
  const char *end = start;
  while (*end != '\0' && *end != '\n' && *end != '\r')
    end++;

  size_t length = (size_t)(end - start);
  if (length > 240)
    length = 240;
  char sourceLine[241];
  memcpy(sourceLine, start, length);
  sourceLine[length] = '\0';

  int digits = 1;
  for (int value = line; value >= 10; value /= 10)
    digits++;
  fprintf(stderr, " %*s |\n", digits, "");
  fprintf(stderr, "%*d | %s\n", digits, line, sourceLine);
  fprintf(stderr, " %*s | ", digits, "");

  int caretColumn = column;
  if (caretColumn > (int)length + 1)
    caretColumn = (int)length + 1;
  for (int i = 1; i < caretColumn; i++) {
    fputc(i <= (int)length && sourceLine[i - 1] == '\t' ? '\t' : ' ', stderr);
  }
  fputc('^', stderr);
  if (label != NULL && label[0] != '\0')
    fprintf(stderr, " %s", label);
  fputc('\n', stderr);
}

static void printHeaderAndContext(const char *phase, const char *code,
                                  const char *message, int line, int column,
                                  const char *label) {
  fprintf(stderr, "%s[%s]: %s\n", phase != NULL ? phase : "error",
          code != NULL ? code : "E0000",
          message != NULL ? message : "compilation failed");
  if (line > 0) {
    fprintf(stderr, "  --> %s:%d:%d\n", diagnosticFilename, line,
            column > 0 ? column : 1);
    printSourceContext(line, column > 0 ? column : 1, label);
  }
}

_Noreturn void diagnosticError(const char *phase, const char *code,
                               const char *message, int line, int column,
                               const char *label, const char *help) {
  printHeaderAndContext(phase, code, message, line, column, label);
  if (help != NULL && help[0] != '\0')
    fprintf(stderr, "\n help: %s\n", help);
  exit(EXIT_FAILURE);
}

_Noreturn void diagnosticErrorWithNote(const char *phase, const char *code,
                                       const char *message, int line, int column,
                                       const char *label, const char *help,
                                       const char *note, int noteLine,
                                       int noteColumn) {
  printHeaderAndContext(phase, code, message, line, column, label);
  if (note != NULL && note[0] != '\0') {
    fprintf(stderr, "\n note: %s\n", note);
    if (noteLine > 0) {
      fprintf(stderr, "  --> %s:%d:%d\n", diagnosticFilename, noteLine,
              noteColumn > 0 ? noteColumn : 1);
      printSourceContext(noteLine, noteColumn > 0 ? noteColumn : 1,
                         "previous declaration is here");
    }
  }
  if (help != NULL && help[0] != '\0')
    fprintf(stderr, "\n help: %s\n", help);
  exit(EXIT_FAILURE);
}
