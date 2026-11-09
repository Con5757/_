/*
 * Underscore compiler - compact, multi-file front end and LLVM pipeline.
 *
 * The front end builds an AST and emits portable C as an intermediate form.
 * Clang lowers that translation unit and runtime.c to LLVM IR, then links the
 * IR into one executable. Temporary C/LLVM files are removed on every exit.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#define INITIAL_CAPACITY 64
#define MAX_DIAGNOSTIC 2048

/* ----------------------------- common utils ----------------------------- */

static void *xrealloc(void *ptr, size_t size) {
  void *next = realloc(ptr, size ? size : 1);
  if (!next) {
    fprintf(stderr, "fatal: out of memory\n");
    exit(2);
  }
  return next;
}

static char *xstrdup(const char *s) {
  size_t n = strlen(s);
  char *copy = (char *)malloc(n + 1);
  if (!copy) {
    fprintf(stderr, "fatal: out of memory\n");
    exit(2);
  }
  memcpy(copy, s, n + 1);
  return copy;
}

static char *formatString(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  va_list copy;
  va_copy(copy, ap);
  int n = vsnprintf(NULL, 0, fmt, copy);
  va_end(copy);
  if (n < 0) {
    va_end(ap);
    return xstrdup("");
  }
  char *s = (char *)malloc((size_t)n + 1);
  if (!s) {
    va_end(ap);
    fprintf(stderr, "fatal: out of memory\n");
    exit(2);
  }
  vsnprintf(s, (size_t)n + 1, fmt, ap);
  va_end(ap);
  return s;
}

static int readFile(const char *filename, char **out, size_t *length) {
  FILE *f = fopen(filename, "rb");
  if (!f) {
    fprintf(stderr, "error[E0001]: cannot open '%s': %s\n", filename,
            strerror(errno));
    return 0;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fprintf(stderr, "error[E0002]: cannot seek '%s'\n", filename);
    fclose(f);
    return 0;
  }
  long end = ftell(f);
  if (end < 0) {
    fprintf(stderr, "error[E0002]: cannot determine size of '%s'\n", filename);
    fclose(f);
    return 0;
  }
  rewind(f);
  size_t n = (size_t)end;
  char *buf = (char *)malloc(n + 1);
  if (!buf) {
    fprintf(stderr, "fatal: out of memory\n");
    fclose(f);
    return 0;
  }
  size_t got = fread(buf, 1, n, f);
  if (got != n) {
    fprintf(stderr, "error[E0003]: failed reading '%s'\n", filename);
    free(buf);
    fclose(f);
    return 0;
  }
  buf[n] = '\0';
  fclose(f);
  *out = buf;
  if (length)
    *length = n;
  return 1;
}

/* --------------------------------- lexer --------------------------------- */

typedef enum {
  T_EOF = 0,
  T_ID,
  T_NUMBER,
  T_STRING,
  T_CHAR,
  T_INT,
  T_FP,
  T_STR,
  T_VOID,
  T_BOOL,
  T_CONST,
  T_STATIC,
  T_GLOBAL,
  T_IF,
  T_ELIF,
  T_ELSE,
  T_DURING,
  T_REPEAT,
  T_SWITCH,
  T_ARG,
  T_INCASE,
  T_DATA,
  T_SET,
  T_ALIAS,
  T_RETREAT,
  T_KILL,
  T_BREAK,
  T_CONT,
  T_ATTEMPT,
  T_WITH,
  T_TRUE,
  T_FALSE,
  T_NULL,
  T_LPAREN,
  T_RPAREN,
  T_LBRACE,
  T_RBRACE,
  T_LBRACKET,
  T_RBRACKET,
  T_LT,
  T_GT,
  T_COMMA,
  T_COLON,
  T_DOT,
  T_QUESTION,
  T_AT,
  T_ASSIGN,
  T_ADD,
  T_SUB,
  T_MUL,
  T_DIV,
  T_MOD,
  T_EQ,
  T_NE,
  T_LE,
  T_GE,
  T_AND,
  T_OR,
  T_XOR,
  T_NOT,
  T_BITAND,
  T_BITOR,
  T_BITXOR,
  T_BITNOT,
  T_SHL,
  T_SHR,
  T_ADD_ASSIGN,
  T_SUB_ASSIGN,
  T_MUL_ASSIGN,
  T_DIV_ASSIGN,
  T_MOD_ASSIGN,
  T_AND_ASSIGN,
  T_OR_ASSIGN,
  T_XOR_ASSIGN,
  T_INC,
  T_DEC,
  T_ARROW,
  T_DARROW,
  T_DEREF,
  T_DDEREF,
  T_SCOPE,
  T_LEVEL
} TokenType;

typedef struct {
  TokenType type;
  char *value;
  size_t line;
  size_t column;
  const char *file;
} Token;

typedef struct {
  Token *data;
  size_t count;
  size_t capacity;
} TokenArray;

typedef struct {
  const char *source;
  const char *file;
  size_t pos;
  size_t line;
  size_t column;
  int failed;
  TokenArray tokens;
} Lexer;

static void tokenPush(TokenArray *a, Token t) {
  if (a->count == a->capacity) {
    a->capacity = a->capacity ? a->capacity * 2 : INITIAL_CAPACITY;
    a->data = (Token *)xrealloc(a->data, a->capacity * sizeof(Token));
  }
  a->data[a->count++] = t;
}

static char lexPeek(Lexer *l, size_t ahead) {
  return l->source[l->pos + ahead];
}

static char lexTake(Lexer *l) {
  char c = l->source[l->pos++];
  if (c == '\n') {
    l->line++;
    l->column = 1;
  } else
    l->column++;
  return c;
}

static void lexError(Lexer *l, size_t line, size_t col, const char *fmt, ...) {
  va_list ap;
  fprintf(stderr, "error[E0100]: ");
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fprintf(stderr, "\n  --> %s:%zu:%zu\n", l->file, line, col);
  l->failed = 1;
}

static TokenType keyword(const char *s) {
  struct Pair {
    const char *s;
    TokenType t;
  };
  static const struct Pair words[] = {{"int", T_INT},
                                      {"fp", T_FP},
                                      {"str", T_STR},
                                      {"void", T_VOID},
                                      {"bool", T_BOOL},
                                      {"const", T_CONST},
                                      {"static", T_STATIC},
                                      {"global", T_GLOBAL},
                                      {"if", T_IF},
                                      {"elif", T_ELIF},
                                      {"else", T_ELSE},
                                      {"during", T_DURING},
                                      {"repeat", T_REPEAT},
                                      {"switch", T_SWITCH},
                                      {"arg", T_ARG},
                                      {"incase", T_INCASE},
                                      {"data", T_DATA},
                                      {"set", T_SET},
                                      {"alias", T_ALIAS},
                                      {"retreat", T_RETREAT},
                                      {"kill", T_KILL},
                                      {"break", T_BREAK},
                                      {"cont", T_CONT},
                                      {"attempt", T_ATTEMPT},
                                      {"with", T_WITH},
                                      {"true", T_TRUE},
                                      {"false", T_FALSE},
                                      {"null", T_NULL},
                                      {NULL, 0}};
  for (size_t i = 0; words[i].s; ++i)
    if (strcmp(s, words[i].s) == 0)
      return words[i].t;
  return T_ID;
}

static void lexAdd(Lexer *l, TokenType type, const char *value, size_t line,
                   size_t col) {
  Token t;
  t.type = type;
  t.value = xstrdup(value ? value : "");
  t.line = line;
  t.column = col;
  t.file = l->file;
  tokenPush(&l->tokens, t);
}

static int matchText(Lexer *l, const char *text) {
  size_t n = strlen(text);
  if (strncmp(l->source + l->pos, text, n) != 0)
    return 0;
  for (size_t i = 0; i < n; ++i)
    lexTake(l);
  return 1;
}

static char *decodeString(Lexer *l, char quote, size_t startLine,
                          size_t startCol) {
  size_t cap = 32, n = 0;
  char *s = (char *)malloc(cap);
  if (!s)
    exit(2);
  while (lexPeek(l, 0) && lexPeek(l, 0) != quote) {
    char c = lexTake(l);
    if (c == '\n' || c == '\r') {
      lexError(l, startLine, startCol,
               "newline in string literal; close the string before the end of "
               "the line");
      free(s);
      return NULL;
    }
    if (c == '\\') {
      char e = lexTake(l);
      if (!e)
        break;
      switch (e) {
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
      case '0':
        c = '\0';
        break;
      case '\\':
        c = '\\';
        break;
      case '"':
        c = '"';
        break;
      case '\'':
        c = '\'';
        break;
      default:
        c = e;
        break;
      }
    }
    if (n + 2 > cap) {
      cap *= 2;
      s = (char *)xrealloc(s, cap);
    }
    s[n++] = c;
  }
  if (lexPeek(l, 0) != quote) {
    lexError(l, startLine, startCol,
             "unterminated string literal; expected closing %c", quote);
    free(s);
    return NULL;
  }
  lexTake(l);
  s[n] = '\0';
  return s;
}

static int isFixedTypeName(const char *s) {
  if (strncmp(s, "int", 3) == 0 || strncmp(s, "fp", 2) == 0 ||
      strncmp(s, "str", 3) == 0) {
    const char *p = s + (s[0] == 'i' ? 3 : (s[0] == 'f' ? 2 : 3));
    return (strcmp(p, "8") == 0 || strcmp(p, "16") == 0 ||
            strcmp(p, "32") == 0 || strcmp(p, "64") == 0);
  }
  return 0;
}

static TokenArray lexSource(const char *source, const char *file,
                            int printTokens) {
  Lexer l;
  memset(&l, 0, sizeof(l));
  l.source = source;
  l.file = file;
  l.line = 1;
  l.column = 1;
  while (lexPeek(&l, 0)) {
    char c = lexPeek(&l, 0);
    size_t line = l.line, col = l.column;
    if (isspace((unsigned char)c)) {
      lexTake(&l);
      continue;
    }

    /* Line and block comments: ;, ;- ... -;, .., and legacy !.. ..! */
    if (c == ';') {
      if (lexPeek(&l, 1) == '-') {
        lexTake(&l);
        lexTake(&l);
        int closed = 0;
        while (lexPeek(&l, 0)) {
          if (matchText(&l, "-;")) {
            closed = 1;
            break;
          }
          lexTake(&l);
        }
        if (!closed)
          lexError(&l, line, col, "unterminated block comment; expected -;");
      } else {
        while (lexPeek(&l, 0) && lexPeek(&l, 0) != '\n')
          lexTake(&l);
      }
      continue;
    }
    if (c == '.' && lexPeek(&l, 1) == '.') {
      while (lexPeek(&l, 0) && lexPeek(&l, 0) != '\n')
        lexTake(&l);
      continue;
    }
    if (c == '!' && lexPeek(&l, 1) == '.' && lexPeek(&l, 2) == '.') {
      lexTake(&l);
      lexTake(&l);
      lexTake(&l);
      int closed = 0;
      while (lexPeek(&l, 0)) {
        if (matchText(&l, "..!")) {
          closed = 1;
          break;
        }
        lexTake(&l);
      }
      if (!closed)
        lexError(&l, line, col, "unterminated !.. comment; expected ..!");
      continue;
    }

    if (isalpha((unsigned char)c) || c == '_') {
      size_t start = l.pos;
      while (isalnum((unsigned char)lexPeek(&l, 0)) || lexPeek(&l, 0) == '_')
        lexTake(&l);
      size_t n = l.pos - start;
      char *word = (char *)malloc(n + 1);
      memcpy(word, source + start, n);
      word[n] = '\0';
      TokenType t = keyword(word);
      if (t == T_ID && isFixedTypeName(word))
        t = T_ID; /* Parsed as a type by its spelling. */
      lexAdd(&l, t, word, line, col);
      free(word);
      continue;
    }

    if (isdigit((unsigned char)c)) {
      size_t start = l.pos;
      while (isdigit((unsigned char)lexPeek(&l, 0)))
        lexTake(&l);
      if (lexPeek(&l, 0) == '.' && isdigit((unsigned char)lexPeek(&l, 1))) {
        lexTake(&l);
        while (isdigit((unsigned char)lexPeek(&l, 0)))
          lexTake(&l);
      }
      if (lexPeek(&l, 0) == 'e' || lexPeek(&l, 0) == 'E') {
        size_t save = l.pos;
        lexTake(&l);
        if (lexPeek(&l, 0) == '+' || lexPeek(&l, 0) == '-')
          lexTake(&l);
        if (!isdigit((unsigned char)lexPeek(&l, 0)))
          l.pos = save;
        else
          while (isdigit((unsigned char)lexPeek(&l, 0)))
            lexTake(&l);
      }
      size_t n = l.pos - start;
      char *number = (char *)malloc(n + 1);
      memcpy(number, source + start, n);
      number[n] = '\0';
      lexAdd(&l, T_NUMBER, number, line, col);
      free(number);
      continue;
    }

    if (c == '"' || c == '\'') {
      lexTake(&l);
      char *value = decodeString(&l, c, line, col);
      if (value) {
        lexAdd(&l, c == '"' ? T_STRING : T_CHAR, value, line, col);
        free(value);
      }
      continue;
    }

    struct Op {
      const char *s;
      TokenType t;
    };
    static const struct Op ops[] = {{"-->", T_DARROW},
                                    {"<--", T_DDEREF},
                                    {"+=", T_ADD_ASSIGN},
                                    {"-=", T_SUB_ASSIGN},
                                    {"*=", T_MUL_ASSIGN},
                                    {"/=", T_DIV_ASSIGN},
                                    {"%=", T_MOD_ASSIGN},
                                    {"&=", T_AND_ASSIGN},
                                    {"|=", T_OR_ASSIGN},
                                    {"^=", T_XOR_ASSIGN},
                                    {"++", T_INC},
                                    {"--", T_DEC},
                                    {"==", T_EQ},
                                    {"!=", T_NE},
                                    {"<=", T_LE},
                                    {">=", T_GE},
                                    {"&&", T_AND},
                                    {"||", T_OR},
                                    {"<<", T_SHL},
                                    {">>", T_SHR},
                                    {"->", T_ARROW},
                                    {"<-", T_DEREF},
                                    {"::", T_SCOPE},
                                    {"~>", T_LEVEL},
                                    {NULL, 0}};
    int found = 0;
    for (size_t i = 0; ops[i].s; ++i) {
      if (matchText(&l, ops[i].s)) {
        lexAdd(&l, ops[i].t, ops[i].s, line, col);
        found = 1;
        break;
      }
    }
    if (found)
      continue;
    TokenType t = T_EOF;
    switch (lexTake(&l)) {
    case '(':
      t = T_LPAREN;
      break;
    case ')':
      t = T_RPAREN;
      break;
    case '{':
      t = T_LBRACE;
      break;
    case '}':
      t = T_RBRACE;
      break;
    case '[':
      t = T_LBRACKET;
      break;
    case ']':
      t = T_RBRACKET;
      break;
    case '<':
      t = T_LT;
      break;
    case '>':
      t = T_GT;
      break;
    case ',':
      t = T_COMMA;
      break;
    case ':':
      t = T_COLON;
      break;
    case '.':
      t = T_DOT;
      break;
    case '?':
      t = T_QUESTION;
      break;
    case '@':
      t = T_AT;
      break;
    case '=':
      t = T_ASSIGN;
      break;
    case '+':
      t = T_ADD;
      break;
    case '-':
      t = T_SUB;
      break;
    case '*':
      t = T_MUL;
      break;
    case '/':
      t = T_DIV;
      break;
    case '%':
      t = T_MOD;
      break;
    case '!':
      t = T_NOT;
      break;
    case '&':
      t = T_BITAND;
      break;
    case '|':
      t = T_BITOR;
      break;
    case '^':
      t = T_BITXOR;
      break;
    case '~':
      t = T_BITNOT;
      break;
    default:
      lexError(&l, line, col, "unexpected character '%c'", source[l.pos - 1]);
      break;
    }
    if (t != T_EOF) {
      char value[2] = {source[l.pos - 1], '\0'};
      lexAdd(&l, t, value, line, col);
    }
  }
  Token eof = {T_EOF, xstrdup("EOF"), l.line, l.column, file};
  tokenPush(&l.tokens, eof);
  if (printTokens) {
    for (size_t i = 0; i < l.tokens.count; ++i)
      printf("%s:%zu:%zu Token %-3d %s\n", file, l.tokens.data[i].line,
             l.tokens.data[i].column, (int)l.tokens.data[i].type,
             l.tokens.data[i].value);
  }
  if (l.failed) {
    for (size_t i = 0; i < l.tokens.count; ++i)
      free(l.tokens.data[i].value);
    free(l.tokens.data);
    memset(&l.tokens, 0, sizeof(l.tokens));
  }
  return l.tokens;
}

