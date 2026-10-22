#include "parse.h"

ASTNode astNodes[MAX_AST_NODES];
int astCount = 0;

ASTNode *newAST(ASTType type) {
  if (astCount >= MAX_AST_NODES) {
    printf("error, ast is too large\n");
    exit(1);
  }

  ASTNode *node = &astNodes[astCount++];
  memset(node, 0, sizeof(ASTNode));
  node->type = type;

  return node;
}

int currentToken = 0;
Token *current(void) { return &tokens[currentToken]; }

int match(isTokenType type) {
  if (current()->type == type) {
    currentToken++;
    return 1;
  }

  return 0;
}
void parserError(const char *message) {
  printf("parser error on line %d: %s\n", current()->line, message);
  exit(1);
}
Token *expect(isTokenType type) {
  if (current()->type != type) {
    parserError("unexpected token");
  }

  return &tokens[currentToken++];
}

// structures
ASTNode *parseIntDecl(void) { // int num = #
  expect(TOKEN_INT);
  Token *name = expect(TOKEN_VARIABLE);
  expect(TOKEN_EQUAL);
  int negative = 0;

  if (match(TOKEN_SUB)) {
    negative = 1;
  }

  Token *number = expect(TOKEN_NUMBER);

  ASTNode *node = newAST(AST_INT_DECL);
  strcpy(node->name, name->text);
  node->value = atoi(number->text);

  if (negative) {
    node->value = -node->value;
  }

  return node;
}
ASTNode *parseFpDecl(void) {
  expect(TOKEN_FP);
  Token *name = expect(TOKEN_VARIABLE);
  expect(TOKEN_EQUAL);
  Token *number = expect(TOKEN_NUMBER);

  ASTNode *node = newAST(AST_FP_DECL);
  strcpy(node->name, name->text);
  node->fpValue = atof(number->text);

  return node;
}
ASTNode *parseStrDecl() {
  expect(TOKEN_STR);
  Token *name = expect(TOKEN_VARIABLE);
  expect(TOKEN_LEFT_SQUARE_PAREN);
  Token *size = expect(TOKEN_NUMBER);
  expect(TOKEN_RIGHT_SQUARE_PAREN);
  expect(TOKEN_EQUAL);
  Token *string = expect(TOKEN_STRING);

  ASTNode *node = newAST(AST_STR_DECL);
  strcpy(node->name, name->text);
  node->typeSize = atoi(size->text);
  strcpy(node->strValue, string->text);

  return node;
}
ASTNode *parseOut(void) {
  ASTNode *node = newAST(AST_OUT);

  expect(TOKEN_OUT);
  expect(TOKEN_LEFT_PAREN);

  node->child = parseExpression();

  expect(TOKEN_RIGHT_PAREN);

  return node;
}
ASTNode *parseMethod(void) {
  ASTNode *node = newAST(AST_METHOD);
  node->method = malloc(sizeof(Method));
  Method *method = node->method;

  method->params = NULL;
  method->paramCount = 0;
  method->body = NULL;

  switch (current()->type) {

  case TOKEN_INT:
    method->returnType.kind = TYPE_INT;
    break;

  case TOKEN_FP:
    method->returnType.kind = TYPE_FP;
    break;

  case TOKEN_STR:
    method->returnType.kind = TYPE_STR;
    break;

  case TOKEN_VOID:
    method->returnType.kind = TYPE_VOID;
    break;

  default:
    parserError("expected method return type");
  }

  currentToken++;
  expect(TOKEN_FUNC);

  Token *name = expect(TOKEN_VARIABLE);
  method->name = strdup(name->text);
  expect(TOKEN_LEFT_PAREN);
  if (!match(TOKEN_RIGHT_PAREN)) {

    while (1) {

      Param param;

      switch (current()->type) {

      case TOKEN_INT:
        param.type.kind = TYPE_INT;
        break;

      case TOKEN_FP:
        param.type.kind = TYPE_FP;
        break;

      case TOKEN_STR:
        param.type.kind = TYPE_STR;
        break;

      case TOKEN_VOID:
        param.type.kind = TYPE_VOID;
        break;

      default:
        parserError("expected parameter type");
      }

      currentToken++;
      Token *paramName = expect(TOKEN_VARIABLE);
      param.name = strdup(paramName->text);
      method->params =
          realloc(method->params, sizeof(Param) * (method->paramCount + 1));
      method->params[method->paramCount++] = param;

      if (match(TOKEN_RIGHT_PAREN)) {
        break;
      }

      expect(TOKEN_COMMA);
    }
  }

  if (match(TOKEN_LEFT_VEC_PAREN)) {
    expect(TOKEN_VOID);
    expect(TOKEN_RIGHT_VEC_PAREN);
  }

  expect(TOKEN_LEFT_CURLY_PAREN);

  ASTNode *first = NULL;
  ASTNode *last = NULL;

  while (!match(TOKEN_RIGHT_CURLY_PAREN)) {

    if (match(TOKEN_NEWLINE)) {
      continue;
    }

    ASTNode *statement = parseStatement();

    if (first == NULL) {
      first = statement;
    } else {
      last->next = statement;
    }

    last = statement;

    if (match(TOKEN_NEWLINE)) {
      continue;
    }

    if (current()->type != TOKEN_RIGHT_CURLY_PAREN) {
      parserError("expected end of line");
    }
  }

  method->body = first;

  return node;
}
ASTNode *parseRetreat(void) {
  ASTNode *node = newAST(AST_RETREAT);

  expect(TOKEN_RETREAT);
  node->child = parseExpression();

  return node;
}


