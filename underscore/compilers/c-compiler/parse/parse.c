#include "parse.h"

ASTNode astNodes[MAX_AST_NODES];
int astCount = 0;
int currentToken = 0;

static int parserFailed = 0;
static Token emptyToken = {TOKEN_EOF, "", 1, 1};

static Token *peekToken(int offset) {
  int index = currentToken + offset;
  if (tokenCount <= 0) {
    return &emptyToken;
  }
  if (index < 0) {
    index = 0;
  }
  if (index >= tokenCount) {
    index = tokenCount - 1;
  }
  return &tokens[index];
}

Token *current(void) {
  return peekToken(0);
}

ASTNode *newAST(ASTType type) {
  if (astCount >= MAX_AST_NODES) {
    fprintf(stderr, "parser error: AST exceeds MAX_AST_NODES (%d)\n",
            MAX_AST_NODES);
    exit(1);
  }

  ASTNode *node = &astNodes[astCount++];
  memset(node, 0, sizeof(*node));
  node->type = type;
  if (current() != NULL) {
    node->line = current()->line;
    node->column = current()->column;
  }
  node->declaredType.kind = TYPE_VOID;
  node->declaredType.width = 0;
  node->valueType = VALUE_VOID;
  return node;
}

static const char *tokenName(isTokenType type) {
  switch (type) {
  case TOKEN_EOF: return "end of file";
  case TOKEN_NEWLINE: return "newline";
  case TOKEN_VARIABLE: return "identifier";
  case TOKEN_NUMBER: return "number";
  case TOKEN_STRING: return "string literal";
  case TOKEN_INT: return "int";
  case TOKEN_FP: return "fp";
  case TOKEN_STR: return "str";
  case TOKEN_VOID: return "void";
  case TOKEN_EQUAL: return "=";
  case TOKEN_EQUAL_EQUAL: return "==";
  case TOKEN_NOT_EQUAL: return "!=";
  case TOKEN_LEFT_PAREN: return "(";
  case TOKEN_RIGHT_PAREN: return ")";
  case TOKEN_LEFT_SQUARE_PAREN: return "[";
  case TOKEN_RIGHT_SQUARE_PAREN: return "]";
  case TOKEN_LEFT_CURLY_PAREN: return "{";
  case TOKEN_RIGHT_CURLY_PAREN: return "}";
  case TOKEN_LEFT_VEC_PAREN: return "<";
  case TOKEN_RIGHT_VEC_PAREN: return ">";
  case TOKEN_COMMA: return ",";
  case TOKEN_COLON: return ":";
  case TOKEN_FUNC: return "func";
  case TOKEN_RETREAT: return "retreat";
  case TOKEN_IF: return "if";
  case TOKEN_ELIF: return "elif";
  case TOKEN_ELSE: return "else";
  case TOKEN_DURING: return "during";
  case TOKEN_REPEAT: return "repeat";
  case TOKEN_ARG: return "arg";
  case TOKEN_INCASE: return "incase";
  default:
    return "token";
  }
}

static const char *tokenDisplay(Token *token) {
  if (token == NULL) return "end of input";
  if (token->type == TOKEN_EOF) return "end of file";
  if (token->type == TOKEN_NEWLINE) return "newline";
  if (token->text[0] != '\0') return token->text;
  return tokenName(token->type);
}

_Noreturn void parserError(const char *message) {
  Token *token = current();
  int line = token != NULL ? token->line : 0;
  int column = token != NULL ? token->column : 1;
  char fullMessage[384];
  const char *help = NULL;

  if (message != NULL && strstr(message, "unterminated block") != NULL) {
    help = "add a closing `}` for the block, or check whether an earlier `{` is missing.";
  } else if (message != NULL && strstr(message, "unterminated switch") != NULL) {
    help = "add a closing `}` after the final switch case.";
  } else if (message != NULL && strstr(message, "user-defined types are not supported") != NULL) {
    help = "declared `data` and `alias` names cannot yet be used for variables or parameters; use `int`, `fp`, or `str` until user-defined type checking is implemented.";
  } else if (message != NULL && strstr(message, "expected expression") != NULL) {
    help = "an expression can be a literal, variable, function call, or operation, such as `total + 1`.";
  } else if (message != NULL && strstr(message, "end of line") != NULL) {
    help = "put each statement on its own line, or close the current block with `}`.";
  } else if (message != NULL && strstr(message, "unknown statement") != NULL) {
    help = "check the spelling of the keyword and make sure this construct is valid in the current context.";
  }

  if (message != NULL && strstr(message, "found `") != NULL) {
    snprintf(fullMessage, sizeof(fullMessage), "%s", message);
  } else {
    snprintf(fullMessage, sizeof(fullMessage), "%s (found `%s`)",
             message != NULL ? message : "invalid syntax", tokenDisplay(token));
  }
  if (message != NULL && strstr(message, "expected `)`, found `newline`") != NULL) {
    help = "add a closing `)` before the end of the line; for example, `out(total + 1)`.";
  } else if (message != NULL && strstr(message, "expected `]`") != NULL) {
    help = "close the index or string-size bracket with `]`.";
  } else if (message != NULL && strstr(message, "expected `}`") != NULL) {
    help = "add a closing `}` to finish the current block.";
  }
  diagnosticError("error", "E2000", fullMessage, line, column,
                  "the parser could not continue from this token", help);
}

int match(isTokenType type) {
  if (current() != NULL && current()->type == type) {
    if (type != TOKEN_EOF) {
      currentToken++;
    }
    return 1;
  }
  return 0;
}

Token *expect(isTokenType type) {
  Token *token = current();
  if (token == NULL || token->type != type) {
    char message[256];
    snprintf(message, sizeof(message), "expected `%s`, found `%s`",
             tokenName(type), tokenDisplay(token));
    parserError(message);
  }
  currentToken++;
  return token;
}

static int isIdentifierToken(isTokenType type) {
  return type == TOKEN_VARIABLE || type == TOKEN_MAIN;
}

static Token *expectIdentifier(void) {
  if (!isIdentifierToken(current()->type)) {
    parserError("expected identifier");
  }
  return &tokens[currentToken++];
}

static void copyText(char destination[MAX_NAME], const char *source) {
  if (source == NULL) {
    destination[0] = '\0';
    return;
  }
  snprintf(destination, MAX_NAME, "%s", source);
}

static void copyOperator(char destination[4], const char *source) {
  if (source == NULL) {
    destination[0] = '\0';
    return;
  }
  snprintf(destination, 4, "%s", source);
}