/* ---------------------------------- AST ---------------------------------- */

typedef enum {
  N_PROGRAM,
  N_BLOCK,
  N_VAR,
  N_FUNCTION,
  N_DATA,
  N_ENUM,
  N_ALIAS,
  N_IF,
  N_WHILE,
  N_FOR,
  N_SWITCH,
  N_CASE,
  N_RETURN,
  N_KILL,
  N_BREAK,
  N_CONTINUE,
  N_EXPR_STMT,
  N_ASSIGN,
  X_IDENTIFIER,
  X_NUMBER,
  X_STRING,
  X_BOOL,
  X_NULL,
  X_BINARY,
  X_UNARY,
  X_CALL,
  X_MEMBER,
  X_INDEX,
  X_ARRAY
} NodeKind;

typedef struct Node Node;
struct Node {
  NodeKind kind;
  const char *file;
  size_t line, column;
  char *name;
  char *text;
  char *type;
  char *ownerType;
  char *methodName;
  int pointerDepth;
  int isConst, isStatic, isGlobal, isInline;
  size_t capacity;   /* str buffer capacity, excluding its NUL byte */
  size_t arrayCount; /* angle-bracket array element count */
  Node *a, *b, *c;
  Node **items;
  size_t count, itemCapacity;
};

static Node *nodeNew(NodeKind kind, Token *at) {
  Node *n = (Node *)calloc(1, sizeof(Node));
  if (!n) {
    fprintf(stderr, "fatal: out of memory\n");
    exit(2);
  }
  n->kind = kind;
  if (at) {
    n->file = at->file;
    n->line = at->line;
    n->column = at->column;
  }
  return n;
}

static void nodeAdd(Node *n, Node *child) {
  if (n->count == n->itemCapacity) {
    n->itemCapacity = n->itemCapacity ? n->itemCapacity * 2 : 4;
    n->items = (Node **)xrealloc(n->items, n->itemCapacity * sizeof(Node *));
  }
  n->items[n->count++] = child;
}

static void freeNode(Node *n) {
  if (!n)
    return;
  freeNode(n->a);
  freeNode(n->b);
  freeNode(n->c);
  for (size_t i = 0; i < n->count; ++i)
    freeNode(n->items[i]);
  free(n->items);
  free(n->name);
  free(n->text);
  free(n->type);
  free(n->ownerType);
  free(n->methodName);
  free(n);
}

/* -------------------------------- parser --------------------------------- */

typedef struct {
  Token *tokens;
  size_t count, pos;
  const char *file;
  int failed;
} Parser;

static Token *peek(Parser *p) {
  return p->pos < p->count ? &p->tokens[p->pos] : NULL;
}
static Token *look(Parser *p, size_t n) {
  return p->pos + n < p->count ? &p->tokens[p->pos + n] : NULL;
}
static Token *take(Parser *p) {
  Token *t = peek(p);
  if (t && t->type != T_EOF)
    p->pos++;
  return t;
}
static int match(Parser *p, TokenType type) {
  if (peek(p) && peek(p)->type == type) {
    take(p);
    return 1;
  }
  return 0;
}

static void parseErrorAt(Parser *p, Token *t, const char *code, const char *fmt,
                         ...) {
  if (p->failed)
    return;
  p->failed = 1;
  fprintf(stderr, "error[%s]: ", code);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  if (!t)
    t = peek(p);
  fprintf(stderr, "\n  --> %s:%zu:%zu\n", t && t->file ? t->file : p->file,
          t ? t->line : 1, t ? t->column : 1);
}

static Token *expect(Parser *p, TokenType type, const char *description) {
  Token *t = peek(p);
  if (!t || t->type != type) {
    parseErrorAt(p, t, "E1001", "expected %s, found '%s'", description,
                 t ? t->value : "end of input");
    return NULL;
  }
  return take(p);
}

static int tokenIsType(Parser *p, Token *t) {
  (void)p;
  if (!t)
    return 0;
  if (t->type == T_INT || t->type == T_FP || t->type == T_STR ||
      t->type == T_VOID || t->type == T_BOOL)
    return 1;
  return t->type == T_ID && isFixedTypeName(t->value);
}

static int isAssignmentToken(TokenType t) {
  return t == T_ASSIGN || t == T_ADD_ASSIGN || t == T_SUB_ASSIGN ||
         t == T_MUL_ASSIGN || t == T_DIV_ASSIGN || t == T_MOD_ASSIGN ||
         t == T_AND_ASSIGN || t == T_OR_ASSIGN || t == T_XOR_ASSIGN;
}

static Node *parseExpression(Parser *p);
static Node *parseStatement(Parser *p);
static Node *parseBlock(Parser *p);

static int parseUnsigned(Parser *p, size_t *out, const char *what) {
  Token *t = expect(p, T_NUMBER, "a non-negative integer size");
  if (!t)
    return 0;
  if (strchr(t->value, '.') || strchr(t->value, 'e') || strchr(t->value, 'E') ||
      t->value[0] == '-') {
    parseErrorAt(p, t, "E1002", "%s must be a non-negative integer", what);
    return 0;
  }
  errno = 0;
  char *end = NULL;
  unsigned long long v = strtoull(t->value, &end, 10);
  if (errno == ERANGE || !end || *end || v > SIZE_MAX) {
    parseErrorAt(p, t, "E1002", "%s is outside the supported size range", what);
    return 0;
  }
  *out = (size_t)v;
  return 1;
}

static Node *parseArrayLiteral(Parser *p) {
  Token *start = expect(p, T_LBRACKET, "'['");
  if (!start)
    return NULL;
  Node *n = nodeNew(X_ARRAY, start);
  if (match(p, T_RBRACKET))
    return n;
  do {
    Node *item = parseExpression(p);
    if (!item) {
      freeNode(n);
      return NULL;
    }
    nodeAdd(n, item);
  } while (match(p, T_COMMA));
  if (!expect(p, T_RBRACKET, "']'")) {
    freeNode(n);
    return NULL;
  }
  return n;
}

static Node *parsePrimary(Parser *p) {
  Token *t = peek(p);
  if (!t)
    return NULL;
  if (t->type == T_NUMBER) {
    take(p);
    Node *n = nodeNew(X_NUMBER, t);
    n->text = xstrdup(t->value);
    return n;
  }
  if (t->type == T_STRING || t->type == T_CHAR) {
    take(p);
    Node *n = nodeNew(X_STRING, t);
    n->text = xstrdup(t->value);
    return n;
  }
  if (t->type == T_TRUE || t->type == T_FALSE) {
    take(p);
    Node *n = nodeNew(X_BOOL, t);
    n->text = xstrdup(t->value);
    return n;
  }
  if (t->type == T_NULL) {
    take(p);
    return nodeNew(X_NULL, t);
  }
  if (t->type == T_ID || (t->type == T_KILL && 0)) {
    take(p);
    Node *n = nodeNew(X_IDENTIFIER, t);
    n->name = xstrdup(t->value);
    return n;
  }
  if (match(p, T_LPAREN)) {
    Node *n = parseExpression(p);
    if (!n)
      return NULL;
    if (!expect(p, T_RPAREN, "')'")) {
      freeNode(n);
      return NULL;
    }
    return n;
  }
  if (t->type == T_LBRACKET)
    return parseArrayLiteral(p);
  parseErrorAt(p, t, "E1003",
               "expected an expression (value, function call, array, or "
               "parenthesized expression), found '%s'",
               t->value);
  return NULL;
}

static Node *parsePostfix(Parser *p) {
  Node *left = parsePrimary(p);
  if (!left)
    return NULL;
  for (;;) {
    Token *t = peek(p);
    if (match(p, T_LPAREN)) {
      Node *call = nodeNew(X_CALL, t);
      call->a = left;
      if (!match(p, T_RPAREN)) {
        do {
          Node *arg = parseExpression(p);
          if (!arg) {
            freeNode(call);
            return NULL;
          }
          nodeAdd(call, arg);
        } while (match(p, T_COMMA));
        if (!expect(p, T_RPAREN, "')' after function arguments")) {
          freeNode(call);
          return NULL;
        }
      }
      left = call;
      continue;
    }
    if (match(p, T_LBRACKET)) {
      Node *idx = nodeNew(X_INDEX, t);
      idx->a = left;
      idx->b = parseExpression(p);
      if (!idx->b || !expect(p, T_RBRACKET, "']' after array index")) {
        freeNode(idx);
        return NULL;
      }
      left = idx;
      continue;
    }
    if (t && (t->type == T_DOT || t->type == T_SCOPE || t->type == T_LEVEL)) {
      take(p);
      Token *member = expect(p, T_ID, "a member or module function name");
      if (!member) {
        freeNode(left);
        return NULL;
      }
      Node *m = nodeNew(X_MEMBER, t);
      m->a = left;
      m->name = xstrdup(member->value);
      m->text = xstrdup(t->type == T_LEVEL ? "~>"
                                           : (t->type == T_SCOPE ? "::" : "."));
      left = m;
      continue;
    }
    if (match(p, T_INC) || match(p, T_DEC)) {
      Token *op = &p->tokens[p->pos - 1];
      Node *u = nodeNew(X_UNARY, op);
      u->text = formatString("post%s", op->type == T_INC ? "++" : "--");
      u->a = left;
      left = u;
      continue;
    }
    break;
  }
  return left;
}

static Node *makeUnary(Parser *p, const char *op, Token *t, Node *right) {
  (void)p;
  Node *n = nodeNew(X_UNARY, t);
  n->text = xstrdup(op);
  n->a = right;
  return n;
}

static Node *parseUnary(Parser *p) {
  Token *t = peek(p);
  if (!t)
    return NULL;
  if (match(p, T_NOT))
    return makeUnary(p, "!", t, parseUnary(p));
  if (match(p, T_SUB))
    return makeUnary(p, "-", t, parseUnary(p));
  if (match(p, T_ADD))
    return makeUnary(p, "+", t, parseUnary(p));
  if (match(p, T_BITNOT))
    return makeUnary(p, "~", t, parseUnary(p));
  if (match(p, T_AT))
    return makeUnary(p, "&", t, parseUnary(p));
  if (match(p, T_DEREF))
    return makeUnary(p, "*", t, parseUnary(p));
  if (match(p, T_DDEREF))
    return makeUnary(p, "**", t, parseUnary(p));
  if (match(p, T_INC))
    return makeUnary(p, "++", t, parseUnary(p));
  if (match(p, T_DEC))
    return makeUnary(p, "--", t, parseUnary(p));
  return parsePostfix(p);
}

typedef struct {
  TokenType token;
  const char *op;
  int precedence;
} Operator;
static const Operator operators[] = {
    {T_OR, "||", 1},    {T_BITNOT, "~", 2}, {T_AND, "&&", 3}, {T_BITOR, "|", 4},
    {T_BITXOR, "^", 5}, {T_BITAND, "&", 6}, {T_EQ, "==", 7},  {T_NE, "!=", 7},
    {T_LT, "<", 8},     {T_GT, ">", 8},     {T_LE, "<=", 8},  {T_GE, ">=", 8},
    {T_SHL, "<<", 9},   {T_SHR, ">>", 9},   {T_ADD, "+", 10}, {T_SUB, "-", 10},
    {T_MUL, "*", 11},   {T_DIV, "/", 11},   {T_MOD, "%", 11}, {0, NULL, 0}};

