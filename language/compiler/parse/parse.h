#ifndef PARSE_H
#define PARSE_H

#include "../global.h"
#include "../lex/lex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum ASTType {
  AST_PROGRAM,

  AST_INT_DECL,
  AST_FP_DECL,
  AST_STR_DECL,

  AST_NUM,
  AST_VAR,
  AST_OUT,

  AST_RETREAT,
  AST_METHOD,

  // arit
  AST_ADD,
  AST_SUB,
  AST_MUL,
  AST_DIV,
  AST_CALL
} ASTType;

typedef enum ValueType { VALUE_INT, VALUE_FP, VALUE_STR, VALUE_VOID } ValueType;

typedef enum TypeKind {
  TYPE_INT,
  TYPE_FP,
  TYPE_STR,
  TYPE_VOID,
} TypeKind;

typedef struct {
  TypeKind kind;
} Type;

typedef struct ASTNode ASTNode;

typedef struct Param {
  char *name;
  Type type;
} Param;

typedef struct Method {
  Type returnType;
  char *name;

  Param *params;
  int paramCount;

  Type processType; // temporary; eventually flesh this out: vectorparams.md

  ASTNode *body;
} Method;

typedef struct ASTNode {
  ASTType type;
  char name[MAX_NAME];

  ValueType valueType;
  int typeSize;

  int value;
  double fpValue;
  char strValue[MAX_NAME];

  struct ASTNode *child;
  struct ASTNode *right;
  struct ASTNode *next;

  Method *method;
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
ASTNode *parseFpDecl();
ASTNode *parseStrDecl();

ASTNode *parseMethod(void);
ASTNode *parseRetreat(void);

// parse
int isMethod(void);
ASTNode *parsePrimary(void);
ASTNode *parseTerm(void);
ASTNode *parseExpression(void);
ASTNode *parseProgram(void);
ASTNode *parseStatement(void);

#endif