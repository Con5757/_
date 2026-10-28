#include "semantic.h"

typedef struct {
  char name[MAX_NAME];
  Type type;
  int flags;
  int arrayCount;
  int isArray;
  int typeSize;
  int scopeDepth;
  int line;
  int column;
} VariableSymbol;

typedef struct {
  char name[MAX_NAME];
  ASTNode *node;
} FunctionSymbol;

static VariableSymbol variables[MAX_AST_NODES];
static int variableCount = 0;
static FunctionSymbol functions[MAX_AST_NODES];
static int functionCount = 0;
static int scopeDepth = 0;
static int loopDepth = 0;
static int switchDepth = 0;
static Method *activeMethod = NULL;
static int scopeStarts[128];

static int isDeclarationType(ASTType type) {
  return type == AST_DECL || type == AST_INT_DECL || type == AST_FP_DECL ||
         type == AST_STR_DECL || type == AST_ARRAY_DECL || type == AST_VEC_DECL;
}

static ValueType valueTypeFromType(Type type) {
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

static const char *valueTypeName(ValueType type) {
  switch (type) {
  case VALUE_INT:
    return "int";
  case VALUE_FP:
    return "fp";
  case VALUE_STR:
    return "str";
  case VALUE_VOID:
    return "void";
  }
  return "unknown";
}

static Type typeFromNode(ASTNode *node) {
  Type type;
  type.kind = TYPE_VOID;
  type.width = 0;
  if (node == NULL)
    return type;
  if (node->declaredType.kind >= TYPE_INT &&
      node->declaredType.kind <= TYPE_VOID) {
    type = node->declaredType;
  } else {
    switch (node->type) {
    case AST_INT_DECL:
      type.kind = TYPE_INT;
      break;
    case AST_FP_DECL:
      type.kind = TYPE_FP;
      break;
    case AST_STR_DECL:
      type.kind = TYPE_STR;
      break;
    default:
      type.kind = TYPE_VOID;
      break;
    }
  }
  return type;
}

static _Noreturn void semanticErrorAt(const char *code, const char *message,
                                      ASTNode *node, const char *label,
                                      const char *help) {
  diagnosticError("error", code, message, node != NULL ? node->line : 0,
                  node != NULL ? node->column : 1, label, help);
}

_Noreturn void semanticError(const char *message, const char *name) {
  char full[384];
  snprintf(full, sizeof(full), "%s `%s`",
           message != NULL ? message : "semantic error",
           name != NULL ? name : "");
  diagnosticError("error", "E3000", full, 0, 1, NULL, NULL);
}

static void enterScope(void) {
  if (scopeDepth + 1 >= (int)(sizeof(scopeStarts) / sizeof(scopeStarts[0]))) {
    diagnosticError("error", "E3090", "maximum lexical scope depth exceeded", 0,
                    1, NULL,
                    "reduce the nesting of blocks, or increase the scope limit "
                    "in semantic.c.");
  }
  scopeStarts[scopeDepth] = variableCount;
  scopeDepth++;
}

static void leaveScope(void) {
  if (scopeDepth > 0) {
    variableCount = scopeStarts[scopeDepth - 1];
    scopeDepth--;
  }
}

static VariableSymbol *findVariable(const char *name) {
  for (int i = variableCount - 1; i >= 0; --i) {
    if (strcmp(variables[i].name, name) == 0)
      return &variables[i];
  }
  return NULL;
}

int varExists(const char *name) {
  return name != NULL && findVariable(name) != NULL;
}

static VariableSymbol *findVariableAtCurrentScope(const char *name) {
  for (int i = variableCount - 1; i >= 0; --i) {
    if (variables[i].scopeDepth < scopeDepth)
      break;
    if (strcmp(variables[i].name, name) == 0)
      return &variables[i];
  }
  return NULL;
}

static VariableSymbol *addVariable(ASTNode *node, Type type, int flags,
                                   int arrayCount, int isArray) {
  if (node == NULL || node->name[0] == '\0')
    return NULL;
  VariableSymbol *conflict = findVariableAtCurrentScope(node->name);
  if (conflict != NULL) {
    char message[320];
    snprintf(message, sizeof(message),
             "the name `%s` is already defined in this scope", node->name);
    diagnosticErrorWithNote(
        "error", "E3001", message, node->line, node->column,
        "this declaration conflicts with an earlier declaration",
        "choose a different name, or remove the duplicate declaration.",
        "the previous declaration is here", conflict->line, conflict->column);
  }
  if (variableCount >= MAX_AST_NODES) {
    semanticErrorAt(
        "E3091", "too many variable declarations", node, NULL,
        "increase MAX_AST_NODES or split this source into smaller modules.");
  }
  VariableSymbol *symbol = &variables[variableCount++];
  memset(symbol, 0, sizeof(*symbol));
  snprintf(symbol->name, sizeof(symbol->name), "%s", node->name);
  symbol->type = type;
  symbol->flags = flags;
  symbol->arrayCount = arrayCount;
  symbol->isArray = isArray;
  symbol->typeSize = node->typeSize;
  symbol->scopeDepth = scopeDepth;
  symbol->line = node->line;
  symbol->column = node->column;
  return symbol;
}

static ASTNode *findFunction(const char *name) {
  for (int i = 0; i < functionCount; ++i) {
    if (strcmp(functions[i].name, name) == 0)
      return functions[i].node;
  }
  return NULL;
}

static void registerFunction(ASTNode *node) {
  if (node == NULL || node->type != AST_METHOD || node->method == NULL)
    return;
  Method *method = node->method;
  ASTNode *previous = findFunction(method->name);
  if (previous != NULL) {
    char message[320];
    snprintf(message, sizeof(message),
             "function `%s` is defined more than once", method->name);
    diagnosticErrorWithNote("error", "E3002", message, node->line, node->column,
                            "duplicate function definition",
                            "keep one definition of this function, or give the "
                            "other function a different name.",
                            "the previous function definition is here",
                            previous->line, previous->column);
  }
  if (functionCount >= MAX_AST_NODES) {
    semanticErrorAt(
        "E3092", "too many function definitions", node, NULL,
        "increase MAX_AST_NODES or split the source into smaller modules.");
  }
  snprintf(functions[functionCount].name, sizeof(functions[functionCount].name),
           "%s", method->name);
  functions[functionCount].node = node;
  functionCount++;
}

static int isNumeric(ValueType type) {
  return type == VALUE_INT || type == VALUE_FP;
}

static int pointerDepthFromFlags(int flags) {
  if ((flags & DECL_FLAG_DOUBLE_POINTER) != 0)
    return 2;
  if ((flags & DECL_FLAG_POINTER) != 0)
    return 1;
  return 0;
}

static int flagsForPointerDepth(int depth) {
  if (depth >= 2)
    return DECL_FLAG_DOUBLE_POINTER;
  if (depth == 1)
    return DECL_FLAG_POINTER;
  return 0;
}

static int isConstLValue(ASTNode *node) {
  if (node == NULL)
    return 0;

  int flags = node->storageFlags;
  if (node->type == AST_VAR) {
    VariableSymbol *symbol = findVariable(node->name);
    if (symbol != NULL)
      flags = symbol->flags;
  } else if (node->type != AST_INDEX && node->type != AST_UNARY) {
    return 0;
  }

  /*
   * `const int ->ptr` means “pointer to const int”, not “const pointer”.
   * Therefore the pointer variable itself remains assignable, while its
   * dereferenced scalar (and a const array element) is not. `storageFlags`
   * on an expression describes the type after indexing/dereferencing.
   */
  if ((flags & DECL_FLAG_CONST) == 0 || pointerDepthFromFlags(flags) != 0) {
    return 0;
  }
  if (node->type == AST_UNARY && node->operatorType != TOKEN_DEREF &&
      node->operatorType != TOKEN_DDEREF) {
    return 0;
  }
  return 1;
}

/*
 * DECL_FLAG_CONST qualifies the pointed-to base type for pointers. Dropping
 * that qualifier is unsafe. Adding it is safe for one pointer level, but not
 * for double pointers (the C-style `int **` -> `const int **` conversion).
 */
static int pointerQualificationCompatible(int targetFlags, int sourceFlags) {
  int targetDepth = pointerDepthFromFlags(targetFlags);
  int sourceDepth = pointerDepthFromFlags(sourceFlags);
  if (targetDepth == 0 || targetDepth != sourceDepth)
    return 1;

  int targetConst = (targetFlags & DECL_FLAG_CONST) != 0;
  int sourceConst = (sourceFlags & DECL_FLAG_CONST) != 0;
  if (sourceConst && !targetConst)
    return 0;
  if (!sourceConst && targetConst && targetDepth > 1)
    return 0;
  return 1;
}

static void checkPointerQualification(int targetFlags, int sourceFlags,
                                      ASTNode *where, const char *context) {
  if (pointerQualificationCompatible(targetFlags, sourceFlags))
    return;

  int targetConst = (targetFlags & DECL_FLAG_CONST) != 0;
  int sourceConst = (sourceFlags & DECL_FLAG_CONST) != 0;
  char message[320];
  if (sourceConst && !targetConst) {
    snprintf(message, sizeof(message),
             "%s would discard the `const` qualifier from the pointed-to type",
             context != NULL ? context : "this pointer conversion");
    semanticErrorAt("E3068", message, where,
                    "the source pointer refers to a const-qualified value",
                    "keep the target pointer const-qualified, or only use a "
                    "mutable pointer with a mutable value.");
  }

  snprintf(message, sizeof(message),
           "%s cannot add `const` through a double pointer safely",
           context != NULL ? context : "this pointer conversion");
  semanticErrorAt("E3068", message, where,
                  "converting `int **` to `const int **` can allow a mutable "
                  "pointer to escape through the const-qualified type",
                  "use a matching pointer qualification at both levels, or "
                  "pass a single pointer to a const-qualified value.");
}

static int typesCompatible(ValueType expected, ValueType actual) {
  if (expected == actual)
    return 1;
  /* Numeric widening from int to fp is safe and convenient. */
  return expected == VALUE_FP && actual == VALUE_INT;
}

static int constantIntegerValue(ASTNode *node, long long *value) {
  if (node == NULL || value == NULL)
    return 0;
  if (node->type == AST_NUM && node->valueType == VALUE_INT) {
    *value = node->value;
    return 1;
  }
  if (node->type == AST_UNARY && node->operatorType == TOKEN_SUB &&
      node->child != NULL && node->child->type == AST_NUM &&
      node->child->valueType == VALUE_INT && node->child->value != LLONG_MIN) {
    *value = -node->child->value;
    return 1;
  }
  return 0;
}

static int stringLiteralLength(const char *text) {
  int length = 0;
  if (text == NULL)
    return 0;
  for (size_t i = 0; text[i] != '\0'; ++i) {
    if (text[i] == '\\' && text[i + 1] != '\0')
      ++i;
    ++length;
  }
  return length;
}

static int constantNumericZero(ASTNode *node) {
  if (node == NULL)
    return 0;
  if (node->type == AST_NUM) {
    return node->valueType == VALUE_FP ? node->fpValue == 0.0
                                       : node->value == 0;
  }
  if (node->type == AST_BOOL)
    return node->value == 0;
  if (node->type == AST_UNARY && node->operatorType == TOKEN_SUB) {
    return constantNumericZero(node->child);
  }
  return 0;
}

/* Reject constant values that do not fit the language's signed fixed-width
 * types. */
static void checkIntegerRange(Type target, ASTNode *expression) {
  if (target.kind != TYPE_INT || target.width <= 0 || target.width >= 64 ||
      expression == NULL)
    return;

  long long value;
  if (!constantIntegerValue(expression, &value))
    return;

  long long minimum = -(1LL << (target.width - 1));
  long long maximum = (1LL << (target.width - 1)) - 1;
  if (value < minimum || value > maximum) {
    char message[320];
    snprintf(message, sizeof(message),
             "integer literal %lld does not fit in `int%d`", value,
             target.width);
    const char *help =
        target.width == 8 ? "use a value from -128 through 127, or choose a "
                            "wider integer type."
        : target.width == 16
            ? "use a value from -32768 through 32767, or choose a wider "
              "integer type."
            : "use a value within the signed 32-bit range, or choose `int64`.";
    semanticErrorAt(
        "E3048", message, expression,
        "this value is outside the target type's representable range", help);
  }
}

static void checkIndexBounds(ASTNode *indexNode, long long index,
                             int capacity) {
  if (capacity <= 0)
    return;
  if (index < 0 || index >= capacity) {
    char message[320];
    snprintf(message, sizeof(message),
             "index %lld is outside the valid range 0..%d", index,
             capacity - 1);
    semanticErrorAt(
        "E3073", message, indexNode,
        "this constant index is outside the known array or string bounds",
        "use an index within the declared capacity, or increase the capacity "
        "of the array/string.");
  }
}

static ValueType checkExpression(ASTNode *expr);
static void checkStatement(ASTNode *node);
static void checkStatementList(ASTNode *node);

static ValueType checkVariable(ASTNode *node) {
  VariableSymbol *symbol = findVariable(node->name);
  if (symbol == NULL && activeMethod != NULL) {
    for (int i = 0; i < activeMethod->paramCount; ++i) {
      if (strcmp(activeMethod->params[i].name, node->name) == 0) {
        ValueType type = valueTypeFromType(activeMethod->params[i].type);
        node->valueType = type;
        node->storageFlags = activeMethod->params[i].storageFlags;
        node->declaredType = activeMethod->params[i].type;
        return type;
      }
    }
  }
  if (symbol == NULL) {
    char message[320];
    snprintf(message, sizeof(message), "cannot find value `%s` in this scope",
             node->name);
    semanticErrorAt("E3003", message, node, "this name has not been declared",
                    "declare the variable before using it, or check the "
                    "spelling and capitalization.");
  }
  node->valueType = valueTypeFromType(symbol->type);
  node->declaredType = symbol->type;
  node->storageFlags = symbol->flags;
  node->typeSize = symbol->typeSize;
  return node->valueType;
}

static int isLValue(ASTNode *node) {
  return node != NULL &&
         (node->type == AST_VAR || node->type == AST_INDEX ||
          node->type == AST_MEMBER ||
          (node->type == AST_UNARY && (node->operatorType == TOKEN_DEREF ||
                                       node->operatorType == TOKEN_DDEREF)));
}

static ValueType checkExpression(ASTNode *expr) {
  if (expr == NULL)
    return VALUE_VOID;
  ValueType left, right;
  switch (expr->type) {
  case AST_NUM:
    return expr->valueType;
  case AST_STRING:
    return VALUE_STR;
  case AST_BOOL:
    return VALUE_INT;
  case AST_VAR:
    return checkVariable(expr);
  case AST_MEMBER:
  case AST_NAMESPACE_ACCESS: {
    /* Access chains are valid only as part of a recognized callable for now. */
    char message[320];
    snprintf(message, sizeof(message),
             "member access on this value is not supported yet");
    semanticErrorAt("E3010", message, expr,
                    "this member expression is not a standalone value",
                    "for standard output, use `std.out(value)`; general "
                    "struct/class member access needs type definitions first.");
    return VALUE_VOID;
  }
  case AST_INDEX: {
    right = checkExpression(expr->right);
    if (right != VALUE_INT) {
      semanticErrorAt("E3011", "array index must be an integer expression",
                      expr->right, "this expression is used as an index",
                      "use an integer index, for example `numbers[0]`; "
                      "floating-point indexes are not allowed.");
    }
    

    long long constantIndex = 0;
    int hasConstantIndex = constantIntegerValue(expr->right, &constantIndex);
    ASTNode *base = expr->child;
    int isCharacterIndex = 0;
    int remainingPointerDepth = 0;
    int elementStringCapacity = 0;
    Type elementType;
    elementType.kind = TYPE_VOID;
    elementType.width = 0;

    if (base != NULL && base->type == AST_VAR) {
      left = checkVariable(base);
      VariableSymbol *symbol = findVariable(base->name);
      if (symbol != NULL) {
        remainingPointerDepth = pointerDepthFromFlags(symbol->flags);
        if (!symbol->isArray && symbol->type.kind != TYPE_STR &&
            remainingPointerDepth == 0) {
          char message[320];
          snprintf(message, sizeof(message), "`%s` is not an array or pointer",
                   base->name);
          semanticErrorAt("E3012", message, expr,
                          "indexing requires an indexable value",
                          "declare the value with an array size such as `int "
                          "values<3> = [1, 2, 3]`.");
        }
        elementType = symbol->type;
        if (symbol->isArray) {
          if (hasConstantIndex)
            checkIndexBounds(expr->right, constantIndex, symbol->arrayCount);
          if (symbol->type.kind == TYPE_STR && symbol->typeSize > 0) {
            elementStringCapacity = symbol->typeSize;
          }
        } else if (symbol->type.kind == TYPE_STR &&
                   remainingPointerDepth == 0) {
          isCharacterIndex = 1;
          elementType.kind = TYPE_INT;
          elementType.width = 8;
          if (hasConstantIndex)
            checkIndexBounds(expr->right, constantIndex, symbol->typeSize);
        } else if (remainingPointerDepth > 0) {
          remainingPointerDepth--;
        }
      } else if (activeMethod != NULL) {
        Param *parameter = NULL;
        for (int i = 0; i < activeMethod->paramCount; ++i) {
          if (strcmp(activeMethod->params[i].name, base->name) == 0) {
            parameter = &activeMethod->params[i];
            break;
          }
        }
        if (parameter == NULL) {
          /* checkVariable normally catches this, but keep the AST invariant
           * explicit. */
          semanticErrorAt(
              "E3003", "cannot resolve this indexed value", base, NULL,
              "declare the variable or check the function parameter name.");
        }
        elementType = parameter->type;
        remainingPointerDepth = pointerDepthFromFlags(parameter->storageFlags);
        if (elementType.kind == TYPE_STR && remainingPointerDepth == 0) {
          isCharacterIndex = 1;
          elementType.kind = TYPE_INT;
          elementType.width = 8;
        } else if (remainingPointerDepth > 0) {
          remainingPointerDepth--;
        } else {
          semanticErrorAt(
              "E3012", "this parameter is not indexable", expr,
              "the parameter is neither an array-compatible pointer nor a "
              "string",
              "pass a pointer parameter or use a string/array value.");
        }
      }
    } else if (base != NULL && base->type == AST_STRING) {
      (void)checkExpression(base);
      isCharacterIndex = 1;
      elementType.kind = TYPE_INT;
      elementType.width = 8;
      if (hasConstantIndex)
        checkIndexBounds(expr->right, constantIndex,
                         stringLiteralLength(base->strValue));
    } else {
      left = checkExpression(base);
      int basePointerDepth =
          pointerDepthFromFlags(base != NULL ? base->storageFlags : 0);
      if (left == VALUE_STR && base != NULL &&
          base->declaredType.kind == TYPE_STR && basePointerDepth == 0) {
        /* A row from `str rows[N]<M>` is itself a fixed-capacity string. */
        isCharacterIndex = 1;
        elementType.kind = TYPE_INT;
        elementType.width = 8;
        if (hasConstantIndex && base->typeSize > 0) {
          checkIndexBounds(expr->right, constantIndex, base->typeSize);
        }
      } else {
        if (basePointerDepth == 0) {
          semanticErrorAt("E3012", "this expression is not indexable", expr,
                          "indexing requires an array, string, or pointer",
                          "index a declared array, for example `values[0]`.");
        }
        elementType = base->declaredType;
        remainingPointerDepth = basePointerDepth - 1;
        if (remainingPointerDepth < 0)
          remainingPointerDepth = 0;
      }
    }

    expr->valueType = isCharacterIndex ? VALUE_INT : base->valueType;
    expr->declaredType = elementType;
    expr->typeSize = elementStringCapacity;
    int baseConst = base != NULL ? base->storageFlags & DECL_FLAG_CONST : 0;
    if (base != NULL && base->type == AST_STRING) {
      /* String literals are immutable even though C's type system is
       * permissive. */
      baseConst = DECL_FLAG_CONST;
    }
    expr->storageFlags =
        flagsForPointerDepth(remainingPointerDepth) | baseConst;
    if (!isCharacterIndex && elementType.kind == TYPE_STR &&
        elementStringCapacity > 0) {
      expr->storageFlags |= DECL_FLAG_STRING_BUFFER;
    }
    return expr->valueType;
  }
  case AST_UNARY:
    left = checkExpression(expr->child);
    if (expr->operatorType == NOT) {
      if (!isNumeric(left))
        semanticErrorAt("E3013",
                        "logical not requires a numeric/boolean expression",
                        expr, NULL,
                        "use `!condition` where `condition` is an integer-like "
                        "truth value.");
      expr->valueType = VALUE_INT;
      return VALUE_INT;
    }
    if (expr->operatorType == TOKEN_SUB) {
      if (!isNumeric(left))
        semanticErrorAt("E3014", "unary minus requires a numeric expression",
                        expr, NULL, "apply `-` to an `int` or `fp` value.");
      expr->valueType = left;
      return left;
    }
    if (expr->operatorType == XOR) {
      if (left != VALUE_INT)
        semanticErrorAt("E3014",
                        "bitwise complement requires an integer expression",
                        expr, NULL,
                        "apply unary `~` to an `int`; floating-point values do "
                        "not support bitwise complement.");
      expr->valueType = VALUE_INT;
      return VALUE_INT;
    }
    if (expr->operatorType == TOKEN_AT) {
      if (!isLValue(expr->child)) {
        semanticErrorAt("E3050", "cannot take the address of this expression",
                        expr,
                        "address-of requires a variable, array element, "
                        "member, or dereferenced pointer",
                        "use `@name` with a declared variable, for example "
                        "`int ->ptr = @value`.");
      }
      int depth = pointerDepthFromFlags(expr->child->storageFlags) + 1;
      if (depth > 2)
        semanticErrorAt(
            "E3051", "pointer depth greater than two is not supported", expr,
            NULL,
            "the documented language currently defines `->` and `-->` only.");
      expr->storageFlags = flagsForPointerDepth(depth) |
                           (expr->child->storageFlags & DECL_FLAG_CONST);
      expr->valueType = left;
      return left;
    }
    if (expr->operatorType == TOKEN_DEREF ||
        expr->operatorType == TOKEN_DDEREF) {
      int required = expr->operatorType == TOKEN_DDEREF ? 2 : 1;
      int depth = pointerDepthFromFlags(expr->child->storageFlags);
      if (depth < required) {
        semanticErrorAt("E3052",
                        "cannot dereference a value that is not a pointer",
                        expr, "the operand has too few pointer levels",
                        required == 2 ? "use `<--value` only with a double "
                                        "pointer declared using `-->`."
                                      : "declare a pointer with `->`, or take "
                                        "a variable's address using `@name`.");
      }
      expr->storageFlags = flagsForPointerDepth(depth - required) |
                           (expr->child->storageFlags & DECL_FLAG_CONST);
      expr->valueType = left;
      return left;
    }
    if (expr->operatorType == TOKEN_PTR || expr->operatorType == TOKEN_DPTR) {
      semanticErrorAt(
          "E3053", "pointer declaration marker cannot be used as an expression",
          expr,
          "`->` and `-->` belong next to a variable name in a declaration",
          "use `@value` to take an address, `<-value` to dereference a "
          "pointer, and `<--value` for a double pointer.");
    }
    expr->valueType = left;
    return left;
  case AST_ASSIGN:
    if (!isLValue(expr->child)) {
      semanticErrorAt(
          "E3015", "invalid assignment target", expr->child,
          "only a variable, indexed element, member, or dereferenced pointer "
          "can be assigned to",
          "write an assignment like `total = total + 1`, not `1 + total = 2`.");
    }
    left = checkExpression(expr->child);
    if (isConstLValue(expr->child)) {
      semanticErrorAt("E3054", "cannot assign to a constant", expr->child,
                      "this value is const-qualified; for pointers, this is "
                      "the pointed-to value",
                      "keep the pointed-to value unchanged, or use a mutable "
                      "pointer only with a mutable value.");
    }
    right = checkExpression(expr->right);
    int isNullPointerAssignment = expr->right->type == AST_NUM &&
                                  expr->right->valueType == VALUE_INT &&
                                  expr->right->value == 0;
    if (pointerDepthFromFlags(expr->child->storageFlags) !=
            pointerDepthFromFlags(expr->right->storageFlags) &&
        !(pointerDepthFromFlags(expr->child->storageFlags) > 0 &&
          isNullPointerAssignment)) {
      semanticErrorAt(
          "E3055", "assignment has incompatible pointer levels", expr,
          "the left and right sides have different pointer depths",
          "use `@value` to obtain a pointer, `<-value` to dereference one, or "
          "assign compatible pointer variables.");
    }
    if (pointerDepthFromFlags(expr->child->storageFlags) ==
        pointerDepthFromFlags(expr->right->storageFlags)) {
      checkPointerQualification(expr->child->storageFlags,
                                expr->right->storageFlags, expr->right,
                                "this pointer assignment");
    }
    if (left == VALUE_STR && expr->child != NULL &&
        (expr->child->type == AST_VAR || expr->child->type == AST_INDEX) &&
        (expr->child->storageFlags & DECL_FLAG_STRING_BUFFER) != 0 &&
        expr->right != NULL && expr->right->type == AST_STRING &&
        expr->child->typeSize > 0 &&
        stringLiteralLength(expr->right->strValue) > expr->child->typeSize) {
      semanticErrorAt("E3035",
                      "string literal is larger than the declared buffer",
                      expr->right, "this assignment would truncate the string",
                      "increase the declared capacity, or use a shorter string "
                      "literal. For example, `str name[12] = \"hello\"` "
                      "reserves room for up to 12 bytes.");
    }
    if (!typesCompatible(left, right)) {
      char message[320];
      snprintf(message, sizeof(message),
               "cannot assign `%s` to a value of type `%s`",
               valueTypeName(right), valueTypeName(left));
      semanticErrorAt("E3016", message, expr,
                      "the types on the two sides do not match",
                      "convert the value explicitly, or use an expression with "
                      "a compatible type.");
    }
    checkIntegerRange(expr->child->declaredType, expr->right);
    if (expr->operatorType != TOKEN_EQUAL &&
        (!isNumeric(left) || !isNumeric(right))) {
      semanticErrorAt("E3017", "compound assignment requires numeric operands",
                      expr,
                      "this compound operator is not defined for these types",
                      "use `=` for compatible values, or use a compound "
                      "operator on numeric variables.");
    }
    expr->valueType = left;
    expr->storageFlags = expr->child != NULL ? expr->child->storageFlags : 0;
    return left;
  case AST_UPDATE:
    if (!isLValue(expr->child)) {
      semanticErrorAt(
          "E3018", "invalid update target", expr->child,
          "increment and decrement require a modifiable value",
          "use `count++` or `count--` on a declared numeric variable.");
    }
    left = checkExpression(expr->child);
    if (isConstLValue(expr->child)) {
      semanticErrorAt("E3054", "cannot modify a constant", expr->child,
                      "this value is const-qualified; for pointers, this is "
                      "the pointed-to value",
                      "keep the pointed-to value unchanged, or use a mutable "
                      "pointer only with a mutable value.");
    }
    if (!isNumeric(left))
      semanticErrorAt("E3019", "increment/decrement requires a numeric value",
                      expr, NULL, "use `++` or `--` with `int` or `fp`.");
    expr->valueType = left;
    expr->storageFlags = expr->child != NULL ? expr->child->storageFlags : 0;
    return left;
  case AST_CALL: {
    /*
     * Native IO operations are compiler runtime operations. They must be
     * recognized before findFunction(), so they work without an explicit
     * `?std with io` import.
     */
    if (strcmp(expr->name, "io.in") == 0) {
      if (expr->child != NULL) {
        semanticErrorAt("E3020", "io.in expects no arguments", expr, NULL,
                        "call `io.in()` without arguments.");
      }

      expr->valueType = VALUE_STR;
      return VALUE_STR;
    }

    if (strcmp(expr->name, "io.out") == 0) {
      if (expr->child == NULL || expr->child->next != NULL) {
        semanticErrorAt("E3020", "io.out expects exactly one argument", expr,
                        NULL, "call `io.out(value)` with one value.");
      }

      ValueType outputType = checkExpression(expr->child);

      if (outputType == VALUE_VOID) {
        semanticErrorAt("E3021", "cannot output a value of type `void`", expr,
                        NULL, "pass an expression that produces a value.");
      }

      expr->valueType = VALUE_VOID;
      return VALUE_VOID;
    }

    /* `io.in()` is emitted as the native `uc_io_in()` runtime helper by the
     * C backend. Give it a string result here as well, so semantic analysis
     * agrees with the code generator instead of reporting an unknown symbol. */
    if (strcmp(expr->name, "io.in") == 0) {
      if (expr->child != NULL) {
        semanticErrorAt("E3079", "io.in expects no arguments", expr, NULL,
                        "call `io.in()` without parameters.");
      }
      expr->valueType = VALUE_STR;
      return VALUE_STR;
    }

    /*
     * Preserve the existing unsupported-input behavior for legacy names.
     */
    if (strcmp(expr->name, "std.in") == 0 ||
        strcmp(expr->name, "std::in") == 0 || strcmp(expr->name, "in") == 0) {
      semanticErrorAt(
          "E3060", "input function is not implemented in the current runtime",
          expr,
          "`in` is tokenized, but an input API has not been specified yet",
          "use a function with a defined input implementation, or add the "
          "`std.in` runtime contract before calling it.");
    }

    /*
     * Keep existing out / std.out behavior unchanged.
     */
    if (strcmp(expr->name, "std.out") == 0 ||
        strcmp(expr->name, "std::out") == 0 || strcmp(expr->name, "out") == 0) {
      if (expr->child == NULL)
        semanticErrorAt("E3020", "`std.out` expects one argument", expr, NULL,
                        "for example, write `std.out(42)`.");

      if (expr->child->next != NULL)
        semanticErrorAt("E3020", "`std.out` expects exactly one argument", expr,
                        NULL,
                        "combine values into one expression, or call `std.out` "
                        "more than once.");

      left = checkExpression(expr->child);

      if (left == VALUE_VOID)
        semanticErrorAt("E3021", "cannot output a value of type `void`", expr,
                        NULL,
                        "output a value-producing expression instead of a void "
                        "function call.");

      expr->valueType = VALUE_VOID;
      return VALUE_VOID;
    }

    ASTNode *functionNode = findFunction(expr->name);

    if (functionNode == NULL) {
      char message[320];
      snprintf(message, sizeof(message), "cannot find function `%s`",
               expr->name);

      semanticErrorAt("E3022", message, expr,
                      "no function with this name is declared",
                      "check the function name, or declare it with a signature "
                      "such as `int add(int a, int b) { ... }.");
    }

    Method *called = functionNode->method;
    int arguments = 0;

    for (ASTNode *arg = expr->child; arg != NULL; arg = arg->next)
      arguments++;

    if (arguments != called->paramCount) {
      char message[320];

      snprintf(message, sizeof(message),
               "function `%s` takes %d argument%s but %d %s supplied",
               expr->name, called->paramCount,
               called->paramCount == 1 ? "" : "s", arguments,
               arguments == 1 ? "was" : "were");

      semanticErrorAt("E3023", message, expr,
                      "the call does not match the function signature",
                      "pass exactly the number of arguments listed in the "
                      "function definition.");
    }

    ASTNode *arg = expr->child;

    for (int i = 0; i < called->paramCount; ++i, arg = arg->next) {
      ValueType actual = checkExpression(arg);
      ValueType expected = valueTypeFromType(called->params[i].type);

      int isNullPointerArgument = arg->type == AST_NUM &&
                                  arg->valueType == VALUE_INT &&
                                  arg->value == 0;

      if (pointerDepthFromFlags(arg->storageFlags) !=
              pointerDepthFromFlags(called->params[i].storageFlags) &&
          !(pointerDepthFromFlags(called->params[i].storageFlags) > 0 &&
            isNullPointerArgument)) {
        char message[320];

        snprintf(message, sizeof(message),
                 "argument %d of `%s` has the wrong pointer depth", i + 1,
                 expr->name);

        semanticErrorAt(
            "E3056", message, arg,
            "the argument's pointer level does not match the parameter",
            "pass a variable of the expected pointer type, or use `@variable` "
            "to take its address.");
      }

      if (pointerDepthFromFlags(arg->storageFlags) ==
          pointerDepthFromFlags(called->params[i].storageFlags)) {
        char context[160];

        snprintf(context, sizeof(context), "argument %d to `%s`", i + 1,
                 expr->name);

        checkPointerQualification(called->params[i].storageFlags,
                                  arg->storageFlags, arg, context);
      }

      if (!typesCompatible(expected, actual)) {
        char message[320];

        snprintf(message, sizeof(message),
                 "argument %d of `%s` expects `%s`, but got `%s`", i + 1,
                 expr->name, valueTypeName(expected), valueTypeName(actual));

        semanticErrorAt("E3024", message, arg,
                        "this argument has the wrong type",
                        "pass a value matching the parameter type shown in the "
                        "function declaration.");
      }

      checkIntegerRange(called->params[i].type, arg);
    }

    expr->valueType = valueTypeFromType(called->returnType);
    return expr->valueType;
  }
  case AST_SPECIAL_BITWISE:
    semanticErrorAt(
        "E3061", "operator `~?` has no specified semantics", expr,
        "the syntax list names this operator but does not define its behavior",
        "define whether `~?` is bitwise, logical, or otherwise before using it "
        "in a program.");
    return VALUE_VOID;
  case AST_ADD:
  case AST_SUB:
  case AST_MUL:
  case AST_DIV:
  case AST_MOD:
  case AST_SHIFT_LEFT:
  case AST_SHIFT_RIGHT:
  case AST_BIT_OR:
  case AST_BIT_AND:
  case AST_BIT_XOR:
    left = checkExpression(expr->child);
    right = checkExpression(expr->right);
    if (!isNumeric(left) || !isNumeric(right)) {
      semanticErrorAt("E3025", "operator requires numeric operands", expr,
                      "one or both operands are not numeric",
                      "use `int` or `fp` values with arithmetic operators, and "
                      "integer values with bitwise operators.");
    }
    if ((expr->type == AST_MOD || expr->type == AST_SHIFT_LEFT ||
         expr->type == AST_SHIFT_RIGHT || expr->type == AST_BIT_OR ||
         expr->type == AST_BIT_AND || expr->type == AST_BIT_XOR) &&
        (left != VALUE_INT || right != VALUE_INT)) {
      semanticErrorAt(
          "E3026", "this operator requires integer operands", expr,
          "bitwise, shift, and modulo operations use integers",
          "convert both operands to `int` before applying this operator.");
    }
    if ((expr->type == AST_DIV || expr->type == AST_MOD) &&
        constantNumericZero(expr->right)) {
      semanticErrorAt(
          "E3049",
          expr->type == AST_MOD ? "remainder by zero is not allowed"
                                : "division by zero is not allowed",
          expr->right, "this divisor is known to be zero at compile time",
          "use a nonzero divisor. If the value comes from a variable, check "
          "that it is not zero before performing the operation.");
    }
    expr->valueType =
        (left == VALUE_FP || right == VALUE_FP) ? VALUE_FP : VALUE_INT;
    return expr->valueType;
  case AST_LOGICAL_NOT:
  case AST_LOGICAL_AND:
  case AST_LOGICAL_OR:
  case AST_LOGICAL_NOR:
  case AST_LOGICAL_XOR:
  case AST_LOGICAL_XNOR:
  case AST_LOGICAL_NAND:
    left = checkExpression(expr->child);
    if (!isNumeric(left))
      semanticErrorAt("E3027",
                      "logical operator requires numeric/boolean operands",
                      expr->child, NULL,
                      "use an integer-like expression as the condition.");
    if (expr->type != AST_LOGICAL_NOT) {
      right = checkExpression(expr->right);
      if (!isNumeric(right))
        semanticErrorAt("E3027",
                        "logical operator requires numeric/boolean operands",
                        expr->right, NULL,
                        "use an integer-like expression as the condition.");
    }
    expr->valueType = VALUE_INT;
    return VALUE_INT;
  case AST_EQUAL:
  case AST_NOT_EQUAL:
  case AST_LESS:
  case AST_GREATER:
  case AST_LESS_EQUAL:
  case AST_GREATER_EQUAL:
    left = checkExpression(expr->child);
    right = checkExpression(expr->right);
    if (left != right && !(isNumeric(left) && isNumeric(right))) {
      semanticErrorAt("E3028", "cannot compare values of different types", expr,
                      "these two values do not have compatible types",
                      "compare values of the same type, or compare numeric "
                      "values with each other.");
    }
    expr->valueType = VALUE_INT;
    return VALUE_INT;
  case AST_ARRAY_DECL:
    for (ASTNode *item = expr->child; item != NULL; item = item->next)
      (void)checkExpression(item);
    return VALUE_VOID;
  default:
    semanticErrorAt("E3029", "this AST node cannot be used as an expression",
                    expr, NULL,
                    "check that the expression is complete and that this "
                    "statement is in the correct context.");
    return VALUE_VOID;
  }
}

static void checkDeclaration(ASTNode *node, int isGlobal) {
  if (node != NULL && node->type == AST_VEC_DECL) {
    semanticErrorAt(
        "E3062", "dynamic vectors are not implemented yet", node,
        "`vec` is recognized, but the runtime has no vector representation",
        "use a fixed-size array such as `int values<3> = [1, 2, 3]` until "
        "vector allocation and resizing are implemented.");
  }
  Type type = typeFromNode(node);
  int isArray = node->type == AST_ARRAY_DECL || node->type == AST_VEC_DECL;
  int arrayCount = node->arrayCount;
  if (node->type == AST_ARRAY_DECL && arrayCount <= 0) {
    semanticErrorAt(
        "E3075", "array capacity must be greater than zero", node, NULL,
        "declare at least one element, or provide a non-empty initializer.");
  }
  if (!isGlobal)
    addVariable(node, type, node->storageFlags, arrayCount, isArray);

  if (type.kind == TYPE_VOID && node->type != AST_VEC_DECL) {
    semanticErrorAt("E3030", "a variable cannot have type `void`", node,
                    "`void` is reserved for functions that return no value",
                    "choose `int`, `fp`, or `str` for the variable's type.");
  }

  /* Evaluate semantic metadata before checking pointer depth or type
   * compatibility. */
  if (node->child != NULL && node->type != AST_ARRAY_DECL &&
      node->type != AST_VEC_DECL) {
    (void)checkExpression(node->child);
  }

  if ((node->storageFlags & DECL_FLAG_CONST) != 0 && node->child == NULL &&
      node->type != AST_VEC_DECL) {
    semanticErrorAt("E3031", "constant declaration requires an initializer",
                    node, "this constant has no initial value",
                    "initialize it, for example `const int limit = 10`.");
  }
  if (activeMethod != NULL && (node->storageFlags & DECL_FLAG_GLOBAL) != 0) {
    semanticErrorAt(
        "E3057",
        "`global` declarations inside functions are not implemented yet", node,
        "the compiler currently supports global declarations at file scope",
        "move this declaration to the top level, or remove `global` to keep it "
        "local to the function.");
  }
  if (pointerDepthFromFlags(node->storageFlags) > 0 && node->child != NULL &&
      node->type != AST_ARRAY_DECL) {
    int expectedDepth = pointerDepthFromFlags(node->storageFlags);
    int actualDepth = pointerDepthFromFlags(node->child->storageFlags);
    int nullInteger = node->child->type == AST_NUM &&
                      node->child->valueType == VALUE_INT &&
                      node->child->value == 0;
    if (actualDepth != expectedDepth && !nullInteger) {
      semanticErrorAt(
          "E3058",
          "pointer initializer does not have the declared pointer depth",
          node->child, "the initializer is not a pointer of the expected level",
          "take an address with `@variable`; for example, use `int value = 20` "
          "followed by `int ->ptr = @value`.");
    }
    if (actualDepth == expectedDepth) {
      checkPointerQualification(node->storageFlags, node->child->storageFlags,
                                node->child, "this pointer initializer");
    }
  } else if (node->child != NULL && node->type != AST_ARRAY_DECL &&
             pointerDepthFromFlags(node->child->storageFlags) > 0) {
    semanticErrorAt(
        "E3059", "cannot initialize a non-pointer variable with a pointer",
        node->child,
        "the initializer has pointer type, but the declaration does not",
        "declare the variable using `->` or `-->`, or dereference the pointer "
        "before assigning it.");
  }

  if (node->type == AST_ARRAY_DECL) {
    int items = 0;
    for (ASTNode *item = node->child; item != NULL; item = item->next) {
      ValueType actual = checkExpression(item);
      ValueType expected = valueTypeFromType(type);
      if (actual == VALUE_VOID || !typesCompatible(expected, actual)) {
        char message[320];
        snprintf(message, sizeof(message),
                 "array element has type `%s`, but `%s` was expected",
                 valueTypeName(actual), valueTypeName(expected));
        semanticErrorAt("E3032", message, item,
                        "this element does not match the array type",
                        "use elements with the declared type, for example `int "
                        "values<2> = [1, 2]`.");
      }
      checkIntegerRange(type, item);
      if (type.kind == TYPE_STR && node->typeSize > 0 &&
          item->type == AST_STRING &&
          stringLiteralLength(item->strValue) > node->typeSize) {
        char message[320];
        snprintf(
            message, sizeof(message),
            "string array element needs %d bytes, but each row has capacity %d",
            stringLiteralLength(item->strValue), node->typeSize);
        semanticErrorAt(
            "E3035", message, item,
            "this string will not fit in the declared row buffer",
            "increase the `[N]` string capacity or shorten this element; for "
            "example, `str words[8]<2> = [\"Hello\", \"Goodbye\"]`.");
      }
      items++;
      if (node->arrayCount > 0 && items > node->arrayCount) {
        char message[320];
        snprintf(message, sizeof(message),
                 "array has more than %d initializer elements",
                 node->arrayCount);
        semanticErrorAt("E3033", message, item,
                        "this element is beyond the declared array capacity",
                        "increase the array size or remove the extra "
                        "initializer elements.");
      }
    }
    return;
  }

  if (node->child != NULL) {
    ValueType actual = checkExpression(node->child);
    ValueType expected = valueTypeFromType(type);
    if (actual == VALUE_VOID || !typesCompatible(expected, actual)) {
      char message[320];
      snprintf(message, sizeof(message),
               "expected an initializer of type `%s`, found `%s`",
               valueTypeName(expected), valueTypeName(actual));
      semanticErrorAt(
          "E3034", message, node->child,
          "this value does not match the declared type",
          "change the initializer or use a compatible declaration type.");
    }
    checkIntegerRange(type, node->child);
    if (type.kind == TYPE_STR && node->typeSize > 0 &&
        node->child->type == AST_STRING &&
        stringLiteralLength(node->child->strValue) > node->typeSize) {
      semanticErrorAt(
          "E3035", "string literal is larger than the declared buffer",
          node->child, "the string may not fit in this buffer",
          "increase the buffer size or shorten the string literal.");
    }
  }
}

static int statementAlwaysReturns(ASTNode *node);

static int statementListAlwaysReturns(ASTNode *node) {
  for (ASTNode *item = node; item != NULL; item = item->next) {
    if (statementAlwaysReturns(item))
      return 1;
  }
  return 0;
}

static int blockAlwaysReturns(ASTNode *block) {
  if (block == NULL)
    return 0;
  return statementListAlwaysReturns(block->type == AST_BLOCK ? block->child
                                                             : block);
}

static int statementAlwaysReturns(ASTNode *node) {
  if (node == NULL)
    return 0;
  if (node->type == AST_RETREAT || node->type == AST_KILL)
    return 1;
  if (node->type == AST_BLOCK)
    return blockAlwaysReturns(node);
  if (node->type == AST_IF && node->third != NULL) {
    int thenReturns = blockAlwaysReturns(node->right);
    int elseReturns = node->third->type == AST_IF
                          ? statementAlwaysReturns(node->third)
                          : blockAlwaysReturns(node->third);
    return thenReturns && elseReturns;
  }
  return 0;
}

static int methodFlagsSupported(const char *flags, int isMain) {
  char copy[MAX_NAME];
  int sawVoid = 0;
  int sawStatic = 0;
  int sawInline = 0;
  if (flags == NULL || flags[0] == '\0') return 1;
  size_t flagCount = strlen(flags);
  if (flags[0] == ',' || flags[flagCount - 1] == ',' || strstr(flags, ",,") != NULL) return 0;
  snprintf(copy, sizeof(copy), "%s", flags);
  char *cursor = strtok(copy, ",");
  while (cursor != NULL) {
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    size_t length = strlen(cursor);
    while (length > 0 && (cursor[length - 1] == ' ' || cursor[length - 1] == '\t')) {
      cursor[--length] = '\0';
    }
    if (strcmp(cursor, "void") == 0) {
      if (sawVoid) return 0;
      sawVoid = 1;
    } else if (strcmp(cursor, "static") == 0) {
      if (sawStatic || isMain) return 0;
      sawStatic = 1;
    } else if (strcmp(cursor, "inline") == 0) {
      if (sawInline || isMain) return 0;
      sawInline = 1;
    } else {
      return 0;
    }
    cursor = strtok(NULL, ",");
  }
  return 1;
}

static void checkMethod(ASTNode *node) {
  Method *method = node->method;
  if (method == NULL)
    return;
  if (!methodFlagsSupported(method->flags, method->isMain)) {
    char message[320];
    snprintf(message, sizeof(message),
             "function flags `<%s>` contain an unsupported or duplicate flag",
             method->flags);
    semanticErrorAt("E3063", message, node,
                    "the compiler refuses to silently ignore function flags",
                    "the currently supported flags are `void`, `static`, and `inline`; write them as a comma-separated list, for example `<static,inline>`. `main` must not use `static` or `inline` because it needs external C linkage.");
  }
  Method *previousMethod = activeMethod;
  activeMethod = method;
  enterScope();

  for (int i = 0; i < method->paramCount; ++i) {
    Param *param = &method->params[i];
    if (param->type.kind == TYPE_VOID) {
      diagnosticError(
          "error", "E3065", "function parameters cannot have type `void`",
          param->line, param->column, "a parameter must have a value type",
          "remove the parameter, or declare a value type such as `int`, `fp`, "
          "or `str`.");
    }
    ASTNode fake;
    memset(&fake, 0, sizeof(fake));
    snprintf(fake.name, sizeof(fake.name), "%s", param->name);
    fake.line = node->line;
    fake.column = node->column;
    (void)addVariable(&fake, param->type, param->storageFlags, 0, 0);
  }

  checkStatementList(method->body);
  if (method->returnType.kind != TYPE_VOID && !method->isMain) {
    if (!statementListAlwaysReturns(method->body)) {
      char message[320];
      snprintf(message, sizeof(message),
               "function `%s` declares return type `%s` but has no `retreat` "
               "statement",
               method->name,
               method->returnType.kind == TYPE_INT  ? "int"
               : method->returnType.kind == TYPE_FP ? "fp"
                                                    : "str");
      semanticErrorAt("E3036", message, node,
                      "the function may finish without returning a value",
                      "add a `retreat value` statement, or change the return "
                      "type to `void`.");
    }
  }

  leaveScope();
  activeMethod = previousMethod;
}

static void checkStatementList(ASTNode *node) {
  for (ASTNode *currentNode = node; currentNode != NULL;
       currentNode = currentNode->next) {
    checkStatement(currentNode);
  }
}

static void checkBlock(ASTNode *block) {
  if (block == NULL)
    return;
  enterScope();
  checkStatementList(block->type == AST_BLOCK ? block->child : block);
  leaveScope();
}

static void checkStatement(ASTNode *node) {
  if (node == NULL)
    return;
  switch (node->type) {
  case AST_PROGRAM:
  case AST_BLOCK:
    checkBlock(node);
    return;
  case AST_INT_DECL:
  case AST_FP_DECL:
  case AST_STR_DECL:
  case AST_DECL:
  case AST_ARRAY_DECL:
  case AST_VEC_DECL:
    checkDeclaration(node, activeMethod == NULL && scopeDepth == 0);
    return;
  case AST_METHOD:
    checkMethod(node);
    return;
  case AST_OUT: {
    ValueType type = checkExpression(node->child);
    if (type == VALUE_VOID)
      semanticErrorAt("E3021", "cannot output a value of type `void`", node,
                      NULL, "output a value-producing expression instead.");
    return;
  }
  case AST_RETREAT:
    if (activeMethod == NULL)
      semanticErrorAt("E3037", "`retreat` is only valid inside a function",
                      node, NULL,
                      "move this return statement into a function body.");
    if (activeMethod->returnType.kind == TYPE_VOID) {
      if (node->child != NULL)
        semanticErrorAt(
            "E3038", "void function cannot return a value", node, NULL,
            "use bare `retreat` or change the function's return type.");
    } else {
      if (node->child == NULL)
        semanticErrorAt("E3039", "non-void function must return a value", node,
                        NULL,
                        "write `retreat value`, where `value` matches the "
                        "function's return type.");
      ValueType actual = checkExpression(node->child);
      ValueType expected = valueTypeFromType(activeMethod->returnType);
      if (expected == VALUE_STR && node->child->type == AST_VAR) {
        VariableSymbol *returnedSymbol = findVariable(node->child->name);
        if (returnedSymbol != NULL &&
            (returnedSymbol->flags & DECL_FLAG_STRING_BUFFER) != 0 &&
            (returnedSymbol->flags & DECL_FLAG_STATIC) == 0 &&
            returnedSymbol->scopeDepth > 0) {
          semanticErrorAt(
              "E3074", "cannot return a local string buffer", node->child,
              "the returned pointer would refer to storage that stops existing "
              "when the function returns",
              "return a string literal, a `str` parameter, a global string, or "
              "use a caller-provided output buffer.");
        }
      }
      if (!typesCompatible(expected, actual))
        semanticErrorAt("E3040", "return value has the wrong type", node,
                        "this value does not match the function's return type",
                        "return a value with the declared function type.");
      checkIntegerRange(activeMethod->returnType, node->child);
    }
    return;
  case AST_KILL: {
    char flags[MAX_NAME];
    snprintf(flags, sizeof(flags), "%s", node->flags);
    char *cursor = flags;
    while (*cursor != '\0') {
      while (*cursor == ' ')
        ++cursor;
      if (*cursor == '\0')
        break;
      if (*cursor == '"') {
        ++cursor;
        while (*cursor != '\0' && *cursor != '"')
          ++cursor;
        if (*cursor == '"')
          ++cursor;
        continue;
      }
      char *start = cursor;
      while (*cursor != '\0' && *cursor != ' ')
        ++cursor;
      char saved = *cursor;
      *cursor = '\0';
      int supported =
          strcmp(start, "-silent") == 0 || strcmp(start, "-success") == 0 ||
          strcmp(start, "-cleanup") == 0 || strcmp(start, "-error") == 0 ||
          strcmp(start, "-panic") == 0;
      if (!supported) {
        char message[320];
        snprintf(message, sizeof(message),
                 "kill flag `%s` is not implemented by the current runtime",
                 start);
        semanticErrorAt("E3067", message, node,
                        "this flag would otherwise be silently ignored",
                        "currently supported flags are `-silent`, `-success`, "
                        "`-cleanup`, `-error`, and `-panic`.");
      }
      if (saved == '\0')
        break;
      *cursor++ = saved;
    }
    return;
  }
  case AST_IF: {
    ValueType condition = checkExpression(node->child);
    if (!isNumeric(condition))
      semanticErrorAt("E3041", "if condition must be numeric/boolean",
                      node->child, NULL,
                      "use a comparison such as `if (count > 0) { ... }`.");
    checkBlock(node->right);
    if (node->third != NULL) {
      if (node->third->type == AST_IF)
        checkStatement(node->third);
      else
        checkBlock(node->third);
    }
    return;
  }
  case AST_SWITCH: {
    ValueType condition = checkExpression(node->child);
    if (condition != VALUE_INT) {
      semanticErrorAt("E3066", "switch expression must have an integer type",
                      node->child,
                      "C switch statements accept integral values, not strings "
                      "or floating-point values",
                      "switch on an `int` value, and use `arg 1:` / `arg 2:` "
                      "for the cases.");
    }
    int sawDefault = 0;
    enterScope();
    switchDepth++;
    for (ASTNode *item = node->right; item != NULL; item = item->next) {
      if (item->flags[0] == 'd') {
        if (sawDefault)
          semanticErrorAt("E3042",
                          "switch has more than one `incase` default branch",
                          item, "only one default branch is allowed",
                          "remove the extra `incase` branch.");
        sawDefault = 1;
      } else {
        ValueType caseType = checkExpression(item->child);
        if (caseType != condition &&
            !(isNumeric(caseType) && isNumeric(condition))) {
          semanticErrorAt(
              "E3043",
              "switch case has a type incompatible with the switch value", item,
              NULL,
              "use `arg` values of the same type as the switch expression.");
        }
      }
      checkStatementList(item->right);
    }
    switchDepth--;
    leaveScope();
    return;
  }
  case AST_BREAK:
    if (loopDepth == 0 && switchDepth == 0)
      semanticErrorAt(
          "E3044", "`break` is not inside a loop or switch", node, NULL,
          "place `break` inside `during`, `repeat`, or a `switch` case.");
    return;
  case AST_CONT:
    if (loopDepth == 0)
      semanticErrorAt("E3045", "`cont` is not inside a loop", node, NULL,
                      "place `cont` inside a `during` or `repeat` loop.");
    return;
  case AST_DURING: {
    ValueType condition = checkExpression(node->child);
    if (!isNumeric(condition))
      semanticErrorAt(
          "E3046", "loop condition must be numeric/boolean", node->child, NULL,
          "use a comparison such as `during (count < 10) { ... }`.");
    loopDepth++;
    checkBlock(node->right);
    loopDepth--;
    return;
  }
  case AST_REPEAT:
    enterScope();
    if (node->child != NULL) {
      if (isDeclarationType(node->child->type))
        checkDeclaration(node->child, 0);
      else
        (void)checkExpression(node->child);
    }
    if (node->right != NULL) {
      ValueType condition = checkExpression(node->right);
      if (!isNumeric(condition))
        semanticErrorAt("E3046", "repeat condition must be numeric/boolean",
                        node->right, NULL,
                        "use a comparison in the second `repeat` field.");
    }
    if (node->third != NULL)
      (void)checkExpression(node->third);
    loopDepth++;
    checkBlock(node->body);
    loopDepth--;
    leaveScope();
    return;
  case AST_ATTEMPT:
    checkBlock(node->child);
    checkBlock(node->right);
    return;
  case AST_EXPRESSION_STATEMENT:
    (void)checkExpression(node->child);
    return;
  case AST_DATA:
  case AST_CLASS:
    /* Their members are retained for the later type/layout pass. */
    return;
  case AST_SET:
  case AST_ALIAS:
  case AST_INCLUDE:
    return;
  default:
    semanticErrorAt("E3047", "unsupported statement node in semantic analysis",
                    node, NULL,
                    "this construct has been parsed but is not yet supported "
                    "by semantic analysis.");
    return;
  }
}

void checkProgram(ASTNode *program) {
  variableCount = 0;
  functionCount = 0;
  scopeDepth = 0;
  loopDepth = 0;
  switchDepth = 0;
  activeMethod = NULL;

  if (program == NULL || program->type != AST_PROGRAM) {
    diagnosticError(
        "error", "E3000",
        "internal compiler error: expected an AST_PROGRAM root", 0, 1, NULL,
        "pass the root node returned by parseProgram() to checkProgram().");
  }

  /* Register signatures and top-level variables first for forward references.
   */
  for (ASTNode *node = program->child; node != NULL; node = node->next) {
    if (node->type == AST_METHOD) {
      registerFunction(node);
    } else if (isDeclarationType(node->type) && node->name[0] != '\0') {
      Type type = typeFromNode(node);
      int array = node->type == AST_ARRAY_DECL || node->type == AST_VEC_DECL;
      addVariable(node, type, node->storageFlags, node->arrayCount, array);
    }
  }

  for (ASTNode *node = program->child; node != NULL; node = node->next) {
    if (node->type == AST_INT_DECL || node->type == AST_FP_DECL ||
        node->type == AST_STR_DECL || node->type == AST_DECL ||
        node->type == AST_ARRAY_DECL || node->type == AST_VEC_DECL) {
      checkDeclaration(node, 1);
    } else {
      checkStatement(node);
    }
  }
}
