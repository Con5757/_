#include "codegen.h"

// helper
static const char *generateType(Type type) {
  switch (type.kind) {
  case TYPE_INT:
    return "int";
  case TYPE_FP:
    return "float";
  case TYPE_STR:
    return "char *";
  case TYPE_VOID:
    return "void";

  default:
    return "void";
  }
}
static void generateExpression(ASTNode *node, FILE *output) {
  switch (node->type) {

  case AST_NUM:
    if (node->valueType == VALUE_FP) {
      fprintf(output, "%f", node->fpValue);
    } else {
      fprintf(output, "%d", node->value);
    }
    break;

  case AST_VAR:
    fprintf(output, "%s", node->name);
    break;

  case AST_ADD:
    fprintf(output, "(");
    generateExpression(node->child, output);
    fprintf(output, " + ");
    generateExpression(node->right, output);
    fprintf(output, ")");
    break;

  case AST_SUB:
    fprintf(output, "(");
    generateExpression(node->child, output);
    fprintf(output, " - ");
    generateExpression(node->right, output);
    fprintf(output, ")");
    break;

  case AST_MUL:
    fprintf(output, "(");
    generateExpression(node->child, output);
    fprintf(output, " * ");
    generateExpression(node->right, output);
    fprintf(output, ")");
    break;

  case AST_DIV:
    fprintf(output, "(");
    generateExpression(node->child, output);
    fprintf(output, " / ");
    generateExpression(node->right, output);
    fprintf(output, ")");
    break;

  case AST_CALL: {
    fprintf(output, "%s(", node->name);

    ASTNode *arg = node->child;

    while (arg != NULL) {
      generateExpression(arg, output);

      if (arg->next != NULL) {
        fprintf(output, ", ");
      }

      arg = arg->next;
    }

    fprintf(output, ")");
    break;
  }

  default:
    break;
  }
}
static void generateRetreat(ASTNode *node, FILE *output) {
  fprintf(output, "\treturn ");
  generateExpression(node->child, output);
  fprintf(output, ";\n");
}
static void generateStatement(ASTNode *node, FILE *output) {
  if (node == NULL) {
    return;
  }

  switch (node->type) {

  case AST_INT_DECL:
    fprintf(output, "\tint %s = %d;\n", node->name, node->value);
    break;

  case AST_FP_DECL:
    fprintf(output, "\tfloat %s = %f;\n", node->name, node->fpValue);
    break;

  case AST_STR_DECL:
    fprintf(output, "\tchar %s[%d] = \"%s\";\n", node->name, node->typeSize + 1,
            node->strValue);
    break;

  case AST_OUT:
    generateOut(node, output);
    break;

  case AST_RETREAT:
    generateRetreat(node, output);
    break;

  default:
    break;
  }
}
static void generateMethod(Method *method, FILE *output) {
  fprintf(output, "%s %s(", generateType(method->returnType), method->name);

  for (int i = 0; i < method->paramCount; i++) {
    Param *param = &method->params[i];
    fprintf(output, "%s %s", generateType(param->type), param->name);

    if (i < method->paramCount - 1) {
      fprintf(output, ", ");
    }
  }

  fprintf(output, ")\n");
  fprintf(output, "{\n");

  ASTNode *statement = method->body;

  while (statement != NULL) {
    generateStatement(statement, output);
    statement = statement->next;
  }

  fprintf(output, "}\n\n");
}
static void generateOut(ASTNode *node, FILE *output) {
  ASTNode *expression = node->child;

  switch (expression->valueType) {

  case VALUE_INT:
    fprintf(output, "\tprintf(\"%%d\\n\", ");
    break;

  case VALUE_FP:
    fprintf(output, "\tprintf(\"%%f\\n\", ");
    break;

  case VALUE_STR:
    fprintf(output, "\tprintf(\"%%s\\n\", ");
    break;

  case VALUE_VOID:
    return;
  }

  generateExpression(expression, output);

  fprintf(output, ");\n");
}

void generateC(ASTNode *program, FILE *output) {
  fprintf(output, "#include <stdio.h>\n\n");

  ASTNode *node = program->child;
  while (node != NULL) {
    if (node->type == AST_METHOD) {
      generateMethod(node->method, output);
    }

    node = node->next;
  }

  fprintf(output, "int main(void)\n");
  fprintf(output, "{\n");

  node = program->child;

  while (node != NULL) {
    if (node->type != AST_METHOD) {
      generateStatement(node, output);
    }

    node = node->next;
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
#if DEBUG_OBJS
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
#endif
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
#if DEBUG_OBJS
  printf("\n\n");
  printf("// ========================================\n");
  printf("// COMPILED OUTPUT\n");
  printf("// ========================================\n\n");
#endif
  char runCommand[MAX_NAME + 40];

  snprintf(runCommand, sizeof(runCommand), "\"%s\"", exeFile);

  system(runCommand);
  clearProgress();
}