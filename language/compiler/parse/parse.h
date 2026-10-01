#ifndef PARSE_H
#define PARSE_H

#include "../global.h"
#include "../lex/lex.h"

typedef enum ASTType {
  AST_PROGRAM,
  AST_INT_DECL,
  AST_FP_DECL,
  AST_STR_DECL,
  AST_NUM,
  AST_VAR,
  AST_OUT,
} ASTType;

typedef enum ValueType { VALUE_INT, VALUE_FP, VALUE_STR } ValueType;

typedef enum {
  TYPE_INT,
  TYPE_FLOAT,
  TYPE_STR,
  TYPE_VOID,
} TypeKind;

typedef struct {
  TypeKind kind;
} Type;

typedef struct ASTNode {
  ASTType type;
  char name[MAX_NAME];

  ValueType valueType;
  int typeSize;
  int value;
  double fpValue;
  char strValue[MAX_NAME];

  struct ASTNode *child;
} ASTNode;

// method structs
typedef struct Param {
  char *name;
  Type type; // check later if tokenType is int, fp, str, void
} Param;

typedef struct Method {
  Type returnType;
  char *name;

  Param *params;
  int paramCount;

  ASTNode *body;
} Method;

extern int currentToken;

ASTNode *newAST(ASTType type);
Token *current(void);
int match(isTokenType type);
void parserError(const char *message);
Token *expect(isTokenType type);

// setups
ASTNode *parseOut();
ASTNode *parseIntDecl(void);
ASTNode *parseFpDecl();

// parse
ASTNode *parseProgram(void);
ASTNode *parseStatement(void);

#endif