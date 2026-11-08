// INCLUDES
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// MACROS
#define MAX_TOKEN_VALUE_LENGTH 1024
#define MAX_TOKENS 1024
#define MAX_SYMBOLS 1024
#define FLAG_COUNT (sizeof(compilerFlags) / sizeof(compilerFlags[0]))

// ====== READFILE ====== //
// ====== READFILE HELPER FUNCTIONS ====== //

void printChars(const char *buffer) {
  for (const char *ptr = buffer; *ptr != '\0'; ++ptr) {
    printf("%c\n", *ptr);
  }
}

// ====== MAIN READFILE FUNCTION ====== //

int readFile(const char *filename, char **buffer) {
  FILE *file = fopen(filename, "r");

  if (!file) {
    perror("Error opening file");
    return -1;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    perror("File seek failed");
    fclose(file);
    return -1;
  }

  long length = ftell(file);

  if (length < 0) {
    perror("File size failed");
    fclose(file);
    return -1;
  }

  rewind(file);

  *buffer = malloc((size_t)length + 1);

  if (!*buffer) {
    perror("Memory allocation failed");
    fclose(file);
    return -1;
  }

  size_t bytesRead = fread(*buffer, 1, (size_t)length, file);

  if (bytesRead != (size_t)length) {
    fprintf(stderr, "Error reading file\n");
    free(*buffer);
    *buffer = NULL;
    fclose(file);
    return -1;
  }

  (*buffer)[bytesRead] = '\0';

  fclose(file);
  return 0;
}

// ====== LEX ====== //

typedef enum {
  TOKEN_INTEGER,
  TOKEN_IDENTIFIER,
  TOKEN_ADD,
  TOKEN_SUBTRACT,
  TOKEN_MULTIPLY,
  TOKEN_DIVIDE,
  TOKEN_ASSIGN,
  TOKEN_STRING,
  TOKEN_FLOATING_POINT,
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE,
  TOKEN_RIGHT_BRACE,
  TOKEN_SQUARE_LEFT,
  TOKEN_SQUARE_RIGHT,
  TOKEN_EOF,
  TOKEN_VOID,
  TOKEN_IF,
  TOKEN_ELIF,
  TOKEN_ELSE,
  TOKEN_DURING,
  TOKEN_REPEAT,
  TOKEN_RETREAT,
  TOKEN_NUMBER,
  TOKEN_QUOTE
} isTokenType;

typedef struct {
  isTokenType type;
  char value[MAX_TOKEN_VALUE_LENGTH];
} Token;

// ====== LEX HELPER FUNCTIONS ====== //

int addToken(Token tokens[], size_t *count, isTokenType type,
             const char *value) {

  if (*count >= MAX_TOKENS) {
    fprintf(stderr, "Maximum token limit reached\n");
    return 0;
  }

  if (strlen(value) >= MAX_TOKEN_VALUE_LENGTH) {
    fprintf(stderr, "Token value is too long\n");
    return 0;
  }

  tokens[*count].type = type;
  strcpy(tokens[*count].value, value);

  (*count)++;

  return 1;
}

void printTokens(Token tokens[], size_t count) {
  for (size_t i = 0; i < count; i++) {
    printf("Token Type: %d, Value: %s\n", tokens[i].type, tokens[i].value);
  }
}

// ====== KEYWORDS ====== //

isTokenType keywordType(const char *word) {
  if (strcmp(word, "int") == 0)
    return TOKEN_INTEGER;
  if (strcmp(word, "fp") == 0)
    return TOKEN_FLOATING_POINT;
  if (strcmp(word, "str") == 0)
    return TOKEN_STRING;
  if (strcmp(word, "void") == 0)
    return TOKEN_VOID;

  if (strcmp(word, "if") == 0)
    return TOKEN_IF;
  if (strcmp(word, "elif") == 0)
    return TOKEN_ELIF;
  if (strcmp(word, "else") == 0)
    return TOKEN_ELSE;

  if (strcmp(word, "during") == 0)
    return TOKEN_DURING;
  if (strcmp(word, "repeat") == 0)
    return TOKEN_REPEAT;

  if (strcmp(word, "retreat") == 0)
    return TOKEN_RETREAT;

  return TOKEN_IDENTIFIER;
}

