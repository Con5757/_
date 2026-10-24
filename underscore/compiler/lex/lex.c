#include "lex.h"
#include "../diagnostic.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

Token tokens[MAX_TOKENS];
int tokenCount = 0;

static void lexerError(const char *code, int line, int column,
                       const char *message, const char *help) {
  diagnosticError("error", code, message, line, column,
                  "while reading this token", help);
}

static int startsWith(const char *source, int index, const char *text) {
  return strncmp(source + index, text, strlen(text)) == 0;
}

static void appendChar(char *buffer, int *length, char c,
                       int line, int column) {
  if (*length >= MAX_NAME - 1) {
    lexerError("E1001", line, column,
               "token exceeds the maximum supported length",
               "shorten the identifier or string literal; the current limit is MAX_NAME - 1 characters.");
  }
  buffer[(*length)++] = c;
}

static int isFixedType(const char *word) {
  static const char *const fixedTypes[] = {
      "int64", "int32", "int16", "int8",
      "fp64", "fp32", "fp16", "fp8",
      "str64", "str32", "str16", "str8"};
  size_t count = sizeof(fixedTypes) / sizeof(fixedTypes[0]);
  for (size_t i = 0; i < count; ++i) {
    if (strcmp(word, fixedTypes[i]) == 0) return 1;
  }
  return 0;
}

static int isKillFlagContext(void) {
  if (tokenCount == 0) return 0;
  isTokenType previous = tokens[tokenCount - 1].type;
  return previous == TOKEN_KILL || previous == TOKEN_KILL_FLAG ||
         previous == TOKEN_KILL_SILENT;
}

static int isMemberNamePosition(void) {
  if (tokenCount == 0) return 0;
  isTokenType previous = tokens[tokenCount - 1].type;
  return previous == TOKEN_PERIOD || previous == TOKEN_NAMESPACE;
}

static isTokenType keywordType(const char *word) {
#define KEYWORD(name, token) if (strcmp(word, name) == 0) return token
  KEYWORD("with", TOKEN_WITH);
  KEYWORD("main", TOKEN_MAIN);
  KEYWORD("int", TOKEN_INT);
  KEYWORD("str", TOKEN_STR);
  KEYWORD("fp", TOKEN_FP);
  KEYWORD("void", TOKEN_VOID);
  KEYWORD("in", TOKEN_IN);
  KEYWORD("out", TOKEN_OUT);
  KEYWORD("func", TOKEN_FUNC);
  KEYWORD("kill", TOKEN_KILL);
  KEYWORD("retreat", TOKEN_RETREAT);
  KEYWORD("if", TOKEN_IF);
  KEYWORD("elif", TOKEN_ELIF);
  KEYWORD("else", TOKEN_ELSE);
  KEYWORD("switch", TOKEN_SWITCH);
  KEYWORD("arg", TOKEN_ARG);
  KEYWORD("break", TOKEN_BREAK);
  KEYWORD("incase", TOKEN_INCASE);
  KEYWORD("cont", TOKEN_CONT);
  KEYWORD("attempt", TOKEN_ATTEMPT);
  KEYWORD("during", TOKEN_DURING);
  KEYWORD("repeat", TOKEN_REPEAT);
  KEYWORD("const", TOKEN_CONST);
  KEYWORD("static", TOKEN_STATIC);
  KEYWORD("global", TOKEN_GLOBAL);
  KEYWORD("class", TOKEN_CLASS);
  KEYWORD("public", TOKEN_PUBLIC);
  KEYWORD("private", TOKEN_PRIVATE);
  KEYWORD("hidden", TOKEN_HIDDEN);
  KEYWORD("data", TOKEN_DATA);
  KEYWORD("set", TOKEN_SET);
  KEYWORD("alias", TOKEN_ALIAS);
  KEYWORD("vec", TOKEN_VEC);
  KEYWORD("true", TOKEN_TRUE);
  KEYWORD("false", TOKEN_FALSE);
#undef KEYWORD
  return TOKEN_VARIABLE;
}

void addToken(isTokenType type, const char *text, int line, int column) {
  size_t length = strlen(text);
  if (tokenCount >= MAX_TOKENS) {
    lexerError("E1002", line, column, "too many tokens in source file",
               "split the source into smaller files or increase MAX_TOKENS.");
  }
  if (length >= MAX_NAME) {
    lexerError("E1001", line, column,
               "token exceeds the maximum supported length",
               "shorten the identifier or string literal; the current limit is MAX_NAME - 1 characters.");
  }
  tokens[tokenCount].type = type;
  tokens[tokenCount].line = line;
  tokens[tokenCount].column = column;
  memcpy(tokens[tokenCount].text, text, length + 1);
  tokenCount++;
}