static int stringContentLength(const char *source) {
  int length = 0;
  if (source == NULL) return 0;
  for (size_t i = 0; source[i] != '\0'; ++i) {
    if (source[i] == '\\' && source[i + 1] != '\0') ++i;
    ++length;
  }
  return length;
}

static int parseCapacity(Token *token, const char *kind) {
  char *end = NULL;
  errno = 0;
  unsigned long long value = strtoull(token->text, &end, 10);
  if (errno == ERANGE || end == token->text || *end != '\0' || value > INT_MAX) {
    char message[256];
    snprintf(message, sizeof(message), "%s capacity is too large for this compiler", kind);
    diagnosticError("error", "E2002", message, token->line, token->column,
                    "this size cannot be represented safely",
                    "use a smaller positive size (at most INT_MAX), or split the data into smaller arrays.");
  }
  if (value == 0) {
    char message[256];
    snprintf(message, sizeof(message), "%s capacity must be greater than zero", kind);
    diagnosticError("error", "E2003", message, token->line, token->column,
                    "zero-sized arrays and string buffers are not supported",
                    "use a positive capacity, such as `<3>` for an array or `[16]` for a string buffer.");
  }
  return (int)value;
}

static char *copyString(const char *source) {
  size_t length = strlen(source);
  char *copy = (char *)malloc(length + 1);
  if (copy == NULL) {
    fprintf(stderr, "parser error: out of memory\n");
    exit(1);
  }
  memcpy(copy, source, length + 1);
  return copy;
}

static void appendText(char destination[MAX_NAME], const char *source) {
  size_t used = strlen(destination);
  if (used >= MAX_NAME - 1) {
    parserError("name or flag list exceeds maximum length");
  }
  snprintf(destination + used, MAX_NAME - used, "%s", source);
}

static void appendNode(ASTNode **first, ASTNode **last, ASTNode *node) {
  if (node == NULL) {
    return;
  }
  node->next = NULL;
  if (*first == NULL) {
    *first = node;
  } else {
    (*last)->next = node;
  }
  *last = node;
}

static void skipNewlines(void) {
  while (match(TOKEN_NEWLINE)) {
    /* Blank lines are insignificant between statements. */
  }
}

static int isTypeToken(isTokenType type) {
  return type == TOKEN_INT || type == TOKEN_FP || type == TOKEN_STR ||
         type == TOKEN_VOID || type == TOKEN_FIXED_TYPE;
}

static Type parseTypeSpec(void) {
  Token *token = current();
  Type type;
  type.width = 0;

  switch (token->type) {
  case TOKEN_INT:
    type.kind = TYPE_INT;
    break;
  case TOKEN_FP:
    type.kind = TYPE_FP;
    break;
  case TOKEN_STR:
    type.kind = TYPE_STR;
    break;
  case TOKEN_VOID:
    type.kind = TYPE_VOID;
    break;
  case TOKEN_FIXED_TYPE: {
    const char *text = token->text;
    size_t digitStart = 0;
    while (text[digitStart] != '\0' && !isdigit((unsigned char)text[digitStart])) {
      ++digitStart;
    }
    int width = text[digitStart] != '\0' ? atoi(text + digitStart) : 0;
    type.width = width;
    if (strncmp(text, "int", 3) == 0) {
      type.kind = TYPE_INT;
    } else if (strncmp(text, "fp", 2) == 0) {
      type.kind = TYPE_FP;
    } else if (strncmp(text, "str", 3) == 0) {
      type.kind = TYPE_STR;
    } else {
      parserError("unknown fixed-width type");
    }
    break;
  }
  default:
    if (token->type == TOKEN_VARIABLE || token->type == TOKEN_MAIN) {
      parserError("user-defined types are not supported as type specifiers yet");
    } else {
      parserError("expected a type");
    }
    type.kind = TYPE_VOID;
    break;
  }

  currentToken++;
  return type;
}

static int isAssignmentToken(isTokenType type) {
  return type == TOKEN_EQUAL || type == TOKEN_ADD_ASSIGN ||
         type == TOKEN_SUB_ASSIGN || type == TOKEN_MUL_ASSIGN ||
         type == TOKEN_DIV_ASSIGN || type == TOKEN_MOD_ASSIGN ||
         type == TOKEN_BIT_AND_ASSIGN || type == TOKEN_BIT_OR_ASSIGN ||
         type == TOKEN_BIT_XOR_ASSIGN;
}

static ASTNode *parseAssignment(void);
static ASTNode *parseLogicalOr(void);
static ASTNode *parseLogicalXor(void);
static ASTNode *parseLogicalAnd(void);
static ASTNode *parseBitwiseOr(void);
static ASTNode *parseBitwiseXor(void);
static ASTNode *parseBitwiseAnd(void);
static ASTNode *parseEquality(void);
static ASTNode *parseRelational(void);
static ASTNode *parseShift(void);
static ASTNode *parseAdditive(void);
static ASTNode *parseMultiplicative(void);
static ASTNode *parseUnary(void);
static ASTNode *parsePostfix(void);
static ASTNode *parseBlock(void);
static ASTNode *parseDeclaration(int modifiers);
static ASTNode *parseArrayInitializer(void);
static ASTNode *parseIf(void);
static ASTNode *parseSwitch(void);
static ASTNode *parseDuring(void);
static ASTNode *parseRepeat(void);
static ASTNode *parseAttempt(void);
static ASTNode *parseAggregate(ASTType aggregateType);
static ASTNode *parseAlias(void);
static ASTNode *parseClass(void);
static ASTNode *parseInclude(void);

static int isMethodAhead(void) {
  int offset = 0;
  if (!isTypeToken(peekToken(offset)->type)) {
    return 0;
  }
  offset++;
  if (peekToken(offset)->type == TOKEN_FUNC) {
    offset++;
  }
  if (!isIdentifierToken(peekToken(offset)->type)) {
    return 0;
  }
  offset++;
  return peekToken(offset)->type == TOKEN_LEFT_PAREN;
}

int isMethod(void) {
  return isMethodAhead();
}

static void setTypeAndValue(ASTNode *node, Type type) {
  node->declaredType = type;
  switch (type.kind) {
  case TYPE_INT: node->valueType = VALUE_INT; break;
  case TYPE_FP: node->valueType = VALUE_FP; break;
  case TYPE_STR: node->valueType = VALUE_STR; break;
  case TYPE_VOID: node->valueType = VALUE_VOID; break;
  }
}

