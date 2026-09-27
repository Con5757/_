#ifndef LEX_H
#define LEX_H

#include "../global.h"

typedef enum {
	TOKEN_EOF,
	TOKEN_NEWLINE,

	TOKEN_VARIABLE,
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

	// operators
	TOKEN_ADD,
	TOKEN_SUB,
	TOKEN_MUL,
	TOKEN_DIV,
	
	// parens
	TOKEN_LEFT_PAREN, // (
	TOKEN_RIGHT_PAREN, // )
	TOKEN_LEFT_SQUARE_PAREN, // [
	TOKEN_RIGHT_SQUARE_PAREN, // ]
	TOKEN_LEFT_CURLY_PAREN, // {
	TOKEN_RIGHT_CURLY_PAREN // }

} isTokenType;

typedef struct {
	isTokenType type;
	char text[MAX_NAME];
	int line;
} Token; 

// functions
void addToken(isTokenType type, const char* text, int line);
void lex(const char* source);

#endif