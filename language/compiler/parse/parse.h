#ifndef PARSE_H
#define PARSE_H

#include "../global.h"
#include "../lex/lex.h"

typedef enum {
  AST_PROGRAM,
  AST_INT_DECL,
  AST_NUM,
  AST_VAR,
  AST_OUT,
} ASTType;

typedef enum { TYPE_INT, TYPE_FP, TYPE_STR } ValueType;

typedef struct ASTNode {
  ASTType type;
  char name[MAX_NAME];

  ValueType valueType;
  int typeSize;

  int value;
  struct ASTNode *child;
} ASTNode;

extern int currentToken;

ASTNode *newAST(ASTType type);
Token *current(void);
int match(isTokenType type);
void parserError(const char *message);
Token *expect(isTokenType type);

// parse
ASTNode *parseProgram(void);
ASTNode *parseStatement(void);

// setups
ASTNode *parseOut();
ASTNode *parseIntDecl(void);

#endif