static ASTNode *parseArrayInitializer(void) {
  expect(TOKEN_LEFT_SQUARE_PAREN);
  ASTNode *first = NULL;
  ASTNode *last = NULL;

  if (!match(TOKEN_RIGHT_SQUARE_PAREN)) {
    do {
      ASTNode *item = parseAssignment();
      appendNode(&first, &last, item);
      if (match(TOKEN_RIGHT_SQUARE_PAREN)) {
        break;
      }
      expect(TOKEN_COMMA);
    } while (1);
  }

  ASTNode *array = newAST(AST_ARRAY_DECL);
  array->child = first;
  return array;
}

static ASTNode *parseDeclaration(int modifiers) {
  Type type = parseTypeSpec();
  int pointerFlag = 0;
  /* Fixed-width declarations use type@name; '@' is declarative here. */
  (void)match(TOKEN_AT);
  if (match(TOKEN_PTR)) {
    pointerFlag = DECL_FLAG_POINTER;
  } else if (match(TOKEN_DPTR)) {
    pointerFlag = DECL_FLAG_DOUBLE_POINTER;
  }

  Token *name = expectIdentifier();
  int stringCapacity = -1;
  int arrayCount = -1;

  /* [N] is a string buffer size; <N> is the documented array count. */
  if (match(TOKEN_LEFT_SQUARE_PAREN)) {
    Token *size = expect(TOKEN_NUMBER);
    stringCapacity = parseCapacity(size, "string buffer");
    expect(TOKEN_RIGHT_SQUARE_PAREN);
  }
  if (match(TOKEN_LEFT_VEC_PAREN)) {
    Token *count = expect(TOKEN_NUMBER);
    arrayCount = parseCapacity(count, "array");
    expect(TOKEN_RIGHT_VEC_PAREN);
  }

  ASTNode *initializer = NULL;
  if (match(TOKEN_EQUAL)) {
    if (current()->type == TOKEN_LEFT_SQUARE_PAREN) {
      initializer = parseArrayInitializer();
    } else {
      initializer = parseAssignment();
    }
  }

  if (arrayCount >= 0 || (initializer != NULL && initializer->type == AST_ARRAY_DECL)) {
    ASTNode *node = initializer != NULL && initializer->type == AST_ARRAY_DECL
                        ? initializer
                        : newAST(AST_ARRAY_DECL);
    copyText(node->name, name->text);
    setTypeAndValue(node, type);
    node->storageFlags = modifiers | pointerFlag;
    node->typeSize = stringCapacity >= 0
                         ? stringCapacity
                         : (type.kind == TYPE_STR ? type.width : 0);
    if (arrayCount >= 0) {
      node->arrayCount = arrayCount;
    } else {
      node->arrayCount = 0;
      for (ASTNode *item = node->child; item != NULL; item = item->next) {
        node->arrayCount++;
      }
    }
    if (initializer == NULL || initializer->type != AST_ARRAY_DECL) {
      node->child = initializer;
    }
    return node;
  }

  ASTNode *node;
  switch (type.kind) {
  case TYPE_INT: node = newAST(AST_INT_DECL); break;
  case TYPE_FP: node = newAST(AST_FP_DECL); break;
  case TYPE_STR: node = newAST(AST_STR_DECL); break;
  case TYPE_VOID: node = newAST(AST_DECL); break;
  default: node = newAST(AST_DECL); break;
  }

  copyText(node->name, name->text);
  setTypeAndValue(node, type);
  node->storageFlags = modifiers | pointerFlag;
  if (type.kind == TYPE_STR && pointerFlag == 0) {
    node->storageFlags |= DECL_FLAG_STRING_BUFFER;
  }
  node->typeSize = stringCapacity >= 0 ? stringCapacity : (type.width > 0 ? type.width : 0);
  node->child = initializer;
  if (type.kind == TYPE_STR && pointerFlag == 0 && node->typeSize == 0) {
    if (initializer != NULL && initializer->type == AST_STRING) {
      /* Match the exact inferred capacity emitted by the C backend. */
      node->typeSize = stringContentLength(initializer->strValue);
    } else {
      /* A runtime-produced string needs a conservative default buffer. */
      node->typeSize = MAX_NAME - 1;
    }
  }

  /* Preserve the legacy scalar fields used by the current code generator. */
  if (initializer != NULL) {
    if (type.kind == TYPE_INT && initializer->type == AST_NUM &&
        initializer->valueType == VALUE_INT) {
      node->value = initializer->value;
    } else if (type.kind == TYPE_INT && initializer->type == AST_UNARY &&
               strcmp(initializer->op, "-") == 0 && initializer->child != NULL &&
               initializer->child->type == AST_NUM &&
               initializer->child->valueType == VALUE_INT) {
      node->value = -initializer->child->value;
    } else if (type.kind == TYPE_FP && initializer->type == AST_NUM) {
      node->fpValue = initializer->valueType == VALUE_FP
                          ? initializer->fpValue
                          : (double)initializer->value;
    } else if (type.kind == TYPE_FP && initializer->type == AST_UNARY &&
               strcmp(initializer->op, "-") == 0 && initializer->child != NULL &&
               initializer->child->type == AST_NUM) {
      node->fpValue = -(initializer->child->valueType == VALUE_FP
                            ? initializer->child->fpValue
                            : (double)initializer->child->value);
    } else if (type.kind == TYPE_STR && initializer->type == AST_STRING) {
      copyText(node->strValue, initializer->strValue);
      if (node->typeSize < 0 || node->typeSize == 0) {
        node->typeSize = stringContentLength(initializer->strValue);
      }
    }
  } else if (type.kind == TYPE_STR && node->typeSize < 0) {
    node->typeSize = 0;
  }

  return node;
}

ASTNode *parseIntDecl(void) {
  if (current()->type != TOKEN_INT) {
    parserError("parseIntDecl called when the current token is not int");
  }
  return parseDeclaration(0);
}

ASTNode *parseFpDecl(void) {
  if (current()->type != TOKEN_FP) {
    parserError("parseFpDecl called when the current token is not fp");
  }
  return parseDeclaration(0);
}

ASTNode *parseStrDecl(void) {
  if (current()->type != TOKEN_STR) {
    parserError("parseStrDecl called when the current token is not str");
  }
  return parseDeclaration(0);
}

