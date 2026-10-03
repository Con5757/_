#ifndef LEX_H
#define LEX_H

#include "../global.h"

typedef enum {
  TOKEN_EOF,
  TOKEN_NEWLINE,
  TOKEN_COMMA,
  TOKEN_MAIN,

  // includes
  TOKEN_WITH,
  TOKEN_INCLUDE,

  // kill flags
  TOKEN_KILL_SILENT,

  TOKEN_VARIABLE,
  TOKEN_STRING,
  TOKEN_NUMBER,

  // types
  TOKEN_INT,
  TOKEN_STR,
  TOKEN_FP,
  TOKEN_VOID,

  // io (move to lib and runtime later)
  TOKEN_IN,
  TOKEN_OUT,

  // assign
  TOKEN_EQUAL,

  // period
  TOKEN_PERIOD,

  // logic
  NOT,
  AND,
  OR,
  NOR,
  XOR,
  XNOR,
  NAND,

  // operators
  TOKEN_ADD,
  TOKEN_SUB,
  TOKEN_MUL,
  TOKEN_DIV,

  // func
  TOKEN_FUNC,
  TOKEN_KILL,
  TOKEN_RETREAT,

  TOKEN_SINGLE_QUOTE,

  // parens
  TOKEN_LEFT_PAREN,         // (
  TOKEN_RIGHT_PAREN,        // )
  TOKEN_LEFT_SQUARE_PAREN,  // [
  TOKEN_RIGHT_SQUARE_PAREN, // ]
  TOKEN_LEFT_CURLY_PAREN,   // {
  TOKEN_RIGHT_CURLY_PAREN,  // }
  TOKEN_LEFT_VEC_PAREN,     // <
  TOKEN_RIGHT_VEC_PAREN     // >
} isTokenType;

typedef struct {
  isTokenType type;
  char text[MAX_NAME];
  int line;
} Token;

extern Token tokens[MAX_TOKENS];
extern int tokenCount;

// functions
void addToken(isTokenType type, const char *text, int line);
void lex(const char *source);

#endif