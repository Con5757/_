#include "parse.h"
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

// structures
ASTNode *parseIntDecl(void) { // int num = #
  expect(TOKEN_INT);
  Token *name = expect(TOKEN_VARIABLE);
  expect(TOKEN_EQUAL);
  Token *number = expect(TOKEN_NUMBER);

  ASTNode *node = newAST(AST_INT_DECL);
  strcpy(node->name, name->text);
  node->value = atoi(number->text);

  return node;
}
ASTNode *parseOut() {
  expect(TOKEN_OUT);
  expect(TOKEN_LEFT_PAREN);

  Token *name = expect(TOKEN_VARIABLE);

  expect(TOKEN_RIGHT_PAREN);

  ASTNode *node = newAST(AST_OUT);
  strcpy(node->name, name->text);

  return node;
}

// parse setup
ASTNode *parseStatement(void) {
  if (current()->type == TOKEN_INT) {
    return parseIntDecl();
  }
  if (current()->type == TOKEN_OUT) {
    return parseOut();
  }
  parserError("unknown statement");

  return NULL;
}
ASTNode *parseProgram(void) {
  ASTNode *program = newAST(AST_PROGRAM);

  program->child = NULL;

  ASTNode *first = NULL;
  ASTNode *last = NULL;

  while (current()->type != TOKEN_EOF) {
    if (match(TOKEN_NEWLINE)) {
      continue;
    }

    ASTNode *statement = parseStatement();

    if (first == NULL) {
      first = statement;
    } else {
      last->child = statement;
    }
    last = statement;

    if (current()->type == TOKEN_NEWLINE) {
      match(TOKEN_NEWLINE);
    } else if (current()->type != TOKEN_EOF) {
      parserError("expected end of line");
    }
  }

  program->child = first;

  return program;
}