#ifndef PARSE_H
#define PARSE_H

#include "../global.h"
#include "../lex/lex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum ASTType {
  AST_PROGRAM,
  AST_BLOCK,

  /* Declarations and definitions. */
  AST_INT_DECL,
  AST_FP_DECL,
  AST_STR_DECL,
  AST_DECL,
  AST_ARRAY_DECL,
  AST_VEC_DECL,
  AST_DATA,
  AST_SET,
  AST_ALIAS,
  AST_CLASS,
  AST_INCLUDE,

  /* Expressions. Keep the original node names for semantic/codegen users. */
  AST_NUM,
  AST_STRING,
  AST_BOOL,
  AST_VAR,
  AST_MEMBER,
  AST_NAMESPACE_ACCESS,
  AST_INDEX,
  AST_UNARY,
  AST_ASSIGN,
  AST_UPDATE,
  AST_OUT,
  AST_IN,
  AST_CALL,
  AST_RETREAT,
  AST_KILL,
  AST_METHOD,

  AST_ADD,
  AST_SUB,
  AST_MUL,
  AST_DIV,
  AST_MOD,
  AST_SHIFT_LEFT,
  AST_SHIFT_RIGHT,
  AST_SPECIAL_BITWISE,
  AST_BIT_OR,
  AST_BIT_AND,
  AST_BIT_XOR,
  AST_LOGICAL_NOT,
  AST_LOGICAL_AND,
  AST_LOGICAL_OR,
  AST_LOGICAL_NOR,
  AST_LOGICAL_XOR,
  AST_LOGICAL_XNOR,
  AST_LOGICAL_NAND,
  AST_EQUAL,
  AST_NOT_EQUAL,
  AST_LESS,
  AST_GREATER,
  AST_LESS_EQUAL,
  AST_GREATER_EQUAL,

  /* Statements and control flow. */
  AST_IF,
  AST_SWITCH,
  AST_CASE,
  AST_BREAK,
  AST_CONT,
  AST_DURING,
  AST_REPEAT,
  AST_ATTEMPT,
  AST_EXPRESSION_STATEMENT
} ASTType;

typedef enum ValueType {
  VALUE_INT,
  VALUE_FP,
  VALUE_STR,
  VALUE_VOID
} ValueType;

typedef enum TypeKind {
  TYPE_INT,
  TYPE_FP,
  TYPE_STR,
  TYPE_VOID
} TypeKind;

typedef struct Type {
  TypeKind kind;
  int width; /* 0 means the language's default width. */
} Type;

typedef struct ASTNode ASTNode;

typedef struct Param {
  char *name;
  Type type;
  int storageFlags;
  int line;
  int column;
} Param;

typedef struct Method {
  Type returnType;
  char *name;
  Param *params;
  int paramCount;
  Type processType; /* Retained for compatibility with the existing compiler. */
  ASTNode *body;    /* Head of the method's statement list. */
  char flags[MAX_NAME];
  int isMain;
} Method;

enum {
  DECL_FLAG_CONST  = 1 << 0,
  DECL_FLAG_STATIC = 1 << 1,
  DECL_FLAG_GLOBAL = 1 << 2,
  DECL_FLAG_PUBLIC = 1 << 3,
  DECL_FLAG_PRIVATE = 1 << 4,
  DECL_FLAG_HIDDEN = 1 << 5,
  DECL_FLAG_POINTER = 1 << 6,
  DECL_FLAG_DOUBLE_POINTER = 1 << 7,
  DECL_FLAG_STRING_BUFFER = 1 << 8
};

struct ASTNode {
  ASTType type;
  int line;
  int column;
  char name[MAX_NAME];
  char op[4];
  char flags[MAX_NAME];

  ValueType valueType;
  Type declaredType;
  int storageFlags;
  int typeSize;
  int arrayCount;
  long long value;
  double fpValue;
  char strValue[MAX_NAME];

  /* child/right represent the conventional first/second operand.
     third is used for constructs with a third branch or clause.
     next links sibling statements, arguments, array items, and cases. */
  ASTNode *child;
  ASTNode *right;
  ASTNode *third;
  ASTNode *body;
  ASTNode *next;

  Method *method;
  isTokenType operatorType;
};

extern int currentToken;
extern int astCount;
extern ASTNode astNodes[MAX_AST_NODES];

ASTNode *newAST(ASTType type);
Token *current(void);
int match(isTokenType type);
void parserError(const char *message);
Token *expect(isTokenType type);

/* Statements and declarations. */
ASTNode *parseOut(void);
ASTNode *parseIntDecl(void);
ASTNode *parseFpDecl(void);
ASTNode *parseStrDecl(void);
ASTNode *parseMethod(void);
ASTNode *parseRetreat(void);
ASTNode *parseStatement(void);
ASTNode *parseProgram(void);

/* Expressions and parser helpers retained as public entry points. */
int isMethod(void);
ASTNode *parsePrimary(void);
ASTNode *parseTerm(void);
ASTNode *parseExpression(void);
ASTNode *parseCall(void);

#endif
