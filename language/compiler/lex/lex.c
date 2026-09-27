#include "lex.h"
#include "../global.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

Token tokens[MAX_TOKENS];
int tokenCount = 0;

void addToken(isTokenType type, const char *text, int line) {
    if (tokenCount >= MAX_TOKENS) {
        printf("max_token error");
    }

    tokens[tokenCount].type = type;
    tokens[tokenCount].line = line;

    strncpy(tokens[tokenCount].text, text, MAX_NAME - 1);
    tokens[tokenCount].text[MAX_NAME - 1] = '\0';

    tokenCount++;
}

void lex(const char *source) {
    int i = 0;
    int line = 1;

    while (source[i] != '\0') {
        char c = source[i];

        if (c == ' ' || c == '\t' || c == '\r') {
            i++;
            continue;
        }
        if (c == '\n') {
            addToken(TOKEN_NEWLINE, "\n", line);
            line++;
            i++;
            continue;
        }

        if (isdigit((unsigned char)c)) {
            char num[MAX_NAME];
            int j = 0;

            while (isdigit((unsigned char)source[i])) {
      
            }
    
            num[j] = '\0';
            addToken(TOKEN_NUMBER, num, line);
            continue;
        }
    }
}