void lex(const char *source) {
  int i = 0;
  int line = 1;
  int column = 1;

  if (source == NULL) {
    lexerError("E1000", 1, 1, "source buffer is NULL",
               "load the source file successfully before calling lex().");
  }

  tokenCount = 0;
  while (source[i] != '\0') {
    char c = source[i];
    int startLine = line;
    int startColumn = column;

    if (c == ' ' || c == '\t' || c == '\r') {
      ++i;
      ++column;
      continue;
    }
    if (c == '\n') {
      addToken(TOKEN_NEWLINE, "\n", line, column);
      ++i;
      ++line;
      column = 1;
      continue;
    }

    /* ';' starts a line comment; ';- ... -;' is a block comment. */
    if (c == ';') {
      if (source[i + 1] == '-') {
        int commentLine = line;
        int commentColumn = column;
        int closed = 0;
        i += 2;
        column += 2;
        while (source[i] != '\0') {
          if (startsWith(source, i, "-;")) {
            i += 2;
            column += 2;
            closed = 1;
            break;
          }
          if (source[i] == '\n') {
            addToken(TOKEN_NEWLINE, "\n", line, column);
            ++i;
            ++line;
            column = 1;
          } else {
            ++i;
            ++column;
          }
        }
        if (!closed) {
          lexerError("E1003", commentLine, commentColumn,
                     "unterminated block comment",
                     "close this comment with `-;`.");
        }
      } else {
        while (source[i] != '\0' && source[i] != '\n') {
          ++i;
          ++column;
        }
      }
      continue;
    }

    if (c == '"') {
      char string[MAX_NAME];
      int length = 0;
      i++;
      column++;
      while (source[i] != '\0' && source[i] != '"') {
        if (source[i] == '\n' || source[i] == '\r') {
          lexerError("E1004", startLine, startColumn,
                     "newline in string literal",
                     "close the string on this line, or add an escape sequence such as `\\n`.");
        }
        if (source[i] == '\\') {
          appendChar(string, &length, source[i], startLine, startColumn);
          ++i; ++column;
          if (source[i] == '\0' || source[i] == '\n' || source[i] == '\r') {
            lexerError("E1005", startLine, startColumn,
                       "unterminated escape sequence in string literal",
                       "add a character after the backslash, for example `\\n`, or close the string.");
          }
          if (source[i] != '\\' && source[i] != '"' && source[i] != 'n' &&
              source[i] != 'r' && source[i] != 't' && source[i] != '0') {
            lexerError("E1008", line, column,
                       "unsupported escape sequence in string literal",
                       "supported escapes are `\\n`, `\\r`, `\\t`, `\\0`, `\\\\`, and `\\\"`.");
          }
          appendChar(string, &length, source[i], startLine, startColumn);
          ++i; ++column;
        } else {
          appendChar(string, &length, source[i], startLine, startColumn);
          ++i; ++column;
        }
      }
      if (source[i] != '"') {
        lexerError("E1006", startLine, startColumn,
                   "unterminated string literal",
                   "add a closing double quote (`\"`).");
      }
      string[length] = '\0';
      ++i; ++column;
      addToken(TOKEN_STRING, string, startLine, startColumn);
      continue;
    }

    if (isdigit((unsigned char)c)) {
      char number[MAX_NAME];
      int length = 0;
      while (isdigit((unsigned char)source[i])) {
        appendChar(number, &length, source[i++], startLine, startColumn);
        ++column;
      }
      if (source[i] == '.' && isdigit((unsigned char)source[i + 1])) {
        appendChar(number, &length, source[i++], startLine, startColumn);
        ++column;
        while (isdigit((unsigned char)source[i])) {
          appendChar(number, &length, source[i++], startLine, startColumn);
          ++column;
        }
      }
      if (source[i] == '.' && isdigit((unsigned char)source[i + 1])) {
        lexerError("E1007", line, column,
                   "malformed numeric literal: more than one decimal point",
                   "a number may contain at most one decimal point; for example, write `12.5` instead of `12.5.6`.");
      }
      number[length] = '\0';
      addToken(TOKEN_NUMBER, number, startLine, startColumn);
      continue;
    }

    if (isalpha((unsigned char)c) || c == '_') {
      char word[MAX_NAME];
      int length = 0;
      while (isalnum((unsigned char)source[i]) || source[i] == '_') {
        appendChar(word, &length, source[i++], startLine, startColumn);
        ++column;
      }
      word[length] = '\0';
      if (isMemberNamePosition()) {
        addToken(TOKEN_VARIABLE, word, startLine, startColumn);
      } else if (isFixedType(word)) {
        addToken(TOKEN_FIXED_TYPE, word, startLine, startColumn);
      } else {
        addToken(keywordType(word), word, startLine, startColumn);
      }
      continue;
    }

    if (c == '-' && isalpha((unsigned char)source[i + 1]) &&
        isKillFlagContext()) {
      char flag[MAX_NAME];
      int length = 0;
      appendChar(flag, &length, source[i++], startLine, startColumn);
      ++column;
      while (isalnum((unsigned char)source[i]) || source[i] == '_' ||
             source[i] == '-') {
        appendChar(flag, &length, source[i++], startLine, startColumn);
        ++column;
      }
      flag[length] = '\0';
      addToken(strcmp(flag, "-silent") == 0 ? TOKEN_KILL_SILENT
                                              : TOKEN_KILL_FLAG,
               flag, startLine, startColumn);
      continue;
    }

    /* Multi-character operators must be tested before their prefixes. */
#define EMIT2(text, type) if (startsWith(source, i, text)) { addToken(type, text, line, column); i += 2; column += 2; continue; }
#define EMIT3(text, type) if (startsWith(source, i, text)) { addToken(type, text, line, column); i += 3; column += 3; continue; }
    EMIT3("-->", TOKEN_DPTR)
    EMIT3("<--", TOKEN_DDEREF)
    EMIT2("->", TOKEN_PTR)
    EMIT2("<-", TOKEN_DEREF)
    EMIT2("~?", TOKEN_SPECIAL_BITWISE)
    EMIT2("!|", NOR)
    EMIT2("!&", NAND)
    EMIT2("!~", XNOR)
    EMIT2("||", OR)
    EMIT2("&&", AND)
    EMIT2("++", TOKEN_INCREMENT)
    EMIT2("--", TOKEN_DECREMENT)
    EMIT2("+=", TOKEN_ADD_ASSIGN)
    EMIT2("-=", TOKEN_SUB_ASSIGN)
    EMIT2("*=", TOKEN_MUL_ASSIGN)
    EMIT2("/=", TOKEN_DIV_ASSIGN)
    EMIT2("%=", TOKEN_MOD_ASSIGN)
    EMIT2("&=", TOKEN_BIT_AND_ASSIGN)
    EMIT2("|=", TOKEN_BIT_OR_ASSIGN)
    EMIT2("^=", TOKEN_BIT_XOR_ASSIGN)
    EMIT2(">=", TOKEN_GREATER_EQUAL)
    EMIT2("<=", TOKEN_LESS_EQUAL)
    EMIT2("==", TOKEN_EQUAL_EQUAL)
    EMIT2("!=", TOKEN_NOT_EQUAL)
    EMIT2("<<", TOKEN_SHIFT_LEFT)
    EMIT2(">>", TOKEN_SHIFT_RIGHT)
    EMIT2("::", TOKEN_NAMESPACE)
#undef EMIT2
#undef EMIT3

    switch (c) {
    case '?': addToken(TOKEN_INCLUDE, "?", line, column); break;
    case '!': addToken(NOT, "!", line, column); break;
    case '~': addToken(XOR, "~", line, column); break;
    case '+': addToken(TOKEN_ADD, "+", line, column); break;
    case '-': addToken(TOKEN_SUB, "-", line, column); break;
    case '*': addToken(TOKEN_MUL, "*", line, column); break;
    case '/': addToken(TOKEN_DIV, "/", line, column); break;
    case '%': addToken(TOKEN_MOD, "%", line, column); break;
    case '|': addToken(TOKEN_BIT_OR, "|", line, column); break;
    case '&': addToken(TOKEN_BIT_AND, "&", line, column); break;
    case '^': addToken(TOKEN_BIT_XOR, "^", line, column); break;
    case '=': addToken(TOKEN_EQUAL, "=", line, column); break;
    case '.': addToken(TOKEN_PERIOD, ".", line, column); break;
    case '@': addToken(TOKEN_AT, "@", line, column); break;
    case ',': addToken(TOKEN_COMMA, ",", line, column); break;
    case ':': addToken(TOKEN_COLON, ":", line, column); break;
    case '\'': addToken(TOKEN_SINGLE_QUOTE, "'", line, column); break;
    case '(': addToken(TOKEN_LEFT_PAREN, "(", line, column); break;
    case ')': addToken(TOKEN_RIGHT_PAREN, ")", line, column); break;
    case '[': addToken(TOKEN_LEFT_SQUARE_PAREN, "[", line, column); break;
    case ']': addToken(TOKEN_RIGHT_SQUARE_PAREN, "]", line, column); break;
    case '{': addToken(TOKEN_LEFT_CURLY_PAREN, "{", line, column); break;
    case '}': addToken(TOKEN_RIGHT_CURLY_PAREN, "}", line, column); break;
    case '<': addToken(TOKEN_LEFT_VEC_PAREN, "<", line, column); break;
    case '>': addToken(TOKEN_RIGHT_VEC_PAREN, ">", line, column); break;
    default: {
      char message[96];
      snprintf(message, sizeof(message), "unexpected character `%c`", c);
      lexerError("E1000", line, column, message,
                 "check for a typo or use a character supported by the language.");
    }
    }
    ++i;
    ++column;
  }

  addToken(TOKEN_EOF, "", line, column);
}