// ====== MAIN LEXER FUNCTION ====== //

Token *lex(const char *buffer, size_t *tokenCount) {

  static Token tokens[MAX_TOKENS];

  size_t i = 0;
  *tokenCount = 0;

  while (buffer[i] != '\0') {

    // Skip whitespace
    if (isspace((unsigned char)buffer[i])) {
      i++;
      continue;
    }

    // Identifiers and keywords
    if (isalpha((unsigned char)buffer[i]) || buffer[i] == '_') {

      char word[MAX_TOKEN_VALUE_LENGTH];
      size_t start = i;

      while (isalnum((unsigned char)buffer[i]) || buffer[i] == '_') {
        i++;
      }

      size_t length = i - start;

      if (length >= MAX_TOKEN_VALUE_LENGTH) {
        fprintf(stderr, "Identifier is too long\n");
        return NULL;
      }

      memcpy(word, &buffer[start], length);
      word[length] = '\0';

      isTokenType type = keywordType(word);

      if (!addToken(tokens, tokenCount, type, word)) {
        return NULL;
      }

      continue;
    }

    // Numbers
    if (isdigit((unsigned char)buffer[i])) {

      char number[MAX_TOKEN_VALUE_LENGTH];
      size_t start = i;

      while (isdigit((unsigned char)buffer[i])) {
        i++;
      }

      size_t length = i - start;

      if (length >= MAX_TOKEN_VALUE_LENGTH) {
        fprintf(stderr, "Number is too long\n");
        return NULL;
      }

      memcpy(number, &buffer[start], length);
      number[length] = '\0';

      if (!addToken(tokens, tokenCount, TOKEN_NUMBER, number)) {
        return NULL;
      }

      continue;
    }

    // Operators and brackets
    isTokenType type;
    char value[2] = {buffer[i], '\0'};

    switch (buffer[i]) {

    case '+':
      type = TOKEN_ADD;
      break;
    case '-':
      type = TOKEN_SUBTRACT;
      break;
    case '*':
      type = TOKEN_MULTIPLY;
      break;
    case '/':
      type = TOKEN_DIVIDE;
      break;
    case '=':
      type = TOKEN_ASSIGN;
      break;

    case '(':
      type = TOKEN_LEFT_PAREN;
      break;
    case ')':
      type = TOKEN_RIGHT_PAREN;
      break;

    case '{':
      type = TOKEN_LEFT_BRACE;
      break;
    case '}':
      type = TOKEN_RIGHT_BRACE;
      break;

    case '[':
      type = TOKEN_SQUARE_LEFT;
      break;
    case ']':
      type = TOKEN_SQUARE_RIGHT;
      break;

    case '"':
      type = TOKEN_QUOTE;
      break;

    default:
      fprintf(stderr, "Unexpected character '%c' at position %zu\n", buffer[i],
              i);
      return NULL;
    }

    if (!addToken(tokens, tokenCount, type, value)) {
      return NULL;
    }

    i++;
  }

  // End of file
  if (!addToken(tokens, tokenCount, TOKEN_EOF, "EOF")) {
    return NULL;
  }

  return tokens;
}

// ====== AST ====== //

typedef enum {
  AST_PROGRAM,
  AST_INT_DECLARATION,
  AST_FP_DECLARATION,

  AST_IDENTIFIER,
  AST_NUMBER,
  AST_BLOCK,
  AST_ASSIGNMENT,

  AST_BINARY_EXPRESSION,
} ASTNodeType;

typedef struct ASTNode {
  ASTNodeType type;
  char value[MAX_TOKEN_VALUE_LENGTH];

  struct ASTNode *left;
  struct ASTNode *right;
} ASTNode;

// ====== AST HELPERS ====== //

