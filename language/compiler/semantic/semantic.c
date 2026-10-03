#include "semantic.h"

char variables[MAX_AST_NODES][MAX_NAME];
ValueType variableTypes[MAX_AST_NODES];
int varCount = 0;

// helper
static ValueType typeFromType(Type type) {
  switch (type.kind) {
  case TYPE_INT:
    return VALUE_INT;

  case TYPE_FP:
    return VALUE_FP;

  case TYPE_STR:
    return VALUE_STR;

  case TYPE_VOID:
    return VALUE_VOID;
  }

  return VALUE_VOID;
}
static ASTNode *findMethod(ASTNode *program, const char *name) {
  ASTNode *node = program->child;

  while (node != NULL) {

    if (node->type == AST_METHOD && strcmp(node->method->name, name) == 0) {
      return node;
    }

    node = node->next;
  }

  return NULL;
}
static Param *findParam(Method *method, const char *name) {
  for (int i = 0; i < method->paramCount; i++) {

    if (strcmp(method->params[i].name, name) == 0) {
      return &method->params[i];
    }
  }

  return NULL;
}
static ValueType getVariableType(const char *name) {
  for (int i = 0; i < varCount; i++) {

    if (strcmp(variables[i], name) == 0) {
      return variableTypes[i];
    }
  }

  return VALUE_VOID;
}

// checks
static ValueType checkExpression(ASTNode *expr, ASTNode *program,
                                 Method *method) {

  if (expr == NULL) {
    semanticError("missing expression", "");
  }

  switch (expr->type) {

  case AST_NUM:
    return expr->valueType;

  case AST_VAR: {
    if (method != NULL) {
      Param *param = findParam(method, expr->name);

      if (param != NULL) {
        expr->valueType = typeFromType(param->type);
        return expr->valueType;
      }
    }

    if (!varExists(expr->name)) {
      semanticError("undefined variable", expr->name);
    }

    expr->valueType = getVariableType(expr->name);
    return expr->valueType;
  }

  case AST_ADD:
  case AST_SUB:
  case AST_MUL:
  case AST_DIV: {
    ValueType left = checkExpression(expr->child, program, method);

    ValueType right = checkExpression(expr->right, program, method);

    if ((left != VALUE_INT && left != VALUE_FP) ||
        (right != VALUE_INT && right != VALUE_FP)) {

      semanticError("arithmetic requires numbers", "");
    }

    if (left == VALUE_FP || right == VALUE_FP) {
      expr->valueType = VALUE_FP;
    } else {
      expr->valueType = VALUE_INT;
    }

    return expr->valueType;
  }

  case AST_CALL: {
    ASTNode *function = findMethod(program, expr->name);

    if (function == NULL) {
      semanticError("undefined function", expr->name);
    }

    Method *called = function->method;

    int argumentCount = 0;

    for (ASTNode *arg = expr->child; arg != NULL; arg = arg->next) {
      argumentCount++;
    }

    if (argumentCount != called->paramCount) {
      semanticError("wrong number of arguments", expr->name);
    }

    ASTNode *arg = expr->child;

    for (int i = 0; i < called->paramCount; i++) {

      ValueType argumentType = checkExpression(arg, program, method);

      ValueType parameterType = typeFromType(called->params[i].type);

      if (argumentType != parameterType) {
        semanticError("argument type mismatch", expr->name);
      }

      arg = arg->next;
    }

    expr->valueType = typeFromType(called->returnType);

    return expr->valueType;
  }

  default:
    semanticError("invalid expression", "");
    return VALUE_VOID;
  }
}
static void checkMethod(ASTNode *program, ASTNode *methodNode) {
  Method *method = methodNode->method;

  ASTNode *statement = method->body;

  while (statement != NULL) {

    if (statement->type == AST_RETREAT) {

      ValueType returned = checkExpression(statement->child, program, method);

      ValueType expected = typeFromType(method->returnType);

      if (expected == VALUE_VOID) {
        semanticError("void function cannot retreat a value", method->name);
      }

      if (returned != expected) {
        semanticError("return type mismatch", method->name);
      }
    }

    else if (statement->type == AST_OUT) {

      ValueType type = checkExpression(statement->child, program, method);

      if (type == VALUE_VOID) {
        semanticError("cannot output void", method->name);
      }
    }

    statement = statement->next;
  }
}

// semantic
int varExists(const char *name) {
  for (int i = 0; i < varCount; i++) {
    if (strcmp(variables[i], name) == 0) {
      return 1;
    }
  }

  return 0;
}
void semanticError(const char *message, const char *name) {
  printf("Semantic error: %s '%s'\n", message, name);
  exit(1);
}
void checkProgram(ASTNode *program) {
  ASTNode *node = program->child;

  while (node != NULL) {

    if (node->type == AST_INT_DECL) {

      if (varExists(node->name)) {
        semanticError("variable already exists", node->name);
      }

      strcpy(variables[varCount], node->name);
      variableTypes[varCount] = VALUE_INT;
      varCount++;
    }

    else if (node->type == AST_FP_DECL) {

      if (varExists(node->name)) {
        semanticError("variable already exists", node->name);
      }

      strcpy(variables[varCount], node->name);
      variableTypes[varCount] = VALUE_FP;
      varCount++;
    }

    else if (node->type == AST_STR_DECL) {

      if (varExists(node->name)) {
        semanticError("variable already exists", node->name);
      }

      strcpy(variables[varCount], node->name);
      variableTypes[varCount] = VALUE_STR;
      varCount++;
    }

    else if (node->type == AST_METHOD) {
      checkMethod(program, node);
    }

    else if (node->type == AST_OUT) {

      ValueType type = checkExpression(node->child, program, NULL);

      if (type == VALUE_VOID) {
        semanticError("cannot output void", "");
      }
    }

    node = node->next;
  }
}