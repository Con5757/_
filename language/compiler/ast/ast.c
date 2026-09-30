#include "ast.h"
#include "../global.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ASTNode astNodes[MAX_AST_NODES];
int astCount = 0;

ASTNode *newAST(ASTType type) {
  if (astCount >= MAX_AST_NODES) {
    printf("error, ast is too large\n");
    exit(1);
  }

  ASTNode *node = &astNodes[astCount++];
  memset(node, 0, sizeof(ASTNode));
  node->type = type;

  return node;
}

int currentToken = 0;
Token *current(void) { return &tokens[currentToken]; }

int match(isTokenType type) {
  if (current()->type == type) {
    currentToken++;
    return 1;
  }

  return 0;
}
void parserError(const char *message) {
  printf("parser error on line %d: %s\n", current()->line, message);
  exit(1);
}
Token *expect(isTokenType type) {
  if (current()->type != type) {
    parserError("unexpected token");
  }

  return &tokens[currentToken++];
}