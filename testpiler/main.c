#include <stdio.h>
#include <ctype.h>

typedef enum {
	TOKEN_INT,
	TOKEN_IDENTIFIER,
	TOKEN_EQUAL,
	TOKEN_NUMBER
} TokenType;

typedef struct {
	TokenType Type;
	char* value;
} Token;

Token lex(char* inputChar) {
	Token token;


}

int main() {
	char text[256] = "int x = 20";

	Token tokens[100];
	int tokenCount = 0;

	int i = 0;
	while (text[i] != '\0') {
		if (isspace(text[i])) {
			i++;
			continue;
		}

		Token token = lex(text[i]);

		tokens[tokenCount] = token;
		tokenCount++;
	}

	return 0;
}