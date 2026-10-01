#include "codegen.h"
#include "../global.h"
#include <Windows.h>
#include <stdio.h>
#include <string.h>

void generateC(ASTNode *program, FILE *output) {
  fprintf(output, "#include <stdio.h>\n\n");
  fprintf(output, "int main(void)\n");
  fprintf(output, "{\n");

  ValueType variableTypes[MAX_AST_NODES];
  char variableNames[MAX_AST_NODES][MAX_NAME];
  int variableCount = 0;
  ASTNode *node = program->child;

  while (node != NULL) {
    if (node->type == AST_INT_DECL) {
      fprintf(output, "\tint %s = %d;\n", node->name, node->value);

      strcpy(variableNames[variableCount], node->name);
      variableTypes[variableCount] = VALUE_INT;
      variableCount++;

    } else if (node->type == AST_FP_DECL) {
      fprintf(output, "\tfloat %s = %f;\n", node->name, node->fpValue);

      strcpy(variableNames[variableCount], node->name);
      variableTypes[variableCount] = VALUE_FP;
      variableCount++;
    }

    else if (node->type == AST_OUT) {
      for (int i = 0; i < variableCount; i++) {
        if (strcmp(variableNames[i], node->name) == 0) {

          if (variableTypes[i] == VALUE_INT) {
            fprintf(output, "\tprintf(\"%%d\\n\", %s);\n", node->name);
          } else if (variableTypes[i] == VALUE_FP) {
            fprintf(output, "\tprintf(\"%%f\\n\", %s);\n", node->name);
          }

          break;
        }
      }
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

  char runCommand[MAX_NAME + 40];

  snprintf(runCommand, sizeof(runCommand), "\"%s\"", exeFile);

  system(runCommand);

  printf("\n");
}