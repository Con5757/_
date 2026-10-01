#ifndef PARSE_H
#define PARSE_H

#include "../global.h"
#include "../lex/lex.h"

typedef enum {
  AST_PROGRAM,
  AST_INT_DECL,
  AST_FP_DECL,
  AST_STR_DECL,
  AST_NUM,
  AST_VAR,
  AST_OUT,
} ASTType;

typedef struct ASTNode {
  ASTType type;
  char name[MAX_NAME];

  int value;
  struct ASTNode *child;
} ASTNode;

extern int currentToken;

ASTNode *newAST(ASTType type);
Token *current(void);
int match(isTokenType type);
void parserError(const char *message);
Token *expect(isTokenType type);

// setups
ASTNode *parseOut();
ASTNode *parseIntDecl(void);

// parse
ASTNode *parseProgram(void);
ASTNode *parseStatement(void);

#endif