static const Operator *findOperator(TokenType t) {
  for (size_t i = 0; operators[i].op; ++i)
    if (operators[i].token == t)
      return &operators[i];
  return NULL;
}

static Node *parseBinary(Parser *p, int minPrec) {
  Node *left = parseUnary(p);
  if (!left)
    return NULL;
  for (;;) {
    Token *t = peek(p);
    const Operator *op = t ? findOperator(t->type) : NULL;
    if (!op || op->precedence < minPrec)
      break;
    take(p);
    Node *right = parseBinary(p, op->precedence + 1);
    if (!right) {
      freeNode(left);
      return NULL;
    }
    Node *n = nodeNew(X_BINARY, t);
    n->text = xstrdup(op->op);
    n->a = left;
    n->b = right;
    left = n;
  }
  return left;
}

static Node *parseExpression(Parser *p) { return parseBinary(p, 1); }

static char *parseTypeName(Parser *p) {
  Token *t = peek(p);
  if (!t)
    return NULL;
  if (tokenIsType(p, t) || t->type == T_ID) {
    take(p);
    return xstrdup(t->value);
  }
  parseErrorAt(p, t, "E1004", "expected a type name, found '%s'", t->value);
  return NULL;
}

static int parsePointerMarker(Parser *p) {
  if (match(p, T_ARROW))
    return 1;
  if (match(p, T_DARROW))
    return 2;
  return 0;
}

static Node *parseVariableAfterType(Parser *p, char *type, int pointerDepth,
                                    int isConst, int isStatic, int isGlobal,
                                    int allowFunction, int allowComma);

static Node *parseTypeDeclaration(Parser *p, int topLevel, int allowComma) {
  int isConst = 0, isStatic = 0, isGlobal = 0;
  while (peek(p) && (peek(p)->type == T_CONST || peek(p)->type == T_STATIC ||
                     peek(p)->type == T_GLOBAL)) {
    TokenType t = take(p)->type;
    if (t == T_CONST)
      isConst = 1;
    else if (t == T_STATIC)
      isStatic = 1;
    else
      isGlobal = 1;
  }
  char *type = parseTypeName(p);
  if (!type)
    return NULL;
  int ptr = parsePointerMarker(p);
  return parseVariableAfterType(p, type, ptr, isConst, isStatic, isGlobal,
                                topLevel, allowComma);
}

static Node *parseVariableAfterType(Parser *p, char *type, int pointerDepth,
                                    int isConst, int isStatic, int isGlobal,
                                    int allowFunction, int allowComma) {
  Token *start = peek(p);
  int fixedType = isFixedTypeName(type);
  int atName = 0;
  if (fixedType && match(p, T_AT))
    atName = 1;
  Token *name = expect(p, T_ID, "a variable or function name");
  if (!name) {
    free(type);
    return NULL;
  }
  if (allowFunction && match(p, T_LPAREN)) {
    Node *fn = nodeNew(N_FUNCTION, start ? start : name);
    fn->name = xstrdup(name->value);
    fn->type = type;
    fn->pointerDepth = pointerDepth;
    fn->isStatic = isStatic;
    fn->isGlobal = isGlobal;
    if (!match(p, T_RPAREN)) {
      do {
        int c = match(p, T_CONST);
        char *pt = parseTypeName(p);
        if (!pt) {
          freeNode(fn);
          return NULL;
        }
        int pp = parsePointerMarker(p);
        Token *pn = expect(p, T_ID, "a parameter name");
        if (!pn) {
          free(pt);
          freeNode(fn);
          return NULL;
        }
        Node *param = nodeNew(N_VAR, pn);
        param->name = xstrdup(pn->value);
        param->type = pt;
        param->pointerDepth = pp;
        param->isConst = c;
        if (match(p, T_LBRACKET)) {
          if (!parseUnsigned(p, &param->capacity,
                             "parameter buffer capacity") ||
              !expect(p, T_RBRACKET, "']'")) {
            freeNode(param);
            freeNode(fn);
            return NULL;
          }
        }
        nodeAdd(fn, param);
      } while (match(p, T_COMMA));
      if (!expect(p, T_RPAREN, "')' after parameters")) {
        freeNode(fn);
        return NULL;
      }
    }
    /* Function flags such as <void>, <static,inline> are accepted as metadata.
     */
    if (match(p, T_LT)) {
      int depth = 1;
      while (peek(p) && depth) {
        if (match(p, T_LT)) {
          depth++;
          continue;
        }
        if (match(p, T_GT)) {
          depth--;
          continue;
        }
        Token *flag = take(p);
        if (depth == 1 && flag) {
          if (!strcmp(flag->value, "static"))
            fn->isStatic = 1;
          else if (!strcmp(flag->value, "inline"))
            fn->isInline = 1;
          else if (!strcmp(flag->value, "void")) { /* legacy signature marker */
          } else if (flag->type != T_COMMA) {
            parseErrorAt(p, flag, "E1014",
                         "unsupported function flag '%s'; supported flags are "
                         "void, static, and inline",
                         flag->value);
            freeNode(fn);
            return NULL;
          }
        }
      }
      if (depth) {
        parseErrorAt(p, peek(p), "E1005",
                     "unterminated function flag list; expected '>'");
        freeNode(fn);
        return NULL;
      }
    }
    if (peek(p) && peek(p)->type == T_LBRACE)
      fn->a = parseBlock(p);
    /* No body means a declaration/prototype, useful for native modules. */
    if (p->failed) {
      freeNode(fn);
      return NULL;
    }
    return fn;
  }

  Node *var = nodeNew(N_VAR, start ? start : name);
  var->name = xstrdup(name->value);
  var->type = type;
  var->pointerDepth = pointerDepth;
  var->isConst = isConst;
  var->isStatic = isStatic;
  var->isGlobal = isGlobal;
  if (atName) { /* @ already consumed before the name */
  }
  if (match(p, T_LBRACKET)) {
    if (!parseUnsigned(p, &var->capacity, "string-buffer capacity") ||
        !expect(p, T_RBRACKET, "']' after string-buffer capacity")) {
      freeNode(var);
      return NULL;
    }
  } else if (strncmp(var->type, "str", 3) == 0 &&
             strcmp(var->type, "str") != 0) {
    var->capacity = (size_t)strtoul(var->type + 3, NULL, 10);
  }
  if (match(p, T_LT)) {
    if (!parseUnsigned(p, &var->arrayCount, "array element count") ||
        !expect(p, T_GT, "'>' after array element count")) {
      freeNode(var);
      return NULL;
    }
  }
  if (match(p, T_ASSIGN)) {
    if (peek(p) && peek(p)->type == T_LBRACKET)
      var->a = parseArrayLiteral(p);
    else
      var->a = parseExpression(p);
    if (!var->a) {
      freeNode(var);
      return NULL;
    }
  } else if (var->isConst) {
    parseErrorAt(p, name, "E1006", "constant '%s' must have an initializer",
                 name->value);
    freeNode(var);
    return NULL;
  }
  if (var->capacity && strcmp(var->type, "str") != 0 &&
      strncmp(var->type, "str", 3) != 0) {
    parseErrorAt(
        p, name, "E1007",
        "square-bracket buffer capacity is only valid for str declarations");
    freeNode(var);
    return NULL;
  }
  if (var->arrayCount == 0 && var->a && var->a->kind == X_ARRAY)
    var->arrayCount = var->a->count;
  if (var->capacity && var->a && var->a->kind == X_STRING &&
      strlen(var->a->text) > var->capacity) {
    parseErrorAt(p, name, "E1013",
                 "string initializer contains %zu bytes but '%s' has capacity "
                 "%zu; increase the bracket size to at least %zu",
                 strlen(var->a->text), var->name, var->capacity,
                 strlen(var->a->text));
    freeNode(var);
    return NULL;
  }
  if (var->capacity && var->a && var->a->kind == X_ARRAY) {
    for (size_t i = 0; i < var->a->count; ++i)
      if (var->a->items[i]->kind == X_STRING &&
          strlen(var->a->items[i]->text) > var->capacity) {
        parseErrorAt(
            p, name, "E1013",
            "string initializer at index %zu exceeds '%s' capacity %zu", i,
            var->name, var->capacity);
        freeNode(var);
        return NULL;
      }
  }
  if (var->arrayCount && var->a && var->a->kind == X_ARRAY &&
      var->a->count > var->arrayCount) {
    parseErrorAt(
        p, name, "E1008",
        "array initializer has %zu elements but the declared array holds %zu",
        var->a->count, var->arrayCount);
    freeNode(var);
    return NULL;
  }
  /* Parse additional names in declarations such as "fp first, second". */
  if (allowComma && match(p, T_COMMA)) {
    Node *next =
        parseVariableAfterType(p, xstrdup(var->type), pointerDepth, isConst,
                               isStatic, isGlobal, 0, allowComma);
    if (!next) {
      freeNode(var);
      return NULL;
    }
    nodeAdd(var, next);
  }
  return var;
}

static Node *parseData(Parser *p) {
  Token *start = take(p);
  Token *name = expect(p, T_ID, "a record type name");
  if (!name)
    return NULL;
  Node *n = nodeNew(N_DATA, start);
  n->name = xstrdup(name->value);
  if (!expect(p, T_LBRACE, "'{' after record name")) {
    freeNode(n);
    return NULL;
  }
  while (peek(p) && peek(p)->type != T_RBRACE && peek(p)->type != T_EOF) {
    Node *member = parseTypeDeclaration(p, 1, 1);
    if (!member) {
      freeNode(n);
      return NULL;
    }
    if (member->kind == N_FUNCTION) {
      member->ownerType = xstrdup(n->name);
      member->methodName = xstrdup(member->name);
      member->name = formatString("%s_%s", n->name, member->methodName);
      int hasSelf = member->count > 0 && member->items[0]->name &&
                    !strcmp(member->items[0]->name, "self");
      if (!hasSelf) {
        Node *self = nodeNew(N_VAR, start);
        self->name = xstrdup("self");
        self->type = xstrdup(n->name);
        self->pointerDepth = 1;
        Node **params = (Node **)malloc((member->count + 1) * sizeof(Node *));
        if (!params) {
          freeNode(self);
          freeNode(member);
          freeNode(n);
          fprintf(stderr, "fatal: out of memory\n");
          exit(2);
        }
        params[0] = self;
        for (size_t i = 0; i < member->count; ++i)
          params[i + 1] = member->items[i];
        free(member->items);
        member->items = params;
        member->count++;
        member->itemCapacity = member->count;
      }
    } else if (member->kind != N_VAR) {
      parseErrorAt(p, peek(p), "E1009",
                   "record members must be fields or typed methods");
      freeNode(member);
      freeNode(n);
      return NULL;
    }
    nodeAdd(n, member);
  }
  if (!expect(p, T_RBRACE, "'}' after record members")) {
    freeNode(n);
    return NULL;
  }
  return n;
}

static Node *parseEnum(Parser *p, int isAlias) {
  Token *start = take(p);
  char *name = NULL;
  if (peek(p) && peek(p)->type == T_ID && look(p, 1) &&
      look(p, 1)->type == T_LBRACE)
    name = xstrdup(take(p)->value);
  Node *n = nodeNew(N_ENUM, start);
  if (!expect(p, T_LBRACE, "'{' before enum values")) {
    free(name);
    freeNode(n);
    return NULL;
  }
  while (peek(p) && peek(p)->type != T_RBRACE && peek(p)->type != T_EOF) {
    Token *v = expect(p, T_ID, "an enum value");
    if (!v) {
      free(name);
      freeNode(n);
      return NULL;
    }
    Node *item = nodeNew(N_CASE, v);
    item->name = xstrdup(v->value);
    nodeAdd(n, item);
    if (!match(p, T_COMMA) && peek(p) && peek(p)->type != T_RBRACE &&
        peek(p)->type != T_ID) {
      parseErrorAt(p, peek(p), "E1010", "expected ',' or another enum value");
      free(name);
      freeNode(n);
      return NULL;
    }
    match(p, T_COMMA);
  }
  if (!expect(p, T_RBRACE, "'}' after enum values")) {
    free(name);
    freeNode(n);
    return NULL;
  }
  if (isAlias) {
    Token *nm = expect(p, T_ID, "the enum type name after the set declaration");
    if (!nm) {
      free(name);
      freeNode(n);
      return NULL;
    }
    free(name);
    name = xstrdup(nm->value);
  }
  n->name = name;
  return n;
}

static Node *parseAlias(Parser *p) {
  Token *start = take(p);
  if (peek(p) && peek(p)->type == T_SET)
    return parseEnum(p, 1);
  char *base = parseTypeName(p);
  if (!base)
    return NULL;
  Token *name = expect(p, T_ID, "an alias name");
  if (!name) {
    free(base);
    return NULL;
  }
  Node *n = nodeNew(N_ALIAS, start);
  n->name = xstrdup(name->value);
  n->type = base;
  return n;
}

static Node *parseIf(Parser *p) {
  Token *start = take(p);
  Node *n = nodeNew(N_IF, start);
  if (!expect(p, T_LPAREN, "'(' after if")) {
    freeNode(n);
    return NULL;
  }
  n->a = parseExpression(p);
  if (!n->a || !expect(p, T_RPAREN, "')' after condition")) {
    freeNode(n);
    return NULL;
  }
  n->b = parseBlock(p);
  if (!n->b) {
    freeNode(n);
    return NULL;
  }
  if (match(p, T_ELIF)) {
    p->pos--;
    n->c = parseIf(p);
  } else if (match(p, T_ELSE)) {
    if (match(p, T_IF)) {
      p->pos--;
      n->c = parseIf(p);
    } else
      n->c = parseBlock(p);
  }
  if (p->failed) {
    freeNode(n);
    return NULL;
  }
  return n;
}