static int parseFunctionFlags(Method *method) {
  if (!match(TOKEN_LEFT_VEC_PAREN)) {
    return 0;
  }

  int depth = 1;
  while (depth > 0) {
    Token *token = current();
    if (token == NULL || token->type == TOKEN_EOF) {
      parserError("unterminated function flags; expected '>'");
    }
    if (token->type == TOKEN_LEFT_VEC_PAREN) {
      depth++;
      appendText(method->flags, "<");
      currentToken++;
    } else if (token->type == TOKEN_RIGHT_VEC_PAREN) {
      depth--;
      currentToken++;
      if (depth > 0) {
        appendText(method->flags, ">");
      }
    } else {
      appendText(method->flags, token->text);
      currentToken++;
    }
  }

  if (strcmp(method->flags, "void") == 0) {
    method->processType.kind = TYPE_VOID;
    method->processType.width = 0;
  }
  return 1;
}

ASTNode *parseMethod(void) {
  Type returnType = parseTypeSpec();
  (void)match(TOKEN_FUNC); /* Legacy form: int func name(...). */

  Token *name = expectIdentifier();
  ASTNode *node = newAST(AST_METHOD);
  node->method = (Method *)calloc(1, sizeof(Method));
  if (node->method == NULL) {
    fprintf(stderr, "parser error: out of memory allocating method\n");
    exit(1);
  }

  Method *method = node->method;
  method->returnType = returnType;
  method->name = copyString(name->text);
  method->isMain = name->type == TOKEN_MAIN || strcmp(name->text, "main") == 0;
  method->processType.kind = TYPE_VOID;
  method->processType.width = 0;
  setTypeAndValue(node, returnType);
  copyText(node->name, name->text);

  expect(TOKEN_LEFT_PAREN);
  if (!match(TOKEN_RIGHT_PAREN)) {
    for (;;) {
      Param param;
      param.storageFlags = 0;
      while (match(TOKEN_CONST)) {
        param.storageFlags |= DECL_FLAG_CONST;
      }
      param.type = parseTypeSpec();
      (void)match(TOKEN_AT);
      if (match(TOKEN_PTR)) {
        param.storageFlags |= DECL_FLAG_POINTER;
      } else if (match(TOKEN_DPTR)) {
        param.storageFlags |= DECL_FLAG_DOUBLE_POINTER;
      }
      Token *paramName = expectIdentifier();
      param.name = copyString(paramName->text);
      param.line = paramName->line;
      param.column = paramName->column;

      Param *grown = (Param *)realloc(
          method->params, sizeof(Param) * (size_t)(method->paramCount + 1));
      if (grown == NULL) {
        fprintf(stderr, "parser error: out of memory allocating parameters\n");
        exit(1);
      }
      method->params = grown;
      method->params[method->paramCount++] = param;

      if (match(TOKEN_RIGHT_PAREN)) {
        break;
      }
      expect(TOKEN_COMMA);
    }
  }

  (void)parseFunctionFlags(method);
  ASTNode *body = parseBlock();
  node->child = body;
  method->body = body->child;
  return node;
}

static ASTNode *parseBlock(void) {
  expect(TOKEN_LEFT_CURLY_PAREN);
  ASTNode *block = newAST(AST_BLOCK);
  ASTNode *first = NULL;
  ASTNode *last = NULL;

  skipNewlines();
  while (current()->type != TOKEN_RIGHT_CURLY_PAREN) {
    if (current()->type == TOKEN_EOF) {
      parserError("unterminated block; expected '}'");
    }

    ASTNode *statement = parseStatement();
    appendNode(&first, &last, statement);

    if (match(TOKEN_NEWLINE)) {
      skipNewlines();
    } else if (current()->type != TOKEN_RIGHT_CURLY_PAREN &&
               current()->type != TOKEN_ELIF && current()->type != TOKEN_ELSE) {
      parserError("expected end of line or '}' after statement");
    }
  }
  expect(TOKEN_RIGHT_CURLY_PAREN);
  block->child = first;
  return block;
}

static ASTNode *parsePrimaryInternal(void) {
  Token *token = current();
  ASTNode *node;

  if (token->type == TOKEN_NUMBER) {
    node = newAST(AST_NUM);
    if (strchr(token->text, '.') != NULL) {
      node->valueType = VALUE_FP;
      node->fpValue = strtod(token->text, NULL);
    } else {
      char *end = NULL;
      errno = 0;
      long long value = strtoll(token->text, &end, 10);
      if (errno == ERANGE || end == token->text || *end != '\0') {
        diagnosticError("error", "E2001",
                        "integer literal is outside the supported signed 64-bit range",
                        token->line, token->column,
                        "this integer cannot be represented by the compiler",
                        "use a value between -9223372036854775808 and 9223372036854775807, or split the calculation into smaller values.");
      }
      node->valueType = VALUE_INT;
      node->value = value;
    }
    currentToken++;
    return node;
  }

  if (token->type == TOKEN_STRING) {
    node = newAST(AST_STRING);
    node->valueType = VALUE_STR;
    copyText(node->strValue, token->text);
    currentToken++;
    return node;
  }

  if (token->type == TOKEN_TRUE || token->type == TOKEN_FALSE) {
    node = newAST(AST_BOOL);
    node->valueType = VALUE_INT;
    node->value = token->type == TOKEN_TRUE ? 1 : 0;
    copyText(node->strValue, token->text);
    currentToken++;
    return node;
  }

  if (isIdentifierToken(token->type) || token->type == TOKEN_OUT ||
      token->type == TOKEN_IN) {
    node = newAST(AST_VAR);
    node->valueType = VALUE_VOID;
    copyText(node->name, token->text);
    currentToken++;
    return node;
  }

  if (match(TOKEN_LEFT_PAREN)) {
    node = parseAssignment();
    expect(TOKEN_RIGHT_PAREN);
    return node;
  }

  parserError("expected expression");
  return NULL;
}

static void flattenCallableName(ASTNode *node, char output[MAX_NAME]) {
  if (node == NULL) {
    return;
  }
  if (node->type == AST_VAR) {
    appendText(output, node->name);
  } else if (node->type == AST_MEMBER || node->type == AST_NAMESPACE_ACCESS) {
    flattenCallableName(node->child, output);
    appendText(output, node->op[0] != '\0' ? node->op : ".");
    appendText(output, node->name);
  } else {
    parserError("only a function or member name can be called");
  }
}

