#include "codegen.h"
#include "../global.h"
#include <Windows.h>
#include <stdio.h>

void generateC(ASTNode *program, FILE *output) {
  fprintf(output, "#include <stdio.h>\n\n");
  fprintf(output, "int main(void)\n");
  fprintf(output, "{\n");

  ASTNode *node = program->child;

  while (node != NULL) {
    if (node->type == AST_INT_DECL) {
      fprintf(output, "\tint %s = %d;\n", node->name, node->value);
    } else if (node->type == AST_OUT) {
      fprintf(output, "\tprintf(\"%%d\\n\", %s);\n", node->name);
    }

    node = node->child;
  }

  fprintf(output, "\treturn 0;\n");
  fprintf(output, "}\n");
}
void compileProgram(ASTNode *program, const char *sourceFile) {
  char baseName[MAX_NAME];
  char cFile[MAX_NAME + 20];
  char exeFile[MAX_NAME + 30];

  strncpy(baseName, sourceFile, MAX_NAME - 1);
  baseName[MAX_NAME - 1] = '\0';

  char *extension = strrchr(baseName, '.');

  if (extension != NULL)
    *extension = '\0';

  snprintf(cFile, sizeof(cFile), "%s-obj.c", baseName);
  snprintf(exeFile, sizeof(exeFile), "%s-compiled.exe", baseName);

  progress("Generating C");

  FILE *output = fopen(cFile, "w");

  if (output == NULL) {
    printf("Could not create '%s'\n", cFile);
    exit(1);
  }

  generateC(program, output);

  fclose(output);

  printf("\n");
  printf("// ========================================\n");
  printf("// C OBJ\n");
  printf("// ========================================\n\n");

  FILE *generated = fopen(cFile, "r");

  if (generated == NULL) {
    printf("Could not read '%s'\n", cFile);
    exit(1);
  }

  char line[512];

  while (fgets(line, sizeof(line), generated) != NULL) {
    printf("%s", line);
  }

  fclose(generated);

  // Compile the generated C using GCC.
  progress("Compiling");

  char command[512];

  snprintf(command, sizeof(command), "gcc \"%s\" -o \"%s\"", cFile, exeFile);

  int result = system(command);

  if (result != 0) {
    printf("\nGCC compilation failed.\n");
    exit(1);
  }

  progress("Running");

  printf("\n");
  printf("// ========================================\n");
  printf("// COMPILED OUTPUT\n");
  printf("// ========================================\n\n");

  printf("debug reached 1");

  char runCommand[MAX_NAME + 40];

  printf("debug reached 2");

  snprintf(runCommand, sizeof(runCommand), "\"%s\"", exeFile);

  printf("debug reached 3");

  system(runCommand);

  printf("debug reached 4");

  printf("\n");

  printf("debug reached 5");
}