static Node *parseWhile(Parser *p) {
  Token *start = take(p);
  Node *n = nodeNew(N_WHILE, start);
  if (!expect(p, T_LPAREN, "'(' after during")) {
    freeNode(n);
    return NULL;
  }
  n->a = parseExpression(p);
  if (!n->a || !expect(p, T_RPAREN, "')' after during condition")) {
    freeNode(n);
    return NULL;
  }
  Node *body = parseBlock(p);
  if (!body) {
    freeNode(n);
    return NULL;
  }
  n->items = body->items;
  n->count = body->count;
  n->itemCapacity = body->itemCapacity;
  body->items = NULL;
  body->count = 0;
  freeNode(body);
  return n;
}

static Node *parseFor(Parser *p) {
  Token *start = take(p);
  Node *n = nodeNew(N_FOR, start);
  if (!expect(p, T_LPAREN, "'(' after repeat")) {
    freeNode(n);
    return NULL;
  }
  if (tokenIsType(p, peek(p)) || peek(p)->type == T_CONST ||
      peek(p)->type == T_STATIC || peek(p)->type == T_GLOBAL)
    n->a = parseTypeDeclaration(p, 0, 0);
  else {
    Node *lhs = parseExpression(p);
    if (!lhs) {
      freeNode(n);
      return NULL;
    }
    if (peek(p) && isAssignmentToken(peek(p)->type)) {
      Token *op = take(p);
      Node *as = nodeNew(N_ASSIGN, op);
      as->text = xstrdup(op->value);
      as->a = lhs;
      as->b = parseExpression(p);
      n->a = as;
    } else
      n->a = lhs;
  }
  if (!expect(p, T_COMMA, "',' after repeat initializer")) {
    freeNode(n);
    return NULL;
  }
  n->b = parseExpression(p);
  if (!n->b || !expect(p, T_COMMA, "',' after repeat condition")) {
    freeNode(n);
    return NULL;
  }
  Node *upd = parseExpression(p);
  if (!upd) {
    freeNode(n);
    return NULL;
  }
  if (peek(p) && isAssignmentToken(peek(p)->type)) {
    Token *op = take(p);
    Node *as = nodeNew(N_ASSIGN, op);
    as->text = xstrdup(op->value);
    as->a = upd;
    as->b = parseExpression(p);
    n->c = as;
  } else
    n->c = upd;
  if (!expect(p, T_RPAREN, "')' after repeat clauses")) {
    freeNode(n);
    return NULL;
  }
  Node *body = parseBlock(p);
  if (!body) {
    freeNode(n);
    return NULL;
  }
  n->items = body->items;
  n->count = body->count;
  n->itemCapacity = body->itemCapacity;
  body->items = NULL;
  body->count = 0;
  freeNode(body);
  return n;
}

static Node *parseSwitch(Parser *p) {
  Token *start = take(p);
  Node *n = nodeNew(N_SWITCH, start);
  if (match(p, T_LPAREN)) {
    n->a = parseExpression(p);
    if (!n->a || !expect(p, T_RPAREN, "')' after switch value")) {
      freeNode(n);
      return NULL;
    }
  } else
    n->a = parseExpression(p);
  if (!n->a || !expect(p, T_LBRACE, "'{' after switch value")) {
    freeNode(n);
    return NULL;
  }
  int sawDefault = 0;
  while (peek(p) && peek(p)->type != T_RBRACE && peek(p)->type != T_EOF) {
    Token *label = peek(p);
    if (match(p, T_ARG)) {
      Node *cs = nodeNew(N_CASE, label);
      cs->a = parseExpression(p);
      if (!cs->a || !expect(p, T_COLON, "':' after case value")) {
        freeNode(cs);
        freeNode(n);
        return NULL;
      }
      while (peek(p) && peek(p)->type != T_ARG && peek(p)->type != T_INCASE &&
             peek(p)->type != T_RBRACE) {
        Node *st = parseStatement(p);
        if (!st) {
          freeNode(cs);
          freeNode(n);
          return NULL;
        }
        nodeAdd(cs, st);
      }
      nodeAdd(n, cs);
    } else if (match(p, T_INCASE)) {
      if (sawDefault) {
        parseErrorAt(p, label, "E1011",
                     "switch may contain only one incase default branch");
        freeNode(n);
        return NULL;
      }
      sawDefault = 1;
      Node *cs = nodeNew(N_CASE, label);
      if (!expect(p, T_COLON, "':' after incase")) {
        freeNode(cs);
        freeNode(n);
        return NULL;
      }
      while (peek(p) && peek(p)->type != T_ARG && peek(p)->type != T_INCASE &&
             peek(p)->type != T_RBRACE) {
        Node *st = parseStatement(p);
        if (!st) {
          freeNode(cs);
          freeNode(n);
          return NULL;
        }
        nodeAdd(cs, st);
      }
      nodeAdd(n, cs);
    } else {
      parseErrorAt(p, label, "E1012",
                   "expected arg or incase inside switch, found '%s'",
                   label->value);
      freeNode(n);
      return NULL;
    }
  }
  if (!expect(p, T_RBRACE, "'}' after switch")) {
    freeNode(n);
    return NULL;
  }
  return n;
}

static Node *parseBlock(Parser *p) {
  Token *start = expect(p, T_LBRACE, "'{' to start a block");
  if (!start)
    return NULL;
  Node *block = nodeNew(N_BLOCK, start);
  while (peek(p) && peek(p)->type != T_RBRACE && peek(p)->type != T_EOF) {
    Node *st = parseStatement(p);
    if (!st) {
      freeNode(block);
      return NULL;
    }
    nodeAdd(block, st);
  }
  if (!expect(p, T_RBRACE, "'}' to close the block")) {
    freeNode(block);
    return NULL;
  }
  return block;
}

static Node *parseStatement(Parser *p) {
  Token *t = peek(p);
  if (!t || t->type == T_EOF)
    return NULL;
  if (t->type == T_LBRACE)
    return parseBlock(p);
  if (t->type == T_IF)
    return parseIf(p);
  if (t->type == T_DURING)
    return parseWhile(p);
  if (t->type == T_REPEAT)
    return parseFor(p);
  if (t->type == T_SWITCH)
    return parseSwitch(p);
  if (t->type == T_DATA)
    return parseData(p);
  if (t->type == T_ALIAS)
    return parseAlias(p);
  if (t->type == T_SET)
    return parseEnum(p, 0);
  if (t->type == T_RETREAT) {
    take(p);
    Node *n = nodeNew(N_RETURN, t);
    if (peek(p) && peek(p)->type != T_RBRACE && peek(p)->type != T_EOF &&
        peek(p)->type != T_ARG && peek(p)->type != T_INCASE)
      n->a = parseExpression(p);
    return n;
  }
  if (t->type == T_KILL) {
    take(p);
    Node *n = nodeNew(N_KILL, t);
    while (match(p, T_SUB)) {
      Token *flag = expect(p, T_ID, "a kill flag such as silent or error");
      if (!flag) {
        freeNode(n);
        return NULL;
      }
      if (!n->text)
        n->text = xstrdup(flag->value);
    }
    return n;
  }
  if (t->type == T_BREAK) {
    take(p);
    return nodeNew(N_BREAK, t);
  }
  if (t->type == T_CONT) {
    take(p);
    return nodeNew(N_CONTINUE, t);
  }
  if (t->type == T_ATTEMPT) {
    take(p);
    Node *body = parseBlock(p);
    if (!body)
      return NULL;
    if (match(p, T_INCASE)) {
      Node *fallback = parseBlock(p);
      freeNode(body);
      freeNode(fallback);
      parseErrorAt(p, t, "E1015",
                   "attempt/incase exception handling is not implemented yet; "
                   "use if/elif/else for supported control flow");
      return NULL;
    }
    freeNode(body);
    parseErrorAt(p, t, "E1015",
                 "attempt exception handling is not implemented yet; use "
                 "if/elif/else for supported control flow");
    return NULL;
  }
  if (t->type == T_CONST || t->type == T_STATIC || t->type == T_GLOBAL ||
      tokenIsType(p, t))
    return parseTypeDeclaration(p, 0, 1);
  if (t->type == T_ID && look(p, 1) &&
      (look(p, 1)->type == T_ID || look(p, 1)->type == T_ARROW ||
       look(p, 1)->type == T_DARROW))
    return parseTypeDeclaration(p, 0, 1);

  Node *lhs = parseExpression(p);
  if (!lhs)
    return NULL;
  if (peek(p) && isAssignmentToken(peek(p)->type)) {
    Token *op = take(p);
    Node *n = nodeNew(N_ASSIGN, op);
    n->text = xstrdup(op->value);
    n->a = lhs;
    n->b = parseExpression(p);
    if (!n->b) {
      freeNode(n);
      return NULL;
    }
    return n;
  }
  Node *stmt = nodeNew(N_EXPR_STMT, t);
  stmt->a = lhs;
  return stmt;
}

static Node *parseTopLevel(Parser *p) {
  Node *program = nodeNew(N_PROGRAM, peek(p));
  while (peek(p) && peek(p)->type != T_EOF) {
    Token *t = peek(p);
    /* Import directives are line-oriented. Built-in modules are linked by
     * runtime/codegen. */
    if (match(p, T_QUESTION) || match(p, T_WITH)) {
      size_t line = t->line;
      while (peek(p) && peek(p)->type != T_EOF && peek(p)->line == line)
        take(p);
      continue;
    }
    if (t->type == T_DATA) {
      Node *n = parseData(p);
      if (!n) {
        freeNode(program);
        return NULL;
      }
      Node **fields =
          (Node **)malloc((n->count ? n->count : 1) * sizeof(Node *));
      if (!fields) {
        freeNode(n);
        freeNode(program);
        fprintf(stderr, "fatal: out of memory\n");
        exit(2);
      }
      size_t fieldCount = 0;
      for (size_t k = 0; k < n->count; ++k) {
        if (n->items[k]->kind == N_FUNCTION)
          nodeAdd(program, n->items[k]);
        else
          fields[fieldCount++] = n->items[k];
      }
      free(n->items);
      n->items = fields;
      n->count = fieldCount;
      n->itemCapacity = fieldCount;
      nodeAdd(program, n);
      continue;
    }
    if (t->type == T_ALIAS) {
      Node *n = parseAlias(p);
      if (!n) {
        freeNode(program);
        return NULL;
      }
      nodeAdd(program, n);
      continue;
    }
    if (t->type == T_SET) {
      Node *n = parseEnum(p, 0);
      if (!n) {
        freeNode(program);
        return NULL;
      }
      nodeAdd(program, n);
      continue;
    }
    if (t->type == T_CONST || t->type == T_STATIC || t->type == T_GLOBAL ||
        tokenIsType(p, t) ||
        (t->type == T_ID && look(p, 1) &&
         (look(p, 1)->type == T_ID || look(p, 1)->type == T_ARROW ||
          look(p, 1)->type == T_DARROW))) {
      Node *n = parseTypeDeclaration(p, 1, 1);
      if (!n) {
        freeNode(program);
        return NULL;
      }
      nodeAdd(program, n);
      continue;
    }
    /* Top-level executable statements are wrapped in an implicit main body. */
    Node *st = parseStatement(p);
    if (!st) {
      freeNode(program);
      return NULL;
    }
    nodeAdd(program, st);
  }
  if (p->failed) {
    freeNode(program);
    return NULL;
  }
  return program;
}

/* -------------------------- validation / typing ------------------------- */

typedef struct {
  char *name;
  char *type;
  int pointerDepth;
  int isArray;
  size_t capacity;
} VarInfo;
typedef struct {
  char *name;
  char *returnType;
  char *ownerType;
  char *methodName;
  int pointerDepth;
  size_t paramCount;
  Node *node;
} FuncInfo;
typedef struct {
  VarInfo *vars;
  size_t varCount, varCap;
  FuncInfo *funcs;
  size_t funcCount, funcCap;
  Node *program;
  int errors;
} Model;

static int addVarInfo(Model *m, const char *name, const char *type, int ptr,
                      int array, size_t cap, Token *at) {
  for (size_t i = 0; i < m->varCount; ++i)
    if (strcmp(m->vars[i].name, name) == 0) {
      fprintf(stderr,
              "error[E2001]: '%s' is already declared in this translation "
              "unit\n  --> %s:%zu:%zu\n",
              name, at->file, at->line, at->column);
      m->errors++;
      return 0;
    }
  if (m->varCount == m->varCap) {
    m->varCap = m->varCap ? m->varCap * 2 : 32;
    m->vars = (VarInfo *)xrealloc(m->vars, m->varCap * sizeof(VarInfo));
  }
  m->vars[m->varCount++] =
      (VarInfo){xstrdup(name), xstrdup(type), ptr, array, cap};
  return 1;
}

static int addFuncInfo(Model *m, Node *n) {
  for (size_t i = 0; i < m->funcCount; ++i)
    if (strcmp(m->funcs[i].name, n->name) == 0) {
      /* A declaration followed by one definition is permitted. */
      if (m->funcs[i].node && n->a) {
        fprintf(stderr,
                "error[E2002]: function '%s' has more than one definition\n  "
                "--> %s:%zu:%zu\n",
                n->name, n->file, n->line, n->column);
        m->errors++;
        return 0;
      }
      if (n->a)
        m->funcs[i].node = n;
      return 1;
    }
  if (m->funcCount == m->funcCap) {
    m->funcCap = m->funcCap ? m->funcCap * 2 : 16;
    m->funcs = (FuncInfo *)xrealloc(m->funcs, m->funcCap * sizeof(FuncInfo));
  }
  m->funcs[m->funcCount++] =
      (FuncInfo){xstrdup(n->name),
                 xstrdup(n->type),
                 n->ownerType ? xstrdup(n->ownerType) : NULL,
                 n->methodName ? xstrdup(n->methodName) : NULL,
                 n->pointerDepth,
                 n->count,
                 n};
  return 1;
}

static FuncInfo *findFunction(Model *m, const char *name) {
  for (size_t i = 0; i < m->funcCount; ++i)
    if (!strcmp(m->funcs[i].name, name))
      return &m->funcs[i];
  return NULL;
}
static FuncInfo *findMethod(Model *m, const char *owner, const char *method) {
  for (size_t i = 0; i < m->funcCount; ++i)
    if (m->funcs[i].ownerType && m->funcs[i].methodName &&
        !strcmp(m->funcs[i].ownerType, owner) &&
        !strcmp(m->funcs[i].methodName, method))
      return &m->funcs[i];
  return NULL;
}
static VarInfo *findVariable(Model *m, const char *name) {
  for (size_t i = 0; i < m->varCount; ++i)
    if (!strcmp(m->vars[i].name, name))
      return &m->vars[i];
  return NULL;
}