static ASTNode *parseArgumentList(char *callName) {
  ASTNode *first = NULL;
  ASTNode *last = NULL;

  expect(TOKEN_LEFT_PAREN);
  if (!match(TOKEN_RIGHT_PAREN)) {
    for (;;) {
      ASTNode *argument = parseAssignment();
      appendNode(&first, &last, argument);
      if (match(TOKEN_RIGHT_PAREN)) {
        break;
      }
      expect(TOKEN_COMMA);
    }
  }

  ASTNode *call = newAST(AST_CALL);
  copyText(call->name, callName);
  call->child = first;
  return call;
}

static ASTNode *parsePostfix(void) {
  ASTNode *node = parsePrimaryInternal();

  for (;;) {
    if (current()->type == TOKEN_LEFT_PAREN) {
      char callName[MAX_NAME] = "";
      flattenCallableName(node, callName);
      int callLine = node->line;
      int callColumn = node->column;
      node = parseArgumentList(callName);
      node->line = callLine;
      node->column = callColumn;
      continue;
    }

    if (current()->type == TOKEN_LEFT_SQUARE_PAREN) {
      Token *bracket = current();
      currentToken++;
      ASTNode *index = parseAssignment();
      expect(TOKEN_RIGHT_SQUARE_PAREN);
      ASTNode *indexed = newAST(AST_INDEX);
      indexed->line = bracket->line;
      indexed->column = bracket->column;
      indexed->child = node;
      indexed->right = index;
      node = indexed;
      continue;
    }

    if (current()->type == TOKEN_PERIOD || current()->type == TOKEN_NAMESPACE) {
      isTokenType accessType = current()->type;
      currentToken++;
      Token *member = expectIdentifier();
      ASTNode *access = newAST(accessType == TOKEN_PERIOD
                                  ? AST_MEMBER
                                  : AST_NAMESPACE_ACCESS);
      access->child = node;
      access->operatorType = accessType;
      copyOperator(access->op, accessType == TOKEN_PERIOD ? "." : "::");
      copyText(access->name, member->text);
      access->line = member->line;
      access->column = member->column;
      node = access;
      continue;
    }

    if (current()->type == TOKEN_INCREMENT || current()->type == TOKEN_DECREMENT) {
      Token *op = current();
      ASTNode *update = newAST(AST_UPDATE);
      update->child = node;
      update->operatorType = op->type;
      copyOperator(update->op, op->text);
      currentToken++;
      node = update;
      continue;
    }

    break;
  }
  return node;
}

ASTNode *parsePrimary(void) {
  return parsePostfix();
}

ASTNode *parseCall(void) {
  ASTNode *node = parsePostfix();
  if (node->type != AST_CALL) {
    parserError("expected function call");
  }
  return node;
}

static ASTNode *parseUnary(void) {
  /* The magnitude of LLONG_MIN is one larger than LLONG_MAX, so it cannot
     be parsed as a positive literal and negated afterward. Recognize the
     spelling directly so the signed 64-bit minimum remains representable. */
  if (current()->type == TOKEN_SUB && peekToken(1) != NULL &&
      peekToken(1)->type == TOKEN_NUMBER &&
      strcmp(peekToken(1)->text, "9223372036854775808") == 0) {
    Token *minus = current();
    ASTNode *node = newAST(AST_NUM);
    node->line = minus->line;
    node->column = minus->column;
    node->valueType = VALUE_INT;
    node->value = LLONG_MIN;
    currentToken += 2;
    return node;
  }

  isTokenType type = current()->type;
  if (type == NOT || type == XOR || type == TOKEN_SUB || type == TOKEN_PTR ||
      type == TOKEN_DEREF || type == TOKEN_DPTR || type == TOKEN_DDEREF ||
      type == TOKEN_AT) {
    Token *op = current();
    currentToken++;
    ASTNode *node = newAST(AST_UNARY);
    node->line = op->line;
    node->column = op->column;
    node->operatorType = type;
    copyOperator(node->op, op->text);
    node->child = parseUnary();
    return node;
  }
  return parsePostfix();
}

static ASTNode *makeBinary(ASTType type, isTokenType operatorType,
                           ASTNode *left, ASTNode *right, const char *op,
                           int line, int column) {
  ASTNode *node = newAST(type);
  node->child = left;
  node->right = right;
  node->line = line;
  node->column = column;
  node->operatorType = operatorType;
  copyOperator(node->op, op);
  return node;
}

static ASTNode *parseMultiplicative(void) {
  ASTNode *left = parseUnary();
  while (current()->type == TOKEN_MUL || current()->type == TOKEN_DIV ||
         current()->type == TOKEN_MOD) {
    Token *op = current();
    currentToken++;
    ASTNode *right = parseUnary();
    ASTType type = op->type == TOKEN_MUL ? AST_MUL
                   : op->type == TOKEN_DIV ? AST_DIV : AST_MOD;
    left = makeBinary(type, op->type, left, right, op->text, op->line, op->column);
  }
  return left;
}

static ASTNode *parseAdditive(void) {
  ASTNode *left = parseMultiplicative();
  while (current()->type == TOKEN_ADD || current()->type == TOKEN_SUB) {
    Token *op = current(); currentToken++;
    ASTNode *right = parseMultiplicative();
    left = makeBinary(op->type == TOKEN_ADD ? AST_ADD : AST_SUB,
                      op->type, left, right, op->text, op->line, op->column);
  }
  return left;
}

static ASTNode *parseShift(void) {
  ASTNode *left = parseAdditive();
  while (current()->type == TOKEN_SHIFT_LEFT || current()->type == TOKEN_SHIFT_RIGHT) {
    Token *op = current(); currentToken++;
    ASTNode *right = parseAdditive();
    left = makeBinary(op->type == TOKEN_SHIFT_LEFT ? AST_SHIFT_LEFT : AST_SHIFT_RIGHT,
                      op->type, left, right, op->text, op->line, op->column);
  }
  return left;
}

static ASTNode *parseRelational(void) {
  ASTNode *left = parseShift();
  while (current()->type == TOKEN_LEFT_VEC_PAREN ||
         current()->type == TOKEN_RIGHT_VEC_PAREN ||
         current()->type == TOKEN_LESS_EQUAL ||
         current()->type == TOKEN_GREATER_EQUAL) {
    Token *op = current(); currentToken++;
    ASTType type = op->type == TOKEN_LEFT_VEC_PAREN ? AST_LESS
                   : op->type == TOKEN_RIGHT_VEC_PAREN ? AST_GREATER
                   : op->type == TOKEN_LESS_EQUAL ? AST_LESS_EQUAL
                   : AST_GREATER_EQUAL;
    ASTNode *right = parseShift();
    left = makeBinary(type, op->type, left, right, op->text, op->line, op->column);
  }
  return left;
}