ASTNode *createASTNode(ASTNodeType type, const char *value) {

  ASTNode *node = malloc(sizeof(ASTNode));

  if (!node) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }

  strncpy(node->value, value, MAX_TOKEN_VALUE_LENGTH - 1);
  node->value[MAX_TOKEN_VALUE_LENGTH - 1] = '\0';

  node->type = type;
  node->left = NULL;
  node->right = NULL;

  return node;
}
void freeAST(ASTNode *node) {
  if (!node) {
    return;
  }
  freeAST(node->left);
  freeAST(node->right);
  free(node);
}
void printAST(ASTNode *node, int depth) {
  if (!node)
    return;

  for (int i = 0; i < depth; i++)
    printf("  ");

  const char *typeName;

  switch (node->type) {
  case AST_PROGRAM:
    typeName = "PROGRAM";
    break;
  case AST_INT_DECLARATION:
    typeName = "INT_DECLARATION";
    break;
  case AST_FP_DECLARATION:
    typeName = "FP_DECLARATION";
    break;
  case AST_IDENTIFIER:
    typeName = "IDENTIFIER";
    break;
  case AST_NUMBER:
    typeName = "NUMBER";
    break;
  case AST_BINARY_EXPRESSION:
    typeName = "BINARY_EXPRESSION";
    break;
  case AST_BLOCK:
    typeName = "BLOCK";
    break;
  case AST_ASSIGNMENT:
    typeName = "ASSIGNMENT";
    break;
  default:
    typeName = "UNKNOWN";
    break;
  }

  printf("%s", typeName);

  if (node->value[0] != '\0')
    printf(" (%s)", node->value);

  printf("\n");

  printAST(node->left, depth + 1);
  printAST(node->right, depth + 1);
}

// ====== AST TREES ====== //

ASTNode *intDeclaration(ASTNode *identifier, ASTNode *value) {
  ASTNode *node = malloc(sizeof(ASTNode));
  node->type = TOKEN_INTEGER;
  strncpy(node->value, "int_declaration", MAX_TOKEN_VALUE_LENGTH - 1);
  node->value[MAX_TOKEN_VALUE_LENGTH - 1] = '\0';
  return node;
}
ASTNode *fpDeclaration(ASTNode *identifier, ASTNode *value) {
  ASTNode *node = malloc(sizeof(ASTNode));
  node->type = TOKEN_FLOATING_POINT;
  strncpy(node->value, "fp_declaration", MAX_TOKEN_VALUE_LENGTH - 1);
  node->value[MAX_TOKEN_VALUE_LENGTH - 1] = '\0';
  return node;
}
ASTNode *strDeclaration(ASTNode *identifier, ASTNode *value) {
  ASTNode *node = malloc(sizeof(ASTNode));
  node->type = TOKEN_STRING;
  strncpy(node->value, "str_declaration", MAX_TOKEN_VALUE_LENGTH - 1);
  node->value[MAX_TOKEN_VALUE_LENGTH - 1] = '\0';
  return node;
}

// ====== PARSER ====== //

typedef struct {
  Token *tokens;
  size_t count;
  size_t position;
} Parser;

// ====== PARSER HELPERS ====== //

Token *peek(Parser *parser) {
  if (parser->position >= parser->count) {
    return NULL;
  }
  return &parser->tokens[parser->position];
}
Token *consume(Parser *parser, isTokenType expected) {

  Token *token = peek(parser);

  if (!token || token->type != expected) {
    fprintf(stderr, "Unexpected token: expected type %d\n", expected);
    return NULL;
  }

  parser->position++;

  return token;
}
Token *advance(Parser *parser) {
  Token *token = peek(parser);
  if (token && token->type != TOKEN_EOF) {
    parser->position++;
  }

  return token;
}
int match(Parser *parser, isTokenType expected) {
  Token *token = peek(parser);

  if (!token || token->type != expected) {
    return 0;
  }

  advance(parser);
  return 1;
}

// ====== PARSING FUNCTIONS ====== //