static void collectVariableTree(Model *m, Node *n) {
  if (!n)
    return;
  if (n->kind == N_VAR) {
    Token fake = {0};
    fake.file = n->file;
    fake.line = n->line;
    fake.column = n->column;
    addVarInfo(m, n->name, n->type, n->pointerDepth, n->arrayCount != 0,
               n->capacity, &fake);
  }
  for (size_t i = 0; i < n->count; ++i)
    collectVariableTree(m, n->items[i]);
}

static int collectTopLevel(Model *m, Node *program) {
  m->program = program;
  for (size_t i = 0; i < program->count; ++i) {
    Node *n = program->items[i];
    if (n->kind == N_FUNCTION)
      addFuncInfo(m, n);
    else if (n->kind == N_VAR) {
      collectVariableTree(m, n);
    }
  }
  return m->errors == 0;
}

static const char *baseTypeName(const char *t) {
  if (!t)
    return "int";
  if (strncmp(t, "int", 3) == 0)
    return "int";
  if (strncmp(t, "fp", 2) == 0)
    return "fp";
  if (strncmp(t, "str", 3) == 0)
    return "str";
  return t;
}

static Node *findLocalDeclaration(Node *n, const char *name) {
  if (!n)
    return NULL;
  if (n->kind == N_VAR && n->name && strcmp(n->name, name) == 0)
    return n;
  for (size_t i = 0; i < n->count; ++i) {
    Node *found = findLocalDeclaration(n->items[i], name);
    if (found)
      return found;
  }
  Node *found = findLocalDeclaration(n->a, name);
  if (found)
    return found;
  found = findLocalDeclaration(n->b, name);
  if (found)
    return found;
  found = findLocalDeclaration(n->c, name);
  return found;
}

static Node *findRecordField(Model *m, const char *typeName,
                             const char *fieldName);

static const char *inferType(Node *n, Model *m, Node *currentFunction) {
  if (!n)
    return "void";
  switch (n->kind) {
  case X_NUMBER:
    return (strchr(n->text, '.') || strchr(n->text, 'e') ||
            strchr(n->text, 'E'))
               ? "fp"
               : "int";
  case X_STRING:
    return "str";
  case X_BOOL:
    return "bool";
  case X_NULL:
    return "pointer";
  case X_IDENTIFIER: {
    VarInfo *v = findVariable(m, n->name);
    if (v)
      return v->pointerDepth ? "pointer" : baseTypeName(v->type);
    if (currentFunction) {
      for (size_t i = 0; i < currentFunction->count; ++i) {
        Node *param = currentFunction->items[i];
        if (param->name && !strcmp(param->name, n->name))
          return param->pointerDepth ? "pointer" : baseTypeName(param->type);
      }
      Node *local = findLocalDeclaration(currentFunction->a, n->name);
      if (local)
        return local->pointerDepth ? "pointer" : baseTypeName(local->type);
    }
    FuncInfo *f = findFunction(m, n->name);
    if (f)
      return f->returnType;
    return "int";
  }
  case X_UNARY:
    if (!strcmp(n->text, "&"))
      return "pointer";
    if (!strcmp(n->text, "*") || !strcmp(n->text, "**"))
      return "int";
    return inferType(n->a, m, currentFunction);
  case X_BINARY: {
    const char *l = inferType(n->a, m, currentFunction),
               *r = inferType(n->b, m, currentFunction);
    if (!strcmp(l, "fp") || !strcmp(r, "fp"))
      return "fp";
    if (!strcmp(n->text, "==") || !strcmp(n->text, "!=") ||
        !strcmp(n->text, "<") || !strcmp(n->text, ">") ||
        !strcmp(n->text, "<=") || !strcmp(n->text, ">=") ||
        !strcmp(n->text, "&&") || !strcmp(n->text, "||"))
      return "bool";
    return l;
  }
  case X_INDEX: {
    const char *t = inferType(n->a, m, currentFunction);
    return t;
  }
  case X_CALL:
    if (n->a && n->a->kind == X_MEMBER && n->a->a &&
        n->a->a->kind == X_IDENTIFIER) {
      const char *mod = n->a->a->name, *fn = n->a->name;
      if (!strcmp(mod, "io")) {
        if (!strcmp(fn, "in"))
          return "str";
        if (!strcmp(fn, "out") || !strcmp(fn, "errout") || !strcmp(fn, "nl"))
          return "void";
      }
      if (!strcmp(mod, "math"))
        return "fp";
    }
    if (n->a && n->a->kind == X_IDENTIFIER) {
      FuncInfo *f = findFunction(m, n->a->name);
      if (f)
        return baseTypeName(f->returnType);
    }
    return "int";
  case X_MEMBER: {
    const char *recordType = NULL;
    if (n->a && n->a->kind == X_IDENTIFIER) {
      VarInfo *v = findVariable(m, n->a->name);
      if (v)
        recordType = baseTypeName(v->type);
      if (!recordType && currentFunction) {
        for (size_t i = 0; i < currentFunction->count; ++i)
          if (currentFunction->items[i]->name &&
              !strcmp(currentFunction->items[i]->name, n->a->name))
            recordType = baseTypeName(currentFunction->items[i]->type);
        Node *local = findLocalDeclaration(currentFunction->a, n->a->name);
        if (local)
          recordType = baseTypeName(local->type);
      }
    }
    if (recordType && m->program)
      for (size_t i = 0; i < m->program->count; ++i) {
        Node *record = m->program->items[i];
        if (record->kind == N_DATA && record->name &&
            !strcmp(record->name, recordType)) {
          Node *field = findRecordField(m, recordType, n->name);
          if (field)
            return field->pointerDepth ? "pointer" : baseTypeName(field->type);
        }
      }
    return "int";
  }
  default:
    return "int";
  }
}

static Node *findRecord(Model *m, const char *typeName) {
  if (!m->program || !typeName)
    return NULL;
  for (size_t i = 0; i < m->program->count; ++i) {
    Node *n = m->program->items[i];
    if (n->kind == N_DATA && n->name && !strcmp(n->name, typeName))
      return n;
  }
  return NULL;
}

static Node *findFieldInTree(Node *n, const char *fieldName) {
  if (!n)
    return NULL;
  if (n->kind == N_VAR && n->name && !strcmp(n->name, fieldName))
    return n;
  for (size_t i = 0; i < n->count; ++i) {
    Node *found = findFieldInTree(n->items[i], fieldName);
    if (found)
      return found;
  }
  return NULL;
}

static Node *findRecordField(Model *m, const char *typeName,
                             const char *fieldName) {
  Node *record = findRecord(m, typeName);
  if (!record)
    return NULL;
  for (size_t i = 0; i < record->count; ++i) {
    Node *found = findFieldInTree(record->items[i], fieldName);
    if (found)
      return found;
  }
  return NULL;
}

static Node *findCurrentVariable(Node *expression, Model *m, Node *fn) {
  (void)m;
  if (!expression || expression->kind != X_IDENTIFIER)
    return NULL;
  if (fn) {
    for (size_t i = 0; i < fn->count; ++i)
      if (fn->items[i]->name && !strcmp(fn->items[i]->name, expression->name))
        return fn->items[i];
    Node *local = findLocalDeclaration(fn->a, expression->name);
    if (local)
      return local;
  }
  return NULL;
}

static int isStringBufferLvalue(Node *n, Model *m, Node *fn) {
  if (!n)
    return 0;
  if (n->kind == X_IDENTIFIER) {
    Node *local = findCurrentVariable(n, m, fn);
    if (local)
      return local->capacity > 0 && !local->pointerDepth;
    VarInfo *global = findVariable(m, n->name);
    return global && global->capacity > 0 && !global->pointerDepth;
  }
  if (n->kind == X_MEMBER && n->a && n->a->kind == X_IDENTIFIER) {
    Node *base = findCurrentVariable(n->a, m, fn);
    const char *type = base ? base->type : NULL;
    if (!type) {
      VarInfo *global = findVariable(m, n->a->name);
      if (global)
        type = global->type;
    }
    Node *field = findRecordField(m, baseTypeName(type), n->name);
    return field && field->capacity > 0 && !field->pointerDepth;
  }
  if (n->kind == X_INDEX && n->a && n->a->kind == X_IDENTIFIER) {
    Node *base = findCurrentVariable(n->a, m, fn);
    if (base)
      return base->capacity > 0 && base->arrayCount > 0;
    VarInfo *global = findVariable(m, n->a->name);
    return global && global->capacity > 0 && global->isArray;
  }
  return 0;
}

static int isKnownBuiltinCall(Node *callee) {
  if (!callee)
    return 0;
  if (callee->kind == X_IDENTIFIER) {
    return strcmp(callee->name, "out") == 0 ||
           strcmp(callee->name, "in") == 0 || strcmp(callee->name, "nl") == 0;
  }
  if (callee->kind == X_MEMBER && callee->a &&
      callee->a->kind == X_IDENTIFIER) {
    const char *m = callee->a->name, *f = callee->name;
    return (!strcmp(m, "io") && (!strcmp(f, "out") || !strcmp(f, "in") ||
                                 !strcmp(f, "nl") || !strcmp(f, "errout"))) ||
           (!strcmp(m, "math") &&
            (strcmp(f, "sqrt") == 0 || strcmp(f, "abs") == 0 ||
             strcmp(f, "min") == 0 || strcmp(f, "max") == 0 ||
             strcmp(f, "round") == 0 || strcmp(f, "rdown") == 0 ||
             strcmp(f, "rup") == 0 || strcmp(f, "sin") == 0 ||
             strcmp(f, "cos") == 0 || strcmp(f, "tan") == 0 ||
             strcmp(f, "asin") == 0 || strcmp(f, "acos") == 0 ||
             strcmp(f, "atan") == 0 || strcmp(f, "log") == 0 ||
             strcmp(f, "logx") == 0 || strcmp(f, "exp") == 0 ||
             strcmp(f, "pi") == 0 || strcmp(f, "clamp") == 0 ||
             strcmp(f, "sign") == 0 || strcmp(f, "lerp") == 0));
  }
  return 0;
}

static void validateExpr(Node *n, Model *m) {
  if (!n)
    return;
  if (n->kind == X_CALL) {
    if (n->a && n->a->kind == X_IDENTIFIER) {
      if (!findFunction(m, n->a->name) && !isKnownBuiltinCall(n->a)) {
        fprintf(stderr,
                "error[E2003]: call to undeclared function '%s'\n  --> "
                "%s:%zu:%zu\n",
                n->a->name, n->file, n->line, n->column);
        m->errors++;
      }
    } else if (n->a && n->a->kind == X_MEMBER && n->a->a &&
               n->a->a->kind == X_IDENTIFIER &&
               (!strcmp(n->a->a->name, "io") ||
                !strcmp(n->a->a->name, "math")) &&
               !isKnownBuiltinCall(n->a)) {
      const char *full = formatString("%s.%s", n->a->a->name, n->a->name);
      fprintf(stderr,
              "error[E2003]: call to unknown native function '%s' (import its "
              "module and check its API name)\n  --> %s:%zu:%zu\n",
              full, n->file, n->line, n->column);
      free((void *)full);
      m->errors++;
    }
    for (size_t i = 0; i < n->count; ++i)
      validateExpr(n->items[i], m);
    validateExpr(n->a, m);
    return;
  }
  validateExpr(n->a, m);
  validateExpr(n->b, m);
  validateExpr(n->c, m);
  for (size_t i = 0; i < n->count; ++i)
    validateExpr(n->items[i], m);
}

static void validateNode(Node *n, Model *m) {
  if (!n)
    return;
  switch (n->kind) {
  case N_FUNCTION:
    for (size_t i = 0; i < n->count; ++i) {
      Node *param = n->items[i];
      /* Parameter names live in a function-local namespace, so they are not
       * added globally. */
      (void)param;
    }
    validateNode(n->a, m);
    break;
  case N_VAR:
    if (n->a) {
      if (!strcmp(inferType(n->a, m, NULL), "void")) {
        fprintf(stderr,
                "error[E2006]: cannot initialize '%s' with a void expression\n"
                "  --> %s:%zu:%zu\n",
                n->name, n->file, n->line, n->column);
        m->errors++;
      }
      validateExpr(n->a, m);
    }
    for (size_t i = 0; i < n->count; ++i)
      validateNode(n->items[i], m);
    break;
  case N_ASSIGN:
    if (n->a && n->a->kind != X_IDENTIFIER && n->a->kind != X_INDEX &&
        n->a->kind != X_MEMBER && n->a->kind != X_UNARY) {
      fprintf(stderr,
              "error[E2004]: left side of assignment is not writable\n  --> "
              "%s:%zu:%zu\n",
              n->file, n->line, n->column);
      m->errors++;
    }
    if (n->b && !strcmp(inferType(n->b, m, NULL), "void")) {
      fprintf(stderr,
              "error[E2006]: cannot assign the result of a void expression\n"
              "  --> %s:%zu:%zu\n",
              n->file, n->line, n->column);
      m->errors++;
    }
    validateExpr(n->a, m);
    validateExpr(n->b, m);
    break;
  case N_BLOCK:
  case N_PROGRAM:
  case N_WHILE:
  case N_FOR:
  case N_SWITCH:
    validateExpr(n->a, m);
    validateExpr(n->b, m);
    validateExpr(n->c, m);
    for (size_t i = 0; i < n->count; ++i)
      validateNode(n->items[i], m);
    break;
  case N_IF:
    validateExpr(n->a, m);
    validateNode(n->b, m);
    validateNode(n->c, m);
    break;
  case N_CASE:
    validateExpr(n->a, m);
    for (size_t i = 0; i < n->count; ++i)
      validateNode(n->items[i], m);
    break;
  case N_RETURN:
  case N_EXPR_STMT:
  case N_KILL:
    validateExpr(n->a, m);
    break;
  default:
    break;
  }
}