static ASTNode *parseEquality(void) {
  ASTNode *left = parseRelational();
  while (current()->type == TOKEN_EQUAL_EQUAL || current()->type == TOKEN_NOT_EQUAL) {
    Token *op = current(); currentToken++;
    ASTNode *right = parseRelational();
    left = makeBinary(op->type == TOKEN_EQUAL_EQUAL ? AST_EQUAL : AST_NOT_EQUAL,
                      op->type, left, right, op->text, op->line, op->column);
  }
  return left;
}

static ASTNode *parseBitwiseAnd(void) {
  ASTNode *left = parseEquality();
  while (match(TOKEN_BIT_AND)) {
    Token *op = &tokens[currentToken - 1];
    ASTNode *right = parseEquality();
    left = makeBinary(AST_BIT_AND, op->type, left, right, op->text, op->line, op->column);
  }
  return left;
}

static ASTNode *parseBitwiseXor(void) {
  ASTNode *left = parseBitwiseAnd();
  while (current()->type == TOKEN_BIT_XOR ||
         current()->type == TOKEN_SPECIAL_BITWISE) {
    Token *op = current();
    currentToken++;
    ASTNode *right = parseBitwiseAnd();
    ASTType type = op->type == TOKEN_BIT_XOR ? AST_BIT_XOR : AST_SPECIAL_BITWISE;
    left = makeBinary(type, op->type, left, right, op->text, op->line, op->column);
  }
  return left;
}

static ASTNode *parseBitwiseOr(void) {
  ASTNode *left = parseBitwiseXor();
  while (match(TOKEN_BIT_OR)) {
    Token *op = &tokens[currentToken - 1];
    ASTNode *right = parseBitwiseXor();
    left = makeBinary(AST_BIT_OR, op->type, left, right, op->text, op->line, op->column);
  }
  return left;
}

static ASTNode *parseLogicalAnd(void) {
  ASTNode *left = parseBitwiseOr();
  while (current()->type == AND || current()->type == NAND) {
    Token *op = current(); currentToken++;
    ASTNode *right = parseBitwiseOr();
    ASTType type = op->type == AND ? AST_LOGICAL_AND : AST_LOGICAL_NAND;
    left = makeBinary(type, op->type, left, right, op->text, op->line, op->column);
  }
  return left;
}

static ASTNode *parseLogicalXor(void) {
  ASTNode *left = parseLogicalAnd();
  while (current()->type == XOR || current()->type == XNOR) {
    Token *op = current(); currentToken++;
    ASTNode *right = parseLogicalAnd();
    ASTType type = op->type == XOR ? AST_LOGICAL_XOR : AST_LOGICAL_XNOR;
    left = makeBinary(type, op->type, left, right, op->text, op->line, op->column);
  }
  return left;
}

static ASTNode *parseLogicalOr(void) {
  ASTNode *left = parseLogicalXor();
  while (current()->type == OR || current()->type == NOR) {
    Token *op = current(); currentToken++;
    ASTNode *right = parseLogicalXor();
    ASTType type = op->type == OR ? AST_LOGICAL_OR : AST_LOGICAL_NOR;
    left = makeBinary(type, op->type, left, right, op->text, op->line, op->column);
  }
  return left;
}

static ASTNode *parseAssignment(void) {
  ASTNode *left = parseLogicalOr();
  if (isAssignmentToken(current()->type)) {
    Token *op = current(); currentToken++;
    ASTNode *right = parseAssignment();
    ASTNode *node = newAST(AST_ASSIGN);
    node->line = op->line;
    node->column = op->column;
    node->child = left;
    node->right = right;
    node->operatorType = op->type;
    copyOperator(node->op, op->text);
    return node;
  }
  return left;
}

/* Public expression entry points retained for callers of the original parser. */
ASTNode *parseTerm(void) {
  return parseMultiplicative();
}

ASTNode *parseExpression(void) {
  return parseAssignment();
}

ASTNode *parseOut(void) {
  Token *token = expect(TOKEN_OUT);
  (void)token;
  expect(TOKEN_LEFT_PAREN);
  ASTNode *expression = parseAssignment();
  expect(TOKEN_RIGHT_PAREN);
  ASTNode *node = newAST(AST_OUT);
  node->child = expression;
  return node;
}

ASTNode *parseRetreat(void) {
  expect(TOKEN_RETREAT);
  ASTNode *node = newAST(AST_RETREAT);
  if (current()->type != TOKEN_NEWLINE &&
      current()->type != TOKEN_RIGHT_CURLY_PAREN &&
      current()->type != TOKEN_EOF) {
    node->child = parseAssignment();
  }
  return node;
}

static ASTNode *parseIfBranch(isTokenType keyword) {
  expect(keyword);
  ASTNode *node = newAST(AST_IF);
  expect(TOKEN_LEFT_PAREN);
  node->child = parseAssignment();
  expect(TOKEN_RIGHT_PAREN);
  node->right = parseBlock();

  int saved = currentToken;
  skipNewlines();
  if (current()->type == TOKEN_ELIF) {
    node->third = parseIfBranch(TOKEN_ELIF);
  } else if (match(TOKEN_ELSE)) {
    node->third = parseBlock();
  } else {
    currentToken = saved;
  }
  return node;
}

static ASTNode *parseIf(void) {
  return parseIfBranch(TOKEN_IF);
}

static ASTNode *parseSwitch(void) {
  expect(TOKEN_SWITCH);
  ASTNode *node = newAST(AST_SWITCH);
  if (match(TOKEN_LEFT_PAREN)) {
    node->child = parseAssignment();
    expect(TOKEN_RIGHT_PAREN);
  } else {
    node->child = parseAssignment();
  }
  expect(TOKEN_LEFT_CURLY_PAREN);
  ASTNode *firstCase = NULL;
  ASTNode *lastCase = NULL;
  skipNewlines();

  while (current()->type != TOKEN_RIGHT_CURLY_PAREN) {
    if (current()->type == TOKEN_EOF) {
      parserError("unterminated switch; expected '}'");
    }

    ASTNode *caseNode = newAST(AST_CASE);
    if (match(TOKEN_ARG)) {
      caseNode->child = parseAssignment();
      expect(TOKEN_COLON);
    } else if (match(TOKEN_INCASE)) {
      caseNode->flags[0] = 'd'; /* default/incase branch */
      expect(TOKEN_COLON);
    } else {
      parserError("expected 'arg' or 'incase' in switch");
    }

    ASTNode *firstStatement = NULL;
    ASTNode *lastStatement = NULL;
    skipNewlines();
    while (current()->type != TOKEN_ARG && current()->type != TOKEN_INCASE &&
           current()->type != TOKEN_RIGHT_CURLY_PAREN) {
      ASTNode *statement = parseStatement();
      appendNode(&firstStatement, &lastStatement, statement);
      if (match(TOKEN_NEWLINE)) {
        skipNewlines();
      } else if (current()->type != TOKEN_ARG && current()->type != TOKEN_INCASE &&
                 current()->type != TOKEN_RIGHT_CURLY_PAREN) {
        parserError("expected end of line in switch case");
      }
    }
    caseNode->right = firstStatement;
    appendNode(&firstCase, &lastCase, caseNode);
    skipNewlines();
  }

  expect(TOKEN_RIGHT_CURLY_PAREN);
  node->right = firstCase;
  return node;
}