// DELCARATION PARSING
ASTNode *parseIntDeclaration(Parser *parser) {
  if (!consume(parser, TOKEN_INTEGER)) {
    return NULL;
  }
  Token *identifier = consume(parser, TOKEN_IDENTIFIER);
  if (!identifier) {
    return NULL;
  }
  if (!consume(parser, TOKEN_ASSIGN)) {
    return NULL;
  }
  Token *value = consume(parser, TOKEN_NUMBER);
  if (!value) {
    return NULL;
  }
  ASTNode *identifierNode = createASTNode(AST_IDENTIFIER, identifier->value);

  ASTNode *valueNode = createASTNode(AST_NUMBER, value->value);

  if (!identifierNode || !valueNode) {
    freeAST(identifierNode);
    freeAST(valueNode);
    return NULL;
  }

  return intDeclaration(identifierNode, valueNode);
}
ASTNode *parseFPDeclaration(Parser *parser) {
  if (!consume(parser, TOKEN_FLOATING_POINT)) {
    return NULL;
  }
  Token *identifier = consume(parser, TOKEN_IDENTIFIER);
  if (!identifier) {
    return NULL;
  }
  if (!consume(parser, TOKEN_ASSIGN)) {
    return NULL;
  }
  Token *value = consume(parser, TOKEN_NUMBER);
  if (!value) {
    return NULL;
  }
  ASTNode *identifierNode = createASTNode(AST_IDENTIFIER, identifier->value);

  ASTNode *valueNode = createASTNode(AST_NUMBER, value->value);

  if (!identifierNode || !valueNode) {
    freeAST(identifierNode);
    freeAST(valueNode);
    return NULL;
  }

  return fpDeclaration(identifierNode, valueNode);
}
ASTNode *parseStrDeclaration(Parser *parser) {
  if (!consume(parser, TOKEN_STRING)) {
    return NULL;
  }
  Token *identifier = consume(parser, TOKEN_IDENTIFIER);
  if (!identifier) {
    return NULL;
  }
  if (!consume(parser, TOKEN_ASSIGN)) {
    return NULL;
  }

  if (!consume(parser, TOKEN_QUOTE)) {
    return NULL;
  }
  Token *value = consume(parser, TOKEN_IDENTIFIER);
  if (!value) {
    return NULL;
  }
  if (!consume(parser, TOKEN_QUOTE)) {
    return NULL;
  }

  ASTNode *identifierNode = createASTNode(AST_IDENTIFIER, identifier->value);
  ASTNode *valueNode = createASTNode(AST_IDENTIFIER, value->value);
  if (!identifierNode || !valueNode) {
    freeAST(identifierNode);
    freeAST(valueNode);
    return NULL;
  }

  return strDeclaration(identifierNode, valueNode);
}

// EXPRESSION PARSING
ASTNode *parsePrimary(Parser *parser);
ASTNode *parseTerm(Parser *parser);
ASTNode *parseExpression(Parser *parser);

ASTNode *parsePrimary(Parser *parser) {
  Token *token = peek(parser);

  if (!token)
    return NULL;

  // Number
  if (token->type == TOKEN_NUMBER) {
    advance(parser);
    return createASTNode(AST_NUMBER, token->value);
  }

  // Identifier
  if (token->type == TOKEN_IDENTIFIER) {
    advance(parser);
    return createASTNode(AST_IDENTIFIER, token->value);
  }

  // Parenthesized expression
  if (match(parser, TOKEN_LEFT_PAREN)) {
    ASTNode *expression = parseExpression(parser);

    if (!expression)
      return NULL;

    if (!consume(parser, TOKEN_RIGHT_PAREN)) {
      freeAST(expression);
      return NULL;
    }

    return expression;
  }

  fprintf(stderr, "Unexpected token in expression: %s\n", token->value);
  return NULL;
}
ASTNode *parseTerm(Parser *parser) {
  ASTNode *left = parsePrimary(parser);

  if (!left)
    return NULL;

  while (peek(parser)->type == TOKEN_MULTIPLY ||
         peek(parser)->type == TOKEN_DIVIDE) {

    Token *operator = advance(parser);

    ASTNode *right = parsePrimary(parser);

    if (!right) {
      freeAST(left);
      return NULL;
    }

    ASTNode *node = createASTNode(AST_BINARY_EXPRESSION, operator->value);

    if (!node) {
      freeAST(left);
      freeAST(right);
      return NULL;
    }

    node->left = left;
    node->right = right;

    left = node;
  }

  return left;
}
ASTNode *parseExpression(Parser *parser) {
  ASTNode *left = parseTerm(parser);

  if (!left)
    return NULL;

  while (peek(parser)->type == TOKEN_ADD ||
         peek(parser)->type == TOKEN_SUBTRACT) {

    Token *operator = advance(parser);

    ASTNode *right = parseTerm(parser);

    if (!right) {
      freeAST(left);
      return NULL;
    }

    ASTNode *node = createASTNode(AST_BINARY_EXPRESSION, operator->value);

    if (!node) {
      freeAST(left);
      freeAST(right);
      return NULL;
    }

    node->left = left;
    node->right = right;

    left = node;
  }

  return left;
}