/* ----------------------------- C generation ----------------------------- */

typedef struct {
  FILE *out;
  Model *model;
  Node *currentFunction;
  const char *functionName;
  const char *functionType;
  int indent;
  int loopDepth;
  int switchDepth;
  int errors;
} Codegen;

static void emitCFunctionName(Codegen *g, const char *name) {
  if (!strcmp(name, "main"))
    fputs("main", g->out);
  else
    fprintf(g->out, "__uo_fn_%s", name);
}

static void indent(Codegen *g) {
  for (int i = 0; i < g->indent; ++i)
    fputs("    ", g->out);
}

static const char *cType(const char *type) {
  if (!type)
    return "int";
  if (!strcmp(type, "int") || !strncmp(type, "int", 3)) {
    if (!strcmp(type, "int8"))
      return "int8_t";
    if (!strcmp(type, "int16"))
      return "int16_t";
    if (!strcmp(type, "int32"))
      return "int32_t";
    if (!strcmp(type, "int64"))
      return "int64_t";
    return "int";
  }
  if (!strcmp(type, "fp"))
    return "double";
  if (!strcmp(type, "fp8") || !strcmp(type, "fp16"))
    return "double";
  if (!strcmp(type, "fp32"))
    return "float";
  if (!strcmp(type, "fp64"))
    return "double";
  if (!strcmp(type, "str") || !strncmp(type, "str", 3))
    return "char";
  if (!strcmp(type, "bool"))
    return "bool";
  if (!strcmp(type, "void"))
    return "void";
  return type;
}

static void emitCString(FILE *out, const char *s) {
  fputc('"', out);
  for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
    switch (*p) {
    case '\\':
      fputs("\\\\", out);
      break;
    case '"':
      fputs("\\\"", out);
      break;
    case '\n':
      fputs("\\n", out);
      break;
    case '\r':
      fputs("\\r", out);
      break;
    case '\t':
      fputs("\\t", out);
      break;
    default:
      if (*p < 32 || *p >= 127)
        fprintf(out, "\\x%02x", *p);
      else
        fputc(*p, out);
      break;
    }
  }
  fputc('"', out);
}

static void emitExpr(Codegen *g, Node *n);

static void emitBinaryAsC(Codegen *g, Node *n) {
  const char *op = n->text;
  if (!strcmp(op, "~"))
    op = "^"; /* Underscore's single tilde is xor in conditions. */
  fputc('(', g->out);
  emitExpr(g, n->a);
  fprintf(g->out, " %s ", op);
  emitExpr(g, n->b);
  fputc(')', g->out);
}

static const char *mathCName(const char *name) {
  if (!strcmp(name, "sqrt"))
    return "__uo_sqrt";
  if (!strcmp(name, "abs"))
    return "fabs";
  if (!strcmp(name, "min"))
    return "fmin";
  if (!strcmp(name, "max"))
    return "fmax";
  if (!strcmp(name, "round"))
    return "round";
  if (!strcmp(name, "rdown"))
    return "floor";
  if (!strcmp(name, "rup"))
    return "ceil";
  if (!strcmp(name, "sin"))
    return "sin";
  if (!strcmp(name, "cos"))
    return "cos";
  if (!strcmp(name, "tan"))
    return "tan";
  if (!strcmp(name, "asin"))
    return "asin";
  if (!strcmp(name, "acos"))
    return "acos";
  if (!strcmp(name, "atan"))
    return "atan";
  if (!strcmp(name, "log"))
    return "log";
  if (!strcmp(name, "exp"))
    return "exp";
  if (!strcmp(name, "clamp"))
    return "__uo_clamp";
  if (!strcmp(name, "sign"))
    return "__uo_sign";
  if (!strcmp(name, "lerp"))
    return "__uo_lerp";
  return name;
}

static void emitCall(Codegen *g, Node *n) {
  Node *callee = n->a;
  if (callee && callee->kind == X_MEMBER && callee->a &&
      callee->a->kind == X_IDENTIFIER) {
    const char *module = callee->a->name, *name = callee->name;
    if (!strcmp(module, "io") && !strcmp(name, "in")) {
      fputs("__io_InCall()", g->out);
      return;
    }
    if (!strcmp(module, "io") && !strcmp(name, "nl")) {
      fputs("__io_NlCall()", g->out);
      return;
    }
    if ((!strcmp(module, "io") && !strcmp(name, "out")) ||
        (!strcmp(module, "io") && !strcmp(name, "errout"))) {
      if (n->count != 1) {
        fprintf(stderr,
                "error[E3001]: io.%s expects exactly one argument\n  --> "
                "%s:%zu:%zu\n",
                name, n->file, n->line, n->column);
        g->errors++;
        fputs("0", g->out);
        return;
      }
      Node *arg = n->items[0];
      const char *type = inferType(arg, g->model, g->currentFunction);
      if (!strcmp(type, "str"))
        fputs("__io_OutCall(", g->out);
      else if (!strcmp(type, "fp"))
        fputs("__io_OutFloat(", g->out);
      else if (!strcmp(type, "bool"))
        fputs("__io_OutBool(", g->out);
      else if (!strcmp(type, "pointer"))
        fputs("__io_OutPointer(", g->out);
      else
        fputs("__io_OutInt(", g->out);
      emitExpr(g, arg);
      fputc(')', g->out);
      return;
    }
    if (!strcmp(module, "math")) {
      if (!strcmp(name, "pi")) {
        fputs("3.14159265358979323846", g->out);
        return;
      }
      if (!strcmp(name, "logx")) {
        if (n->count != 2) {
          fputs("0.0", g->out);
          g->errors++;
          return;
        }
        fputs("(log(", g->out);
        emitExpr(g, n->items[0]);
        fputs(") / log(", g->out);
        emitExpr(g, n->items[1]);
        fputs("))", g->out);
        return;
      }
      fprintf(g->out, "%s(", mathCName(name));
      for (size_t i = 0; i < n->count; ++i) {
        if (i)
          fputs(", ", g->out);
        emitExpr(g, n->items[i]);
      }
      fputc(')', g->out);
      return;
    }
    /* Methods declared inside data records lower to functions with an implicit
     * self pointer. */
    if (callee->a && callee->a->kind == X_IDENTIFIER) {
      Node *receiver =
          findCurrentVariable(callee->a, g->model, g->currentFunction);
      const char *receiverType = receiver ? receiver->type : NULL;
      int receiverIsPointer = receiver ? receiver->pointerDepth > 0 : 0;
      if (!receiverType) {
        VarInfo *global = findVariable(g->model, callee->a->name);
        if (global) {
          receiverType = global->type;
          receiverIsPointer = global->pointerDepth > 0;
        }
      }
      FuncInfo *method =
          receiverType ? findMethod(g->model, receiverType, name) : NULL;
      if (method) {
        emitCFunctionName(g, method->name);
        fputc('(', g->out);
        if (!receiverIsPointer)
          fputc('&', g->out);
        emitExpr(g, callee->a);
        for (size_t i = 0; i < n->count; ++i) {
          fputs(", ", g->out);
          emitExpr(g, n->items[i]);
        }
        fputc(')', g->out);
        return;
      }
    }
  }
  if (callee && callee->kind == X_IDENTIFIER &&
      (!strcmp(callee->name, "out") || !strcmp(callee->name, "in") ||
       !strcmp(callee->name, "nl"))) {
    if (!strcmp(callee->name, "in"))
      fputs("__io_InCall()", g->out);
    else if (!strcmp(callee->name, "nl"))
      fputs("__io_NlCall()", g->out);
    else if (n->count == 1) {
      const char *type = inferType(n->items[0], g->model, g->currentFunction);
      fputs(!strcmp(type, "str")
                ? "__io_OutCall("
                : (!strcmp(type, "fp") ? "__io_OutFloat(" : "__io_OutInt("),
            g->out);
      emitExpr(g, n->items[0]);
      fputc(')', g->out);
    } else
      fputs("0", g->out);
    return;
  }
  emitExpr(g, callee);
  fputc('(', g->out);
  for (size_t i = 0; i < n->count; ++i) {
    if (i)
      fputs(", ", g->out);
    emitExpr(g, n->items[i]);
  }
  fputc(')', g->out);
}

static void emitExpr(Codegen *g, Node *n) {
  if (!n) {
    fputs("0", g->out);
    return;
  }
  switch (n->kind) {
  case X_IDENTIFIER:
    if (findFunction(g->model, n->name))
      emitCFunctionName(g, n->name);
    else
      fputs(n->name, g->out);
    break;
  case X_NUMBER:
    fputs(n->text, g->out);
    break;
  case X_STRING:
    emitCString(g->out, n->text);
    break;
  case X_BOOL:
    fputs(!strcmp(n->text, "true") ? "true" : "false", g->out);
    break;
  case X_NULL:
    fputs("NULL", g->out);
    break;
  case X_BINARY:
    emitBinaryAsC(g, n);
    break;
  case X_UNARY:
    if (!strncmp(n->text, "post", 4)) {
      fputc('(', g->out);
      emitExpr(g, n->a);
      fputs(n->text + 4, g->out);
      fputc(')', g->out);
    } else {
      fputc('(', g->out);
      fputs(n->text, g->out);
      emitExpr(g, n->a);
      fputc(')', g->out);
    }
    break;
  case X_CALL:
    emitCall(g, n);
    break;
  case X_MEMBER:
    emitExpr(g, n->a);
    fputs(n->text && !strcmp(n->text, "~>") ? "->" : ".", g->out);
    fputs(n->name, g->out);
    break;
  case X_INDEX:
    emitExpr(g, n->a);
    fputc('[', g->out);
    emitExpr(g, n->b);
    fputc(']', g->out);
    break;
  case X_ARRAY:
    fputc('{', g->out);
    for (size_t i = 0; i < n->count; ++i) {
      if (i)
        fputs(", ", g->out);
      emitExpr(g, n->items[i]);
    }
    fputc('}', g->out);
    break;
  default:
    fputs("0", g->out);
    break;
  }
}

static int isIoInputCall(Node *n) {
  return n && n->kind == X_CALL && n->a && n->a->kind == X_MEMBER && n->a->a &&
         n->a->a->kind == X_IDENTIFIER && !strcmp(n->a->a->name, "io") &&
         !strcmp(n->a->name, "in");
}

static void emitTypedExpr(Codegen *g, const char *targetType, Node *value) {
  if (isIoInputCall(value)) {
    const char *type = baseTypeName(targetType);
    if (!strcmp(type, "fp")) {
      fputs("strtod(__io_InCall(), NULL)", g->out);
      return;
    }
    if (!strcmp(type, "int")) {
      fputs("strtoll(__io_InCall(), NULL, 10)", g->out);
      return;
    }
    if (!strcmp(type, "bool")) {
      fputs("(strtol(__io_InCall(), NULL, 10) != 0)", g->out);
      return;
    }
  }
  emitExpr(g, value);
}

static void emitSingleDeclaration(Codegen *g, Node *n, int global) {
  indent(g);
  if (n->isConst)
    fputs("const ", g->out);
  if (n->isStatic)
    fputs("static ", g->out);
  if (n->isGlobal && !global)
    fputs("extern ", g->out);
  if (!strcmp(n->type, "str") || !strncmp(n->type, "str", 3)) {
    if (n->pointerDepth > 0) {
      fputs("char ", g->out);
      for (int i = 0; i < n->pointerDepth + 1; ++i)
        fputc('*', g->out);
      fprintf(g->out, "%s", n->name);
      if (n->a) {
        fputs(" = ", g->out);
        emitExpr(g, n->a);
      } else if (global)
        fputs(" = NULL", g->out);
      fputs(";\n", g->out);
      return;
    }
    if (n->capacity > 0 && n->arrayCount > 0) {
      fprintf(g->out, "char %s[%zu][%zu]", n->name, n->arrayCount,
              n->capacity + 1);
      if (n->a && n->a->kind == X_ARRAY) {
        fputs(" = ", g->out);
        emitExpr(g, n->a);
      }
      fputs(";\n", g->out);
      return;
    }
    if (n->capacity > 0) {
      fprintf(g->out, "char %s[%zu]", n->name, n->capacity + 1);
      if (n->a && n->a->kind == X_STRING) {
        fputs(" = ", g->out);
        emitExpr(g, n->a);
      } else if (n->a && global) {
        fputs(" = \"\"", g->out);
      }
      fputs(";\n", g->out);
      if (n->a && !global && n->a->kind != X_STRING) {
        indent(g);
        fprintf(g->out, "snprintf(%s, sizeof(%s), \"%%s\", ", n->name, n->name);
        emitExpr(g, n->a);
        fputs(");\n", g->out);
      }
      return;
    }
    if (n->arrayCount > 0) {
      fprintf(g->out, "char *%s[%zu]", n->name, n->arrayCount);
      if (n->a) {
        fputs(" = ", g->out);
        emitExpr(g, n->a);
      }
      fputs(";\n", g->out);
      return;
    }
    fputs("char *", g->out);
    fprintf(g->out, "%s", n->name);
    if (n->a) {
      fputs(" = ", g->out);
      emitExpr(g, n->a);
    } else if (global)
      fputs(" = NULL", g->out);
    fputs(";\n", g->out);
    return;
  }
  fputs(cType(n->type), g->out);
  fputc(' ', g->out);
  for (int i = 0; i < n->pointerDepth; ++i)
    fputc('*', g->out);
  fprintf(g->out, "%s", n->name);
  if (n->arrayCount)
    fprintf(g->out, "[%zu]", n->arrayCount);
  if (n->a) {
    fputs(" = ", g->out);
    emitTypedExpr(g, n->type, n->a);
  } else if (global && !n->pointerDepth && !n->arrayCount)
    fputs(" = 0", g->out);
  fputs(";\n", g->out);
}

static void emitDeclaration(Codegen *g, Node *n, int global) {
  if (!n)
    return;
  emitSingleDeclaration(g, n, global);
  for (size_t i = 0; i < n->count; ++i)
    emitDeclaration(g, n->items[i], global);
}