// helper
int isMethod(void) {
  isTokenType type = current()->type;

  if (type != TOKEN_INT && type != TOKEN_FP && type != TOKEN_STR &&
      type != TOKEN_VOID) {
    return 0;
  }

  return tokens[currentToken + 1].type == TOKEN_FUNC;
}

// parse setup
ASTNode *parsePrimary(void) {
  if (current()->type == TOKEN_NUMBER) {
    ASTNode* node = newAST(AST_NUM);

    if (strchr(current()->text, '.') != NULL) {
      node->valueType = VALUE_FP;
      node->fpValue = atof(current()->text);
    } else {
      node->valueType = VALUE_INT;
      node->value = atoi(current()->text);
    }
    currentToken++;

    return node;
  }

  // variable or function call
  if (current()->type == TOKEN_VARIABLE) {

    if (tokens[currentToken + 1].type == TOKEN_LEFT_PAREN) {
      return parseCall();
    }

    ASTNode *node = newAST(AST_VAR);

    strcpy(node->name, current()->text);

    currentToken++;

    return node;
  }

  // parenthesized expression
  if (match(TOKEN_LEFT_PAREN)) {
    ASTNode *node = parseExpression();

    expect(TOKEN_RIGHT_PAREN);

    return node;
  }

  parserError("expected expression");
  return NULL;
}
ASTNode *parseTerm(void) {
  ASTNode *left = parsePrimary();

  while (current()->type == TOKEN_MUL || current()->type == TOKEN_DIV) {

    isTokenType operator = current()->type;
    currentToken++;

    ASTNode *right = parsePrimary();

    ASTNode *node;

    if (operator == TOKEN_MUL) {
      node = newAST(AST_MUL);
    } else {
      node = newAST(AST_DIV);
    }

    node->child = left;
    node->right = right;

    left = node;
  }

  return left;
}
ASTNode *parseExpression(void) {
  ASTNode *left = parseTerm();

  while (current()->type == TOKEN_ADD || current()->type == TOKEN_SUB) {

    isTokenType operator = current()->type;
    currentToken++;

    ASTNode *right = parseTerm();

    ASTNode *node;

    if (operator == TOKEN_ADD) {
      node = newAST(AST_ADD);
    } else {
      node = newAST(AST_SUB);
    }

    node->child = left;
    node->right = right;

    left = node;
  }

  return left;
}
ASTNode *parseCall(void) {
  ASTNode* node = newAST(AST_CALL);

  Token* name = expect(TOKEN_VARIABLE);
  strcpy(node->name, name->text);

  expect(TOKEN_LEFT_PAREN);

  ASTNode* first = NULL;
  ASTNode *last = NULL;

  if (!match(TOKEN_RIGHT_PAREN)) {
    while (1) {
      ASTNode* argument = parseExpression();

      if (first == NULL) {
        first = argument;
      } else {
        last->next = argument;
      }

      last = argument;

      if (match(TOKEN_RIGHT_PAREN)) {
        break;
      }

      expect(TOKEN_COMMA);
    }
  }
  node->child = first;

  return node;
}
ASTNode *parseStatement(void) {

  if (isMethod()) {
    return parseMethod();
  }

  if (current()->type == TOKEN_INT) {
    return parseIntDecl();
  }

  if (current()->type == TOKEN_FP) {
    return parseFpDecl();
  }

  if (current()->type == TOKEN_STR) {
    return parseStrDecl();
  }

  if (current()->type == TOKEN_OUT) {
    return parseOut();
  }

  if (current()->type == TOKEN_RETREAT) {
    return parseRetreat();
  }

  parserError("unknown statement");
  return NULL;
}
ASTNode *parseProgram(void) {
  ASTNode *program = newAST(AST_PROGRAM);

  ASTNode *first = NULL;
  ASTNode *last = NULL;

  while (current()->type != TOKEN_EOF) {

    if (match(TOKEN_NEWLINE)) {
      continue;
    }

    ASTNode *statement = parseStatement();

    if (first == NULL) {
      first = statement;
    } else {
      last->next = statement;
    }

    last = statement;

    if (match(TOKEN_NEWLINE)) {
      continue;
    }

    if (current()->type != TOKEN_EOF) {
      parserError("expected end of line");
    }
  }

  program->child = first;

  return program;
}