// STATEMENT PARSING
ASTNode *parseStatement(Parser *parser);
ASTNode *parseBlock(Parser *parser);
ASTNode *parseProgram(Parser *parser);
ASTNode *parseAssignment(Parser *parser);

ASTNode *parseStatement(Parser *parser) {
  Token *token = peek(parser);
  if (!token || token->type == TOKEN_EOF) {
    return NULL;
  }

  switch (token->type) {
  case TOKEN_INTEGER:
    return parseIntDeclaration(parser);
  case TOKEN_FLOATING_POINT:
    return parseFPDeclaration(parser);
  case TOKEN_STRING:
    return parseStrDeclaration(parser);
  case TOKEN_IDENTIFIER:
    return parseAssignment(parser);
  default:
    fprintf(stderr, "Unexpected token in statement: %s\n", token->value);
    return NULL;
  }
}
ASTNode *parseBlock(Parser *parser) {
  if (!consume(parser, TOKEN_LEFT_BRACE)) {
    return NULL;
  }

  ASTNode *block = createASTNode(AST_BLOCK, "block");
  if (!block) {
    return NULL;
  }

  ASTNode *current = block;

  while (peek(parser) && peek(parser)->type != TOKEN_RIGHT_BRACE) {
    if (peek(parser)->type == TOKEN_EOF) {
      fprintf(stderr, "Expected closing brace\n");
      freeAST(block);
      return NULL;
    }

    ASTNode *statement = parseStatement(parser);

    if (!statement) {
      freeAST(block);
      return NULL;
    }

    if (!current->left) {
      current->left = statement;
    } else {
      ASTNode *next = createASTNode(AST_BLOCK, "block");

      if (!next) {
        freeAST(statement);
        freeAST(block);
        return NULL;
      }

      next->left = statement;
      current->right = next;
      current = next;
    }
  }

  // Expect closing brace
  if (!consume(parser, TOKEN_RIGHT_BRACE)) {
    freeAST(block);
    return NULL;
  }

  return block;
}
ASTNode *parseProgram(Parser *parser) {
  ASTNode *program = createASTNode(AST_PROGRAM, "program");
  if (!program) {
    return NULL;
  }

  ASTNode *current = program;

  while (peek(parser) && peek(parser)->type != TOKEN_EOF) {
    ASTNode *statement = parseStatement(parser);

    if (!statement) {
      freeAST(program);
      return NULL;
    }

    if (!current->left) {
      current->left = statement;
    } else {
      ASTNode *next = createASTNode(AST_BLOCK, "block");

      if (!next) {
        freeAST(statement);
        freeAST(program);
        return NULL;
      }

      next->left = statement;
      current->right = next;
      current = next;
    }
  }

  return program;
}
ASTNode *parseAssignment(Parser *parser) {
  Token *identifier = consume(parser, TOKEN_IDENTIFIER);

  if (!identifier)
    return NULL;

  if (!consume(parser, TOKEN_ASSIGN))
    return NULL;

  ASTNode *value = parseExpression(parser);
  if (!value)
    return NULL;

  ASTNode *identifierNode = createASTNode(AST_IDENTIFIER, identifier->value);

  if (!identifierNode) {
    freeAST(value);
    return NULL;
  }

  ASTNode *assignment = createASTNode(AST_ASSIGNMENT, "=");

  if (!assignment) {
    freeAST(identifierNode);
    freeAST(value);
    return NULL;
  }

  assignment->left = identifierNode;
  assignment->right = value;

  return assignment;
}

// ====== SEMANTIC ====== //

typedef enum {
  SYMBOL_INT,
  SYMBOL_FP,
  SYMBOL_INVALID,
} SymbolType;

typedef struct {
  char name[MAX_TOKEN_VALUE_LENGTH];
  SymbolType type;
} Symbol;

typedef struct {
  Symbol symbols[MAX_SYMBOLS];
  size_t count;
} SymbolTable;