static void emitSingleField(Codegen *g, Node *n) {
  fputs("    ", g->out);
  if (!strcmp(n->type, "str") || !strncmp(n->type, "str", 3)) {
    if (n->capacity)
      fprintf(g->out, "char %s[%zu];\n", n->name, n->capacity + 1);
    else
      fprintf(g->out, "char *%s;\n", n->name);
  } else {
    fputs(cType(n->type), g->out);
    fputc(' ', g->out);
    for (int i = 0; i < n->pointerDepth; ++i)
      fputc('*', g->out);
    fprintf(g->out, "%s", n->name);
    if (n->arrayCount)
      fprintf(g->out, "[%zu]", n->arrayCount);
    fputs(";\n", g->out);
  }
}

static void emitField(Codegen *g, Node *n) {
  if (!n)
    return;
  emitSingleField(g, n);
  for (size_t i = 0; i < n->count; ++i)
    emitField(g, n->items[i]);
}

static void emitStatement(Codegen *g, Node *n);
static void emitStatementList(Codegen *g, Node **items, size_t count) {
  for (size_t i = 0; i < count; ++i)
    emitStatement(g, items[i]);
}

static void emitBlockContents(Codegen *g, Node *n) {
  if (!n)
    return;
  if (n->kind == N_BLOCK)
    emitStatementList(g, n->items, n->count);
  else
    emitStatement(g, n);
}

static void emitStatement(Codegen *g, Node *n) {
  if (!n)
    return;
  switch (n->kind) {
  case N_BLOCK:
    indent(g);
    fputs("{\n", g->out);
    g->indent++;
    emitStatementList(g, n->items, n->count);
    g->indent--;
    indent(g);
    fputs("}\n", g->out);
    break;
  case N_VAR:
    emitDeclaration(g, n, 0);
    break;
  case N_ASSIGN:
    indent(g);
    if (!strcmp(n->text, "=") &&
        isStringBufferLvalue(n->a, g->model, g->currentFunction)) {
      fputs("snprintf(", g->out);
      emitExpr(g, n->a);
      fputs(", sizeof(", g->out);
      emitExpr(g, n->a);
      fputs("), \"%s\", ", g->out);
      emitExpr(g, n->b);
      fputs(");\n", g->out);
    } else {
      emitExpr(g, n->a);
      fprintf(g->out, " %s ", n->text);
      emitTypedExpr(g, inferType(n->a, g->model, g->currentFunction), n->b);
      fputs(";\n", g->out);
    }
    break;
  case N_EXPR_STMT:
    indent(g);
    emitExpr(g, n->a);
    fputs(";\n", g->out);
    break;
  case N_IF:
    indent(g);
    fputs("if (", g->out);
    emitExpr(g, n->a);
    fputs(") ", g->out);
    if (n->b && n->b->kind == N_BLOCK) {
      fputs("{\n", g->out);
      g->indent++;
      emitBlockContents(g, n->b);
      g->indent--;
      indent(g);
      fputc('}', g->out);
    } else
      emitStatement(g, n->b);
    if (n->c) {
      if (n->c->kind == N_IF) {
        fputs(" else ",
              g->out); /* emit chained else-if without extra indentation */
        Node *old = n->c;
        fputs("if (", g->out);
        emitExpr(g, old->a);
        fputs(") {\n", g->out);
        g->indent++;
        emitBlockContents(g, old->b);
        g->indent--;
        indent(g);
        fputc('}', g->out);
        Node *tail = old->c;
        if (tail) {
          if (tail->kind == N_IF) {
            fputs(" else ", g->out);
            emitStatement(g, tail);
          } else {
            fputs(" else {\n", g->out);
            g->indent++;
            emitBlockContents(g, tail);
            g->indent--;
            indent(g);
            fputc('}', g->out);
          }
        }
      } else {
        fputs(" else {\n", g->out);
        g->indent++;
        emitBlockContents(g, n->c);
        g->indent--;
        indent(g);
        fputc('}', g->out);
      }
    }
    fputc('\n', g->out);
    break;
  case N_WHILE:
    indent(g);
    fputs("while (", g->out);
    emitExpr(g, n->a);
    fputs(") {\n", g->out);
    g->indent++;
    g->loopDepth++;
    emitStatementList(g, n->items, n->count);
    g->loopDepth--;
    g->indent--;
    indent(g);
    fputs("}\n", g->out);
    break;
  case N_FOR:
    indent(g);
    fputs("for (", g->out);
    if (n->a && n->a->kind == N_VAR) {
      Node *v = n->a;
      fputs(cType(v->type), g->out);
      fputc(' ', g->out);
      for (int i = 0; i < v->pointerDepth; ++i)
        fputc('*', g->out);
      fputs(v->name, g->out);
      if (v->a) {
        fputs(" = ", g->out);
        emitExpr(g, v->a);
      }
    } else if (n->a && n->a->kind == N_ASSIGN) {
      emitExpr(g, n->a->a);
      fprintf(g->out, " %s ", n->a->text);
      emitExpr(g, n->a->b);
    } else
      emitExpr(g, n->a);
    fputs("; ", g->out);
    emitExpr(g, n->b);
    fputs("; ", g->out);
    if (n->c && n->c->kind == N_ASSIGN) {
      emitExpr(g, n->c->a);
      fprintf(g->out, " %s ", n->c->text);
      emitExpr(g, n->c->b);
    } else
      emitExpr(g, n->c);
    fputs(") {\n", g->out);
    g->indent++;
    g->loopDepth++;
    emitStatementList(g, n->items, n->count);
    g->loopDepth--;
    g->indent--;
    indent(g);
    fputs("}\n", g->out);
    break;
  case N_SWITCH:
    indent(g);
    fputs("switch (", g->out);
    emitExpr(g, n->a);
    fputs(") {\n", g->out);
    g->indent++;
    g->switchDepth++;
    for (size_t i = 0; i < n->count; ++i) {
      Node *cs = n->items[i];
      indent(g);
      if (cs->a) {
        fputs("case ", g->out);
        emitExpr(g, cs->a);
        fputs(":\n", g->out);
      } else
        fputs("default:\n", g->out);
      g->indent++;
      emitStatementList(g, cs->items, cs->count);
      g->indent--;
    }
    g->switchDepth--;
    g->indent--;
    indent(g);
    fputs("}\n", g->out);
    break;
  case N_RETURN:
    indent(g);
    if (!n->a && g->functionName && !strcmp(g->functionName, "main"))
      fputs("return 0;\n", g->out);
    else if (!n->a && g->functionType && !strcmp(g->functionType, "void"))
      fputs("return;\n", g->out);
    else {
      fputs("return ", g->out);
      emitExpr(g, n->a);
      fputs(";\n", g->out);
    }
    break;
  case N_KILL:
    indent(g);
    if (g->functionName && !strcmp(g->functionName, "main"))
      fprintf(g->out, "return %d;\n",
              n->text &&
                      (!strcmp(n->text, "error") || !strcmp(n->text, "panic"))
                  ? 1
                  : 0);
    else
      fprintf(g->out, "exit(%d);\n",
              n->text &&
                      (!strcmp(n->text, "error") || !strcmp(n->text, "panic"))
                  ? 1
                  : 0);
    break;
  case N_BREAK:
    indent(g);
    fputs("break;\n", g->out);
    break;
  case N_CONTINUE:
    indent(g);
    fputs("continue;\n", g->out);
    break;
  default:
    break;
  }
}

static void emitFunctionPrototype(Codegen *g, Node *n) {
  if (n->isStatic || n->isInline)
    fputs(n->isInline ? "static inline " : "static ", g->out);
  else
    fputs("extern ", g->out);
  fputs(!strcmp(n->name, "main") ? "int" : cType(n->type), g->out);
  fputc(' ', g->out);
  for (int i = 0; i < n->pointerDepth; ++i)
    fputc('*', g->out);
  emitCFunctionName(g, n->name);
  fputc('(', g->out);
  if (!n->count)
    fputs("void", g->out);
  for (size_t i = 0; i < n->count; ++i) {
    Node *p = n->items[i];
    if (i)
      fputs(", ", g->out);
    if (!strcmp(p->type, "str") || !strncmp(p->type, "str", 3))
      fputs("char *", g->out);
    else {
      fputs(cType(p->type), g->out);
      fputc(' ', g->out);
      for (int d = 0; d < p->pointerDepth; ++d)
        fputc('*', g->out);
    }
    fputs(p->name, g->out);
  }
  fputs(");\n", g->out);
}

static void emitFunction(Codegen *g, Node *n) {
  if (!n->a)
    return; /* Native/external prototype only. */
  if (n->isStatic || n->isInline)
    fputs(n->isInline ? "static inline " : "static ", g->out);
  fputs(!strcmp(n->name, "main") ? "int" : cType(n->type), g->out);
  fputc(' ', g->out);
  for (int i = 0; i < n->pointerDepth; ++i)
    fputc('*', g->out);
  emitCFunctionName(g, n->name);
  fputc('(', g->out);
  if (!n->count)
    fputs("void", g->out);
  for (size_t i = 0; i < n->count; ++i) {
    Node *p = n->items[i];
    if (i)
      fputs(", ", g->out);
    if (!strcmp(p->type, "str") || !strncmp(p->type, "str", 3))
      fputs("char *", g->out);
    else {
      fputs(cType(p->type), g->out);
      fputc(' ', g->out);
      for (int d = 0; d < p->pointerDepth; ++d)
        fputc('*', g->out);
    }
    fputs(p->name, g->out);
  }
  fputs(") {\n", g->out);
  const char *oldName = g->functionName, *oldType = g->functionType;
  g->currentFunction = n;
  g->functionName = n->name;
  g->functionType = n->type;
  g->indent = 1;
  emitBlockContents(g, n->a);
  if (!strcmp(n->name, "main")) {
    indent(g);
    fputs("return 0;\n", g->out);
  } else if (!strcmp(n->type, "void")) {
    indent(g);
    fputs("return;\n", g->out);
  }
  g->indent = 0;
  fputs("}\n\n", g->out);
  g->currentFunction = NULL;
  g->functionName = oldName;
  g->functionType = oldType;
}

static int generateC(Node *program, Model *model, FILE *out) {
  Codegen g;
  memset(&g, 0, sizeof(g));
  g.out = out;
  g.model = model;
  fputs("/* Generated by the Underscore compiler. Do not edit. */\n", out);
  fputs("#include <stdio.h>\n#include <stdlib.h>\n#include "
        "<stdint.h>\n#include <stdbool.h>\n#include <stddef.h>\n#include "
        "<string.h>\n#include <math.h>\n",
        out);
  fputs(
      "extern void __io_OutCall(const char *);\nextern void __io_OutInt(long "
      "long);\nextern void __io_OutFloat(double);\nextern void "
      "__io_OutBool(int);\nextern void __io_OutPointer(const void *);\nextern "
      "char *__io_InCall(void);\nextern void __io_NlCall(void);\n",
      out);
  fputs("static double __uo_sqrt(double x){return x<0.0?-1.0:sqrt(x);}\n", out);
  fputs("static double __uo_clamp(double x,double lo,double hi){return "
        "lo>hi?x:(x<lo?lo:(x>hi?hi:x));}\n",
        out);
  fputs("static double __uo_sign(double x){return (x>0)-(x<0);}\nstatic double "
        "__uo_lerp(double a,double b,double t){return a+(b-a)*t;}\n\n",
        out);

  /* Type declarations precede prototypes so all units share one C translation
   * unit. */
  for (size_t i = 0; i < program->count; ++i) {
    Node *n = program->items[i];
    if (n->kind == N_DATA) {
      fprintf(out, "typedef struct %s {\n", n->name);
      for (size_t j = 0; j < n->count; ++j)
        emitField(&g, n->items[j]);
      fprintf(out, "} %s;\n\n", n->name);
    } else if (n->kind == N_ENUM) {
      if (n->name)
        fprintf(out, "typedef enum %s {\n", n->name);
      else
        fputs("enum {\n", out);
      for (size_t j = 0; j < n->count; ++j)
        fprintf(out, "    %s%s\n", n->items[j]->name,
                j + 1 < n->count ? "," : "");
      if (n->name)
        fprintf(out, "} %s;\n\n", n->name);
      else
        fputs("};\n\n", out);
    } else if (n->kind == N_ALIAS)
      fprintf(out, "typedef %s %s;\n\n", cType(n->type), n->name);
  }
  for (size_t i = 0; i < program->count; ++i)
    if (program->items[i]->kind == N_FUNCTION)
      emitFunctionPrototype(&g, program->items[i]);
  fputc('\n', out);
  for (size_t i = 0; i < program->count; ++i)
    if (program->items[i]->kind == N_VAR)
      emitDeclaration(&g, program->items[i], 1);
  fputc('\n', out);
  for (size_t i = 0; i < program->count; ++i) {
    Node *n = program->items[i];
    if (n->kind == N_FUNCTION)
      emitFunction(&g, n);
  }
  /* Non-function top-level statements run in an implicit main if no explicit
   * main exists. */
  int hasMain = 0;
  for (size_t i = 0; i < program->count; ++i)
    if (program->items[i]->kind == N_FUNCTION &&
        !strcmp(program->items[i]->name, "main") && program->items[i]->a)
      hasMain = 1;
  int hasTopCode = 0;
  for (size_t i = 0; i < program->count; ++i) {
    NodeKind k = program->items[i]->kind;
    if (k != N_FUNCTION && k != N_VAR && k != N_DATA && k != N_ENUM &&
        k != N_ALIAS)
      hasTopCode = 1;
  }
  if (!hasMain) {
    fputs("int main(void) {\n", out);
    if (hasTopCode) {
      g.functionName = "main";
      g.functionType = "int";
      g.indent = 1;
      for (size_t i = 0; i < program->count; ++i) {
        Node *n = program->items[i];
        if (n->kind != N_FUNCTION && n->kind != N_VAR && n->kind != N_DATA &&
            n->kind != N_ENUM && n->kind != N_ALIAS)
          emitStatement(&g, n);
      }
    }
    fputs("    return 0;\n}\n", out);
  }
  return !g.errors && !ferror(out);
}