static ASTNode *parseDuring(void) {
  expect(TOKEN_DURING);
  ASTNode *node = newAST(AST_DURING);
  expect(TOKEN_LEFT_PAREN);
  node->child = parseAssignment();
  expect(TOKEN_RIGHT_PAREN);
  node->right = parseBlock();
  return node;
}

static ASTNode *parseRepeat(void) {
  expect(TOKEN_REPEAT);
  ASTNode *node = newAST(AST_REPEAT);
  expect(TOKEN_LEFT_PAREN);

  if (current()->type != TOKEN_COMMA) {
    if (isTypeToken(current()->type) || current()->type == TOKEN_CONST ||
        current()->type == TOKEN_STATIC || current()->type == TOKEN_GLOBAL) {
      int modifiers = 0;
      while (current()->type == TOKEN_CONST || current()->type == TOKEN_STATIC ||
             current()->type == TOKEN_GLOBAL) {
        isTokenType consumed = current()->type;
        currentToken++;
        modifiers |= consumed == TOKEN_CONST ? DECL_FLAG_CONST
                     : consumed == TOKEN_STATIC ? DECL_FLAG_STATIC
                     : DECL_FLAG_GLOBAL;
      }
      node->child = parseDeclaration(modifiers);
    } else {
      node->child = parseAssignment();
    }
  }
  expect(TOKEN_COMMA);
  if (current()->type != TOKEN_COMMA) {
    node->right = parseAssignment();
  }
  expect(TOKEN_COMMA);
  if (current()->type != TOKEN_RIGHT_PAREN) {
    node->third = parseAssignment();
  }
  expect(TOKEN_RIGHT_PAREN);
  node->body = parseBlock();
  return node;
}

static ASTNode *parseAttempt(void) {
  expect(TOKEN_ATTEMPT);
  ASTNode *node = newAST(AST_ATTEMPT);
  node->child = parseBlock();
  int saved = currentToken;
  skipNewlines();
  if (!match(TOKEN_INCASE)) {
    currentToken = saved;
    parserError("expected 'incase' after attempt block");
  }
  node->right = parseBlock();
  return node;
}

static ASTNode *parseAggregate(ASTType aggregateType) {
  currentToken++; /* consume data or set */
  ASTNode *node = newAST(aggregateType);
  if (aggregateType == AST_DATA || aggregateType == AST_CLASS) {
    if (current()->type == TOKEN_VARIABLE) {
      copyText(node->name, current()->text);
      currentToken++;
    }
  }
  expect(TOKEN_LEFT_CURLY_PAREN);
  ASTNode *first = NULL;
  ASTNode *last = NULL;
  skipNewlines();
  while (current()->type != TOKEN_RIGHT_CURLY_PAREN) {
    if (current()->type == TOKEN_EOF) {
      parserError("unterminated aggregate; expected '}'");
    }
    ASTNode *member;
    if (aggregateType == AST_DATA || aggregateType == AST_CLASS) {
      member = parseStatement();
    } else {
      member = newAST(AST_VAR);
      copyText(member->name, expectIdentifier()->text);
    }
    appendNode(&first, &last, member);
    if (match(TOKEN_COMMA) || match(TOKEN_NEWLINE)) {
      skipNewlines();
    } else if (current()->type != TOKEN_RIGHT_CURLY_PAREN) {
      parserError("expected newline, comma, or '}' in aggregate");
    }
  }
  expect(TOKEN_RIGHT_CURLY_PAREN);
  node->child = first;
  return node;
}

static ASTNode *parseAlias(void) {
  expect(TOKEN_ALIAS);
  ASTNode *node = newAST(AST_ALIAS);
  if (current()->type == TOKEN_SET) {
    node->child = parseAggregate(AST_SET);
    if (isIdentifierToken(current()->type)) {
      copyText(node->name, expectIdentifier()->text);
    }
  } else if (isTypeToken(current()->type)) {
    Type type = parseTypeSpec();
    node->declaredType = type;
    copyText(node->name, expectIdentifier()->text);
  } else {
    parserError("expected a type or set after alias");
  }
  return node;
}

static ASTNode *parseClass(void) {
  expect(TOKEN_CLASS);
  ASTNode *node = newAST(AST_CLASS);
  copyText(node->name, expectIdentifier()->text);
  expect(TOKEN_LEFT_CURLY_PAREN);
  ASTNode *first = NULL;
  ASTNode *last = NULL;
  skipNewlines();
  while (current()->type != TOKEN_RIGHT_CURLY_PAREN) {
    int modifiers = 0;
    if (match(TOKEN_PUBLIC)) modifiers |= DECL_FLAG_PUBLIC;
    else if (match(TOKEN_PRIVATE)) modifiers |= DECL_FLAG_PRIVATE;
    else if (match(TOKEN_HIDDEN)) modifiers |= DECL_FLAG_HIDDEN;
    ASTNode *member = parseStatement();
    member->storageFlags |= modifiers;
    appendNode(&first, &last, member);
    if (match(TOKEN_NEWLINE)) skipNewlines();
    else if (current()->type != TOKEN_RIGHT_CURLY_PAREN) parserError("expected end of line in class");
  }
  expect(TOKEN_RIGHT_CURLY_PAREN);
  node->child = first;
  return node;
}