// SYMBOL
Symbol *findSymbol(SymbolTable *table, const char *name);
int addSymbol(SymbolTable *table, const char *name, SymbolType type);
void freeSymbols(SymbolTable *table);

Symbol *findSymbol(SymbolTable *table, const char *name) {
  for (size_t i = 0; i < table->count; i++) {
    if (strcmp(table->symbols[i].name, name) == 0) {
      return &table->symbols[i];
    }
  }

  return NULL;
}
int addSymbol(SymbolTable *table, const char *name, SymbolType type) {
  if (findSymbol(table, name)) {
    fprintf(stderr, "Semantic error: variable '%s' already declared\n", name);
    return 0;
  }

  if (table->count >= MAX_SYMBOLS) {
    fprintf(stderr, "Semantic error: symbol table is full\n");
    return 0;
  }

  Symbol *symbol = &table->symbols[table->count];

  strncpy(symbol->name, name, MAX_TOKEN_VALUE_LENGTH - 1);
  symbol->name[MAX_TOKEN_VALUE_LENGTH - 1] = '\0';

  symbol->type = type;

  table->count++;

  return 1;
}
void freeSymbols(SymbolTable *table) { table->count = 0; }

// ANALYSIS
SymbolType getExpressionType(ASTNode *node, SymbolTable *table);
int analyzeDeclaration(ASTNode *node, SymbolTable *table);
int analyzeAssignment(ASTNode *node, SymbolTable *table);
int analyzeStatement(ASTNode *node, SymbolTable *table);
int analyzeProgram(ASTNode *program, SymbolTable *table);

SymbolType getExpressionType(ASTNode *node, SymbolTable *table) {

  if (!node)
    return SYMBOL_INVALID;

  if (node->type == AST_NUMBER) {
    if (strchr(node->value, '.') != NULL)
      return SYMBOL_FP;

    return SYMBOL_INT;
  }

  if (node->type == AST_IDENTIFIER) {
    Symbol *symbol = findSymbol(table, node->value);

    if (!symbol) {
      fprintf(stderr, "Semantic error: undefined variable '%s'\n", node->value);
      return SYMBOL_INVALID;
    }

    return symbol->type;
  }

  if (node->type == AST_BINARY_EXPRESSION) {

    SymbolType left = getExpressionType(node->left, table);
    SymbolType right = getExpressionType(node->right, table);

    if (left == SYMBOL_INVALID || right == SYMBOL_INVALID)
      return SYMBOL_INVALID;

    if (left != right) {
      fprintf(stderr, "Semantic error: incompatible expression types\n");
      return SYMBOL_INVALID;
    }

    return left;
  }

  return SYMBOL_INVALID;
}
int analyzeDeclaration(ASTNode *node, SymbolTable *table) {
  if (!node || !node->left || !node->right)
    return 0;

  SymbolType declaredType;

  if (node->type == AST_INT_DECLARATION) {
    declaredType = SYMBOL_INT;
  } else if (node->type == AST_FP_DECLARATION) {
    declaredType = SYMBOL_FP;
  } else {
    return 0;
  }

  const char *name = node->left->value;

  SymbolType valueType = getExpressionType(node->right, table);

  if (valueType == SYMBOL_INVALID)
    return 0;

  if (valueType != declaredType) {
    fprintf(stderr, "Semantic error: incompatible type for '%s'\n", name);
    return 0;
  }

  return addSymbol(table, name, declaredType);
}
int analyzeAssignment(ASTNode *node, SymbolTable *table) {
  if (!node || !node->left || !node->right)
    return 0;

  Symbol *symbol = findSymbol(table, node->left->value);

  if (!symbol) {
    fprintf(stderr, "Semantic error: undefined variable '%s'\n",
            node->left->value);
    return 0;
  }

  SymbolType valueType = getExpressionType(node->right, table);

  if (valueType == SYMBOL_INVALID)
    return 0;

  if (valueType != symbol->type) {
    fprintf(stderr, "Semantic error: incompatible assignment to '%s'\n",
            symbol->name);
    return 0;
  }

  return 1;
}
int analyzeStatement(ASTNode *node, SymbolTable *table) {
  if (!node)
    return 0;

  switch (node->type) {

  case AST_INT_DECLARATION:
  case AST_FP_DECLARATION:
    return analyzeDeclaration(node, table);

  case AST_ASSIGNMENT:
    return analyzeAssignment(node, table);

  case AST_BLOCK: {
    ASTNode *current = node;

    while (current) {
      if (current->left) {
        if (!analyzeStatement(current->left, table))
          return 0;
      }

      current = current->right;
    }

    return 1;
  }

  default:
    fprintf(stderr, "Semantic error: unsupported statement\n");
    return 0;
  }
}
int analyzeProgram(ASTNode *program, SymbolTable *table) {
  if (!program || !table)
    return 0;

  ASTNode *current = program;

  while (current) {

    if (current->left) {
      if (!analyzeStatement(current->left, table))
        return 0;
    }

    current = current->right;
  }

  return 1;
}

