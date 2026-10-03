#include "lex.h"
#include "../global.h"
#include <ctype.h>
#include <excpt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Token tokens[MAX_TOKENS];
int tokenCount = 0;

// helper func for lex
void addToken(isTokenType type, const char *text, int line) {
  if (tokenCount >= MAX_TOKENS) {
    printf("max_token error");
    exit(1);
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

      while (isdigit((unsigned char)source[i]) || source[i] == '.') {
        if (j < MAX_NAME - 1) {
          num[j++] = source[i];
        }
        i++;
      }

      num[j] = '\0';
      addToken(TOKEN_NUMBER, num, line);
      continue;
    }

    if (isalpha((unsigned char)c) || c == '_') {
      char word[MAX_NAME];
      int j = 0;

      while (isalnum((unsigned char)source[i]) || source[i] == '_') {
        if (j < MAX_NAME - 1)
          word[j++] = source[i];

        i++;
      }

      word[j] = '\0';

      if (strcmp(word, "with") == 0) {
        addToken(TOKEN_WITH, word, line);
      }

      // flags
      else if (strcmp(word, "-silent") == 0) {
        addToken(TOKEN_KILL_SILENT, word, line);
      }

      // types
      else if (strcmp(word, "int") == 0) {
        addToken(TOKEN_INT, word, line);
      } else if (strcmp(word, "str") == 0) {
        addToken(TOKEN_STR, word, line);
      } else if (strcmp(word, "fp") == 0) {
        addToken(TOKEN_FP, word, line);
      } else if (strcmp(word, "void") == 0) {
        addToken(TOKEN_VOID, word, line);
      }

      else if (strcmp(word, "main") == 0) {
        addToken(TOKEN_MAIN, word, line);
      }

      

      else if (strcmp(word, "out") == 0) { // (move to runtime)
        addToken(TOKEN_OUT, word, line);
      } else if (strcmp(word, "in") == 0) {
        addToken(TOKEN_IN, word, line);
      }

      // functions/methods
      else if (strcmp(word, "func") == 0) {
        addToken(TOKEN_FUNC, word, line);
      } else if (strcmp(word, "kill") == 0) {
        addToken(TOKEN_KILL, word, line);
      } else if (strcmp(word, "retreat") == 0) {
        addToken(TOKEN_RETREAT, word, line);
      }

      // logic
      else if (strcmp(word, "||") == 0) {
        addToken(OR, word, line);
      } else if (strcmp(word, "&&") == 0) {
        addToken(AND, word, line);
      } else if (strcmp(word, "!|") == 0) {
        addToken(NOR, word, line);
      } else if (strcmp(word, "!&") == 0) {
        addToken(NAND, word, line);
      } else if (strcmp(word, "!~") == 0) {
        addToken(XNOR, word, line);
      }

      else {
        addToken(TOKEN_VARIABLE, word, line);
      }

      continue;
    }

    // logic
    if (c == '!') {
      addToken(NOT, "!", line);
      i++;
      continue;
    }
    if (c == '~') {
      addToken(XOR, "~", line);
      i++;
      continue;
    }

    if (c == '?') {
      addToken(TOKEN_INCLUDE, "?", line);
      i++;
      continue;
    }
    if (c == '+') {
      addToken(TOKEN_ADD, "+", line);
      i++;
      continue;
    }
    if (c == '-') {
      addToken(TOKEN_SUB, "-", line);
      i++;
      continue;
    }
    if (c == '*') {
      addToken(TOKEN_MUL, "*", line);
      i++;
      continue;
    }
    if (c == '/') {
      addToken(TOKEN_DIV, "/", line);
      i++;
      continue;
    }

    // quotes
    if (c == '"') {
      char string[MAX_NAME];
      int j = 0;

      i++; // skip opening quote

      while (source[i] != '"' && source[i] != '\0') {
        if (j < MAX_NAME - 1) {
          string[j++] = source[i];
        }

        i++;
      }

      if (source[i] == '\0') {
        printf("lexer error on line %d: unterminated string\n", line);
        exit(1);
      }

      string[j] = '\0';

      i++; // skip closing quote

      addToken(TOKEN_STRING, string, line);
      continue;
    }

    if (c == ',') {
      addToken(TOKEN_COMMA, ",", line);
      i++;
      continue;
    }

    if (c == '<') {
      addToken(TOKEN_LEFT_VEC_PAREN, "<", line);
      i++;
      continue;
    }
    if (c == '>') {
      addToken(TOKEN_RIGHT_VEC_PAREN, ">", line);
      i++;
      continue;
    }

    if (c == '\'') {
      addToken(TOKEN_SINGLE_QUOTE, "'", line);
      i++;
      continue;
    }

    // equal
    if (c == '=') {
      addToken(TOKEN_EQUAL, "=", line);
      i++;
      continue;
    }

    // period
    if (c == '.' && source[i + 1] == '.') {
      i += 2;

      while (source[i] != '\0' && source[i] != '\n') {
        i++;
      }

      continue;
    }

    if (c == '.') {
      addToken(TOKEN_PERIOD, ".", line);
      i++;
      continue;
    }

    // parens/brackets
    if (c == '(') {
      addToken(TOKEN_LEFT_PAREN, "(", line);
      i++;
      continue;
    }
    if (c == ')') {
      addToken(TOKEN_RIGHT_PAREN, ")", line);
      i++;
      continue;
    }
    if (c == '[') {
      addToken(TOKEN_LEFT_SQUARE_PAREN, "[", line);
      i++;
      continue;
    }
    if (c == ']') {
      addToken(TOKEN_RIGHT_SQUARE_PAREN, "]", line);
      i++;
      continue;
    }
    if (c == '{') {
      addToken(TOKEN_LEFT_CURLY_PAREN, "{", line);
      i++;
      continue;
    }
    if (c == '}') {
      addToken(TOKEN_RIGHT_CURLY_PAREN, "}", line);
      i++;
      continue;
    }

    printf("lexer error on line %d: unknown char %c\n", line, c);
    exit(i);
  }

  addToken(TOKEN_EOF, "", line);
}