/* ------------------------------- pipeline -------------------------------- */

static int quoteArg(char *dst, size_t cap, const char *src) {
  size_t n = 0;
  if (cap < 3)
    return 0;
  dst[n++] = '"';
  for (const char *p = src; *p; ++p) {
    if (*p == '"' || *p == '\\') {
      if (n + 2 >= cap)
        return 0;
      dst[n++] = '\\';
      dst[n++] = *p;
    } else {
      if (n + 1 >= cap)
        return 0;
      dst[n++] = *p;
    }
  }
  if (n + 2 > cap)
    return 0;
  dst[n++] = '"';
  dst[n] = '\0';
  return 1;
}

static int runCommand(const char *command) {
  int rc = system(command);
#ifdef _WIN32
  return rc == 0;
#else
  return rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 0;
#endif
}

static int fileExists(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return 0;
  fclose(f);
  return 1;
}

static char *dirnameOf(const char *path) {
  const char *a = strrchr(path, '/');
  const char *b = strrchr(path, '\\');
  const char *last = a;
  if (!last || (b && b > last))
    last = b;
  if (!last)
    return xstrdup(".");
  size_t n = (size_t)(last - path);
  if (n == 0)
    n = 1;
  char *d = (char *)malloc(n + 1);
  memcpy(d, path, n);
  d[n] = '\0';
  return d;
}

static char *resolveRuntime(const char *argv0) {
  static const char *workingCandidates[] = {
      "runtime.c", "tools/runtime.c", "underscore/compilers/c/v2/runtime.c",
      "tools/underscore/compilers/c/v2/runtime.c", NULL};

  for (size_t i = 0; workingCandidates[i]; ++i)
    if (fileExists(workingCandidates[i]))
      return xstrdup(workingCandidates[i]);

  char *dir = dirnameOf(argv0);
  static const char *executableCandidates[] = {
      "runtime.c",
      "../runtime.c",
      "../compilers/c/v2/runtime.c",
      "../underscore/compilers/c/v2/runtime.c",
      "compilers/c/v2/runtime.c",
      "underscore/compilers/c/v2/runtime.c",
      NULL};

  for (size_t i = 0; executableCandidates[i]; ++i) {
    char *candidate = formatString("%s/%s", dir, executableCandidates[i]);
    if (fileExists(candidate)) {
      free(dir);
      return candidate;
    }
    free(candidate);
  }

  free(dir);
  return NULL;
}

static char *temporaryPath(const char *name) {
#ifdef _WIN32
  const char *dir = getenv("TEMP");
  if (!dir || !*dir)
    dir = getenv("TMP");
  if (!dir || !*dir)
    dir = ".";
  char separator = '\\';
#else
  const char *dir = getenv("TMPDIR");
  if (!dir || !*dir)
    dir = "/tmp";
  char separator = '/';
#endif

  size_t length = strlen(dir);
  if (length && (dir[length - 1] == '/' || dir[length - 1] == '\\'))
    return formatString("%s%s", dir, name);
  return formatString("%s%c%s", dir, separator, name);
}

static char *defaultOutput(const char *input) {
  const char *base = strrchr(input, '/');
  const char *b2 = strrchr(input, '\\');
  if (!base || (b2 && b2 > base))
    base = b2;
  base = base ? base + 1 : input;
  char *name = xstrdup(base);
  char *dot = strrchr(name, '.');
  if (dot)
    *dot = '\0';
#ifdef _WIN32
  char *out = formatString("%s.exe", name);
#else
  char *out = formatString("%s", name);
#endif
  free(name);
  return out;
}

static void removeTemps(const char *a, const char *b, const char *c,
                        const char *d) {
  if (a)
    remove(a);
  if (b)
    remove(b);
  if (c)
    remove(c);
  if (d)
    remove(d);
}

static void printHelp(void) {
  puts("Underscore compiler");
  puts("Usage: underscore [flags] <source._> [more-source._ ...]");
  puts("\nFlags:");
  puts("  -o <file>   Write the executable to <file>");
  puts("  -Plex       Print lexer tokens");
  puts("  -Past       Print the abstract syntax tree");
  puts("  -Pfile      Print source files before compilation");
  puts("  -check      Parse and validate only (used by editor diagnostics)");
  puts("  -help       Display this help");
}

static void printAst(Node *n, int depth) {
  if (!n)
    return;
  for (int i = 0; i < depth; ++i)
    fputs("  ", stdout);
  static const char *names[] = {
      "PROGRAM",    "BLOCK",  "VARIABLE", "FUNCTION", "DATA",      "ENUM",
      "ALIAS",      "IF",     "DURING",   "REPEAT",   "SWITCH",    "CASE",
      "RETURN",     "KILL",   "BREAK",    "CONTINUE", "EXPR_STMT", "ASSIGN",
      "IDENTIFIER", "NUMBER", "STRING",   "BOOL",     "NULL",      "BINARY",
      "UNARY",      "CALL",   "MEMBER",   "INDEX",    "ARRAY"};
  printf("%s", names[n->kind]);
  if (n->name)
    printf(" %s", n->name);
  if (n->type)
    printf(" : %s", n->type);
  if (n->text)
    printf(" [%s]", n->text);
  putchar('\n');
  printAst(n->a, depth + 1);
  printAst(n->b, depth + 1);
  printAst(n->c, depth + 1);
  for (size_t i = 0; i < n->count; ++i)
    printAst(n->items[i], depth + 1);
}

int main(int argc, char **argv) {
  int printLex = 0, printAstFlag = 0, printFile = 0, checkOnly = 0;
  const char *outputPath = NULL;
  char *ownedOutputPath = NULL;
  char **inputs = NULL;
  size_t inputCount = 0, inputCap = 0;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "-Plex"))
      printLex = 1;
    else if (!strcmp(argv[i], "-Past"))
      printAstFlag = 1;
    else if (!strcmp(argv[i], "-Pfile"))
      printFile = 1;
    else if (!strcmp(argv[i], "-check"))
      checkOnly = 1;
    else if (!strcmp(argv[i], "-help") || !strcmp(argv[i], "--help") ||
             !strcmp(argv[i], "?")) {
      printHelp();
      free(inputs);
      return 0;
    } else if (!strcmp(argv[i], "-o")) {
      if (i + 1 >= argc) {
        fprintf(stderr, "error: -o requires an output filename\n");
        free(inputs);
        return 1;
      }
      outputPath = argv[++i];
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "error: unknown flag '%s'\n", argv[i]);
      free(inputs);
      return 1;
    } else {
      if (inputCount == inputCap) {
        inputCap = inputCap ? inputCap * 2 : 4;
        inputs = (char **)xrealloc(inputs, inputCap * sizeof(char *));
      }
      inputs[inputCount++] = argv[i];
    }
  }
  if (!inputCount) {
    printHelp();
    free(inputs);
    return 1;
  }
  if (!outputPath) {
    ownedOutputPath = defaultOutput(inputs[0]);
    outputPath = ownedOutputPath;
  }

  Node *program = nodeNew(N_PROGRAM, NULL);
  int failed = 0;
  for (size_t i = 0; i < inputCount; ++i) {
    char *source = NULL;
    size_t len = 0;
    if (!readFile(inputs[i], &source, &len)) {
      failed = 1;
      continue;
    }
    if (printFile) {
      printf("===== %s =====\n%s\n", inputs[i], source);
    }
    TokenArray tokens = lexSource(source, inputs[i], printLex);
    if (!tokens.data) {
      failed = 1;
      free(source);
      continue;
    }
    Parser p = {tokens.data, tokens.count, 0, inputs[i], 0};
    Node *unit = parseTopLevel(&p);
    if (!unit)
      failed = 1;
    else {
      for (size_t j = 0; j < unit->count; ++j)
        nodeAdd(program, unit->items[j]);
      free(unit->items);
      unit->items = NULL;
      unit->count = 0;
      freeNode(unit);
    }
    for (size_t j = 0; j < tokens.count; ++j)
      free(tokens.data[j].value);
    free(tokens.data);
    free(source);
  }
  free(inputs);
  if (failed) {
    freeNode(program);
    if (outputPath && outputPath != NULL) { /* no output has been created */
    }
    return 1;
  }
  if (printAstFlag)
    printAst(program, 0);

  Model model;
  memset(&model, 0, sizeof(model));
  collectTopLevel(&model, program);
  size_t mains = 0;
  for (size_t i = 0; i < program->count; ++i) {
    Node *n = program->items[i];
    if (n->kind == N_FUNCTION && !strcmp(n->name, "main") && n->a)
      mains++;
    validateNode(n, &model);
  }
  if (mains > 1) {
    fprintf(stderr,
            "error[E2005]: a program may define only one main function\n");
    model.errors++;
  }
  if (model.errors) {
    for (size_t i = 0; i < model.varCount; ++i) {
      free(model.vars[i].name);
      free(model.vars[i].type);
    }
    for (size_t i = 0; i < model.funcCount; ++i) {
      free(model.funcs[i].name);
      free(model.funcs[i].returnType);
      free(model.funcs[i].ownerType);
      free(model.funcs[i].methodName);
    }
    free(model.vars);
    free(model.funcs);
    freeNode(program);
    return 1;
  }
  unsigned long stamp = (unsigned long)time(NULL) ^ (unsigned long)getpid();
  char *tempCName = formatString(".__underscore_%lu.c", stamp);
  char *tempIRName = formatString(".__underscore_%lu.ll", stamp);
  char *runtimeIRName = formatString(".__underscore_runtime_%lu.ll", stamp);
  char *tempC = temporaryPath(tempCName);
  char *tempIR = temporaryPath(tempIRName);
  char *runtimeIR = temporaryPath(runtimeIRName);
  free(tempCName);
  free(tempIRName);
  free(runtimeIRName);
  char *qC = (char *)malloc(strlen(tempC) * 2 + 8),
       *qIR = (char *)malloc(strlen(tempIR) * 2 + 8),
       *qRIR = (char *)malloc(strlen(runtimeIR) * 2 + 8);
  char *qRuntime = NULL, *qOutput = (char *)malloc(strlen(outputPath) * 2 + 8);
  if (!quoteArg(qC, strlen(tempC) * 2 + 8, tempC) ||
      !quoteArg(qIR, strlen(tempIR) * 2 + 8, tempIR) ||
      !quoteArg(qRIR, strlen(runtimeIR) * 2 + 8, runtimeIR) ||
      !quoteArg(qOutput, strlen(outputPath) * 2 + 8, outputPath)) {
    fprintf(stderr, "error: temporary path is too long\n");
    failed = 1;
    goto cleanup;
  }
  char *runtimePath = NULL;
  if (!checkOnly) {
    runtimePath = resolveRuntime(argv[0]);
    if (!runtimePath) {
      fprintf(stderr,
              "error[E4001]: runtime.c was not found. Keep runtime.c beside "
              "the compiler executable or in the working directory.\n");
      failed = 1;
      goto cleanup;
    }
    qRuntime = (char *)malloc(strlen(runtimePath) * 2 + 8);
    if (!quoteArg(qRuntime, strlen(runtimePath) * 2 + 8, runtimePath)) {
      fprintf(stderr, "error: runtime path is too long\n");
      failed = 1;
      free(runtimePath);
      goto cleanup;
    }
  }
  const char *clang = getenv("UNDERSCORE_CLANG");
  if (!clang || !*clang)
    clang = "clang";
  FILE *cf = fopen(tempC, "wb");
  if (!cf) {
    fprintf(stderr, "error[E4002]: cannot create temporary C source: %s\n",
            strerror(errno));
    free(runtimePath);
    failed = 1;
    goto cleanup;
  }
  int genOK = generateC(program, &model, cf);
  fclose(cf);
  if (!genOK) {
    fprintf(stderr, "error[E4003]: code generation failed\n");
    free(runtimePath);
    failed = 1;
    goto cleanup;
  }
  char *cmd1 =
      formatString("%s -std=c11 -O1 -S -emit-llvm %s -o %s", clang, qC, qIR);
  if (!runCommand(cmd1)) {
    fprintf(stderr, "error[E4004]: Clang could not lower the generated program "
                    "to LLVM IR\n");
    failed = 1;
  }
  free(cmd1);
  if (!failed && checkOnly) {
    puts("Underscore: check passed");
  } else if (!failed) {
    char *cmd2 = formatString("%s -std=c11 -O1 -S -emit-llvm %s -o %s", clang,
                              qRuntime, qRIR);
    char *cmd3 =
        formatString("%s -O1 %s %s -lm -o %s", clang, qIR, qRIR, qOutput);
    if (!runCommand(cmd2)) {
      fprintf(stderr,
              "error[E4005]: Clang could not lower runtime.c to LLVM IR\n");
      failed = 1;
    } else if (!runCommand(cmd3)) {
      fprintf(stderr, "error[E4006]: LLVM linking failed; the executable was "
                      "not produced\n");
      failed = 1;
    }
    free(cmd2);
    free(cmd3);
    if (failed)
      remove(outputPath);
    else
      printf("Built %s from %zu source file%s\n", outputPath, inputCount,
             inputCount == 1 ? "" : "s");
  }
  free(runtimePath);
cleanup:
  removeTemps(tempC, tempIR, runtimeIR, NULL);
  free(tempC);
  free(tempIR);
  free(runtimeIR);
  free(qC);
  free(qIR);
  free(qRIR);
  free(qRuntime);
  free(qOutput);
  for (size_t i = 0; i < model.varCount; ++i) {
    free(model.vars[i].name);
    free(model.vars[i].type);
  }
  for (size_t i = 0; i < model.funcCount; ++i) {
    free(model.funcs[i].name);
    free(model.funcs[i].returnType);
    free(model.funcs[i].ownerType);
    free(model.funcs[i].methodName);
  }
  free(model.vars);
  free(model.funcs);
  freeNode(program);
  free(ownedOutputPath);
  return failed ? 1 : 0;
}