// ====== CODEGEN ====== //

typedef struct {
  FILE *output;
  int tempCount;
} CodegenContext;

// DECLARATION
int generateExpression(ASTNode *node, CodegenContext *context, char *result,
                       size_t resultSize);
int generateIntDeclaration(ASTNode *node, CodegenContext *context);
int generateAssignment(ASTNode *node, CodegenContext *context);
int generateStatement(ASTNode *node, CodegenContext *context);
int generateProgram(ASTNode *program, FILE *output);

int generateExpression(ASTNode *node, CodegenContext *context, char *result,
                       size_t resultSize) {
  if (!node)
    return 0;

  if (node->type == AST_NUMBER) {
    snprintf(result, resultSize, "%s", node->value);
    return 1;
  }

  if (node->type == AST_IDENTIFIER) {
    int temp = context->tempCount++;

    fprintf(context->output, "    %%t%d = load i32, ptr %%%s\n", temp,
            node->value);

    snprintf(result, resultSize, "%%t%d", temp);
    return 1;
  }

  if (node->type == AST_BINARY_EXPRESSION) {
    char left[64];
    char right[64];

    if (!generateExpression(node->left, context, left, sizeof(left)))
      return 0;

    if (!generateExpression(node->right, context, right, sizeof(right)))
      return 0;

    const char *instruction;

    if (strcmp(node->value, "+") == 0)
      instruction = "add";
    else if (strcmp(node->value, "-") == 0)
      instruction = "sub";
    else if (strcmp(node->value, "*") == 0)
      instruction = "mul";
    else if (strcmp(node->value, "/") == 0)
      instruction = "sdiv";
    else
      return 0;

    int temp = context->tempCount++;

    fprintf(context->output, "    %%t%d = %s i32 %s, %s\n", temp, instruction,
            left, right);

    snprintf(result, resultSize, "%%t%d", temp);
    return 1;
  }

  return 0;
}
int generateIntDeclaration(ASTNode *node, CodegenContext *context) {
  if (!node || !node->left || !node->right)
    return 0;

  const char *name = node->left->value;
  char value[64];

  if (!generateExpression(node->right, context, value, sizeof(value)))
    return 0;

  fprintf(context->output, "    %%%s = alloca i32\n", name);
  fprintf(context->output, "    store i32 %s, ptr %%%s\n", value, name);

  return 1;
}
int generateAssignment(ASTNode *node, CodegenContext *context) {

  if (!node || !node->left || !node->right)
    return 0;

  const char *name = node->left->value;
  char value[64];

  if (!generateExpression(node->right, context, value, sizeof(value)))
    return 0;

  fprintf(context->output, "    store i32 %s, ptr %%%s\n", value, name);

  return 1;
}
int generateStatement(ASTNode *node, CodegenContext *context) {
  if (!node)
    return 0;

  switch (node->type) {

  case AST_INT_DECLARATION:
    return generateIntDeclaration(node, context);

  case AST_ASSIGNMENT:
    return generateAssignment(node, context);

  case AST_BLOCK: {
    ASTNode *current = node;

    while (current) {
      if (current->left) {
        if (!generateStatement(current->left, context))
          return 0;
      }

      current = current->right;
    }

    return 1;
  }

  default:
    fprintf(stderr, "Codegen error: unsupported statement\n");
    return 0;
  }
}
int generateProgram(ASTNode *program, FILE *output) {
  if (!program || !output)
    return 0;

  CodegenContext context = {.output = output, .tempCount = 0};
  fprintf(output, "define i32 @main() {\n");
  fprintf(output, "entry:\n");

  ASTNode *current = program;
  while (current) {
    if (current->left) {
      if (!generateStatement(current->left, &context)) {
        fprintf(stderr, "Codegen error: failed to generate program\n");
        return 0;
      }
    }

    current = current->right;
  }

  fprintf(output, "    ret i32 0\n");
  fprintf(output, "}\n");

  return 1;
}

