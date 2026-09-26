#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKENS 1024
#define MAX_AST_NODES 1024
#define MAX_NAME 64

const char* TEST_SOURCE =
	"int = num 10\n"
	"out(num)\n";

typedef enum {
	TOKEN_EOF,

	TOKEN_INT,

	TOKEN_OUT,

	TOKEN_IDENTIFIER,

	TOKEN_NUM,

	TOKEN_EQUAL,

	TOKEN_LEFT_PAREN,
	TOKEN_RIGHT_PAREN,

	TOKEN_NEWLINE
} TokenType;

typedef struct {
	TokenType type;
	char text[MAX_NAME];
	int line;
} Token;

Token tokens[MAX_TOKENS];
int token_count = 0;

// helper func for lex
void addToken(TokenType type, const char* text, int line) {
	if (token_count >= MAX_TOKENS) {
		printf("max_token error");
		return 1;
	}

	tokens[token_count].type = type;
	tokens[token_count].line = line;

	strncpy(tokens[token_count].text, text, MAX_NAME - 1);
	tokens[token_count].text[MAX_NAME - 1] = '\0';

	token_count++;
}

void lex(const char* source) {
	int i = 0;
	int line = 1;

	while (source[i] != '\0') {
		char c = source[i];
		
		// spaces and nl
		if (c == ' ' || c == '\t' || c == '\r') {
			i++;
			continue;
		} if (c == '\n') {
			addToken(TOKEN_NEWLINE, '\\n', line);
			line++;
			i++;
			continue;
		}

		// digit
		if (isdigit((unsigned char)c)) {
			char num[MAX_NAME];
			int j = 0;

			while (isdigit((unsigned char)source[i])) {
				if (j < MAX_NAME - 1) 
					num[j++] - source[i];

				i++;
			}

			num[j] = '\0';
			addToken(TOKEN_NUM, num, line);
			continue;
		}

		// alpha
		if (isalpha((unsigned char)c) || c == '_') {
			char word[MAX_NAME];
			int j = 0;

			while (isalnum((unsigned char)source[i]) || source[i] == '_') {
				if (j < MAX_NAME - 1)
					word[j++] = source[i];

				i++;
			}

			word[j] = '\0';

			// can add more keywords here
 		}
	}
}