static ASTNode *parseInclude(void) {
  expect(TOKEN_INCLUDE);
  ASTNode *node = newAST(AST_INCLUDE);
  Token *library = expectIdentifier();
  copyText(node->name, library->text);

  /* Package manifests use the documented '&folder' shorthand. */
  if (strcmp(node->name, "manifest") == 0) {
    diagnosticError("error", "E2004",
                    "`?manifest` is not supported; use `&folder` to load a package manifest",
                    library->line, library->column,
                    "this import form is intentionally rejected",
                    "use `&logs` to load `logs/logs.ufg` from the source directory.");
  }

  ASTNode *first = NULL;
  ASTNode *last = NULL;

  for (;;) {
    int beforeNewlines = currentToken;
    int previousLine = currentToken > 0 ? tokens[currentToken - 1].line : 0;
    skipNewlines();
    if (!match(TOKEN_WITH)) {
      currentToken = beforeNewlines;
      break;
    }
    Token *withToken = &tokens[currentToken - 1];
    if (withToken->line <= previousLine) {
      diagnosticError("error", "E2005",
                      "`with` must start on its own line after the library root or previous module list",
                      withToken->line, withToken->column,
                      "the import root and each `with` clause must use separate lines",
                      "write `?std` on one line, then `with io, math` on the next line.");
    }
    for (;;) {
      Token *module = expectIdentifier();
      ASTNode *item = newAST(AST_VAR);
      copyText(item->name, module->text);
      appendNode(&first, &last, item);
      if (!match(TOKEN_COMMA))
        break;
    }
  }

  node->child = first;
  return node;
}

static ASTNode *parseKill(void) {
  expect(TOKEN_KILL);
  ASTNode *node = newAST(AST_KILL);
  if (current()->type == TOKEN_KILL_SILENT || current()->type == TOKEN_KILL_FLAG) {
    node->line = current()->line;
    node->column = current()->column;
  }
  while (current()->type == TOKEN_KILL_SILENT || current()->type == TOKEN_KILL_FLAG) {
    if (node->flags[0] != '\0') appendText(node->flags, " ");
    appendText(node->flags, current()->text);
    int isReasonFlag = strncmp(current()->text, "-r", 2) == 0;
    currentToken++;
    if (isReasonFlag && current()->type == TOKEN_STRING) {
      appendText(node->flags, " \"");
      appendText(node->flags, current()->text);
      appendText(node->flags, "\"");
      currentToken++;
    }
  }
  return node;
}

static ASTNode *parseExpressionStatement(void) {
  ASTNode *expression = parseAssignment();
  ASTNode *node = newAST(AST_EXPRESSION_STATEMENT);
  node->child = expression;
  return node;
}

static ASTNode *parseStatementImpl(void) {
  if (isMethodAhead()) return parseMethod();

  int modifiers = 0;
  while (current()->type == TOKEN_CONST || current()->type == TOKEN_STATIC ||
         current()->type == TOKEN_GLOBAL) {
    isTokenType consumed = current()->type;
    currentToken++;
    modifiers |= consumed == TOKEN_CONST ? DECL_FLAG_CONST
                 : consumed == TOKEN_STATIC ? DECL_FLAG_STATIC
                 : DECL_FLAG_GLOBAL;
  }

  if (isTypeToken(current()->type)) return parseDeclaration(modifiers);
  if (modifiers != 0) parserError("storage modifier must precede a declaration");

  switch (current()->type) {
  case TOKEN_OUT: return parseOut();
  case TOKEN_RETREAT: return parseRetreat();
  case TOKEN_KILL: return parseKill();
  case TOKEN_IF: return parseIf();
  case TOKEN_SWITCH: return parseSwitch();
  case TOKEN_DURING: return parseDuring();
  case TOKEN_REPEAT: return parseRepeat();
  case TOKEN_ATTEMPT: return parseAttempt();
  case TOKEN_BREAK: currentToken++; return newAST(AST_BREAK);
  case TOKEN_CONT: currentToken++; return newAST(AST_CONT);
  case TOKEN_DATA: return parseAggregate(AST_DATA);
  case TOKEN_SET: return parseAggregate(AST_SET);
  case TOKEN_ALIAS: return parseAlias();
  case TOKEN_CLASS: return parseClass();
  case TOKEN_INCLUDE: return parseInclude();
  case TOKEN_VEC: {
    currentToken++;
    ASTNode *node = newAST(AST_VEC_DECL);
    if (isIdentifierToken(current()->type)) copyText(node->name, expectIdentifier()->text);
    if (match(TOKEN_EQUAL)) node->child = parseAssignment();
    return node;
  }
  case TOKEN_BIT_AND: {
    currentToken++; /* `&` at statement start means a manifest package. */
    Token *folder = expectIdentifier();
    ASTNode *node = newAST(AST_INCLUDE);
    ASTNode *item = newAST(AST_VAR);
    copyText(node->name, "manifest");
    copyText(item->name, folder->text);
    node->child = item;
    return node;
  }
  default:
    if (current()->type == TOKEN_VARIABLE && peekToken(1)->type == TOKEN_VARIABLE) {
      parserError("user-defined types are not supported as declaration types yet");
    }
    if (current()->type == TOKEN_VARIABLE || current()->type == TOKEN_MAIN ||
        current()->type == TOKEN_IN || current()->type == TOKEN_LEFT_PAREN ||
        current()->type == NOT || current()->type == XOR || current()->type == TOKEN_SUB ||
        current()->type == TOKEN_AT || current()->type == TOKEN_DEREF ||
        current()->type == TOKEN_DDEREF || current()->type == TOKEN_NUMBER ||
        current()->type == TOKEN_STRING || current()->type == TOKEN_TRUE ||
        current()->type == TOKEN_FALSE) {
      return parseExpressionStatement();
    }
    parserError("unknown statement");
    return NULL;
  }
}

ASTNode *parseProgram(void) {
  currentToken = 0;
  parserFailed = 0;

  ASTNode *program = newAST(AST_PROGRAM);
  ASTNode *first = NULL;
  ASTNode *last = NULL;
  skipNewlines();

  while (current()->type != TOKEN_EOF) {
    ASTNode *statement = parseStatement();
    appendNode(&first, &last, statement);

    if (match(TOKEN_NEWLINE)) {
      skipNewlines();
    } else if (current()->type != TOKEN_EOF) {
      parserError("expected end of line after top-level statement");
    }
  }

  program->child = first;
  return program;
}


ASTNode *parseStatement(void) {
  Token *start = current();
  ASTNode *node = parseStatementImpl();
  if (node != NULL && start != NULL &&
      !(node->type == AST_KILL && node->flags[0] != '\0')) {
    node->line = start->line;
    node->column = start->column;
  }
  return node;
}