// ====== MAIN ====== //

// FLAGS
typedef struct {
  const char *name;
  const char *description;
} CompilerFlag;

const CompilerFlag compilerFlags[] = {
    {"-Plex", "Print lexer tokens"},
    {"-Past", "Print the abstract syntax tree"},
    {"-Pfile", "Print the source file"},
    {"-help", "Display compiler help"},
    {"?", "Display compiler help"}};

void printHelp(void);
int parseFlags(int argc, char *argv[], int *printLex, int *printAST,
                      int *printFile);

void printHelp(void) {
  printf("Underscore Compiler\n");
  printf("Usage: compiler [flags] <source.u>\n\n");

  printf("Available flags:\n");

  for (size_t i = 0; i < FLAG_COUNT; i++) {
    printf("  %-8s %s\n", compilerFlags[i].name, compilerFlags[i].description);
  }
}
int parseFlags(int argc, char *argv[], int *printLex, int *printAST,
               int *printFile) {
  for (int i = 1; i < argc; i++) {

    if (strcmp(argv[i], "-Plex") == 0) {
      *printLex = 1;
    } else if (strcmp(argv[i], "-Past") == 0) {
      *printAST = 1;
    } else if (strcmp(argv[i], "-Pfile") == 0) {
      *printFile = 1;
    } else if (strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "?") == 0) {
      printHelp();
      return 2;
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "Unknown flag: %s\n", argv[i]);
      return 0;
    }
  }

  return 1;
}

// MAIN
int main(int argc, char *argv[]) {

  int printLex = 0;
  int printASTFlag = 0;
  int printFile = 0;

  // Parse compiler flags
  int flagResult = parseFlags(argc, argv, &printLex, &printASTFlag, &printFile);

  if (flagResult == 2)
    return 0;

  if (flagResult == 0)
    return 1;

  // Find source filename
  char *fileName = NULL;

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      if (fileName != NULL) {
        fprintf(stderr, "Only one source file is allowed\n");
        return 1;
      }

      fileName = argv[i];
    }
  }

  if (!fileName) {
    fprintf(stderr, "Usage: compiler [flags] <source.u>\n");
    return 1;
  }

  // Read source file
  char *buffer = NULL;

  if (readFile(fileName, &buffer) != 0)
    return 1;

  if (printFile) {
    printChars(buffer);
    printf("\n");
  }

  // Lexing
  size_t tokenCount = 0;
  Token *tokens = lex(buffer, &tokenCount);

  if (!tokens) {
    fprintf(stderr, "Lexing failed\n");
    free(buffer);
    return 1;
  }

  if (printLex)
    printTokens(tokens, tokenCount);

  // Parsing
  Parser parser = {.tokens = tokens, .count = tokenCount, .position = 0};

  ASTNode *program = parseProgram(&parser);

  if (!program) {
    fprintf(stderr, "Parsing failed\n");
    free(tokens);
    free(buffer);
    return 1;
  }

  if (printASTFlag)
    printAST(program, 0);

  // Semantic analysis
  SymbolTable symbols = {0};

  if (!analyzeProgram(program, &symbols)) {
    fprintf(stderr, "Semantic analysis failed\n");
    freeAST(program);
    free(tokens);
    free(buffer);
    return 1;
  }

  // Code generation
  FILE *output = fopen("output.ll", "w");

  if (!output) {
    perror("Could not create output.ll");
    freeAST(program);
    free(tokens);
    free(buffer);
    return 1;
  }

  int success = generateProgram(program, output);

  fclose(output);

  // Cleanup
  freeAST(program);
  free(tokens);
  free(buffer);

  if (!success) {
    remove("output.ll");
    return 1;
  }

  return 0;
}