#include "semantic.h"
#include "../global.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char variables[MAX_AST_NODES][MAX_NAME];
int varCount = 0;

int varExists(const char *name) {
  for (int i = 0; i < varCount; i++) {
    if (strcmp(variables[i], name) == 0) {
      return 1;
    }
  }

  return 0;
}
void semanticError(const char *message, const char *name) {
  printf("Semantic error: %s '%s'\n", message, name);
  exit(1);
}
void checkProgram(ASTNode *program) {
  ASTNode *node = program->child;

  while (node != NULL) {
    if (node->type == AST_INT_DECL) {
      if (varExists(node->name)) {
        semanticError("variable already exists", node->name);
      }
      strcpy(variables[varCount++], node->name);
    } else if (node->type == AST_OUT) {
      if (!varExists(node->name)) {
        semanticError("undefined var", node->name);
      }
    } else if (node->type == AST_FP_DECL) {
      if (varExists(node->name)) {
        semanticError("variable already exists", node->name);
      }

      strcpy(variables[varCount++], node->name);
    } else if (node->type == AST_STR_DECL) {
      if (varExists(node->name)) {
        semanticError("variable already exists", node->name);
      }

      strcpy(variables[varCount++], node->name);
    }

    node = node->child;
  }
}