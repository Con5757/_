#include "codegen.h"

static int generatedDataTypeCount = 0;
static Method *activeMethod = NULL;
static ASTNode *topLevelNodes = NULL;

static _Noreturn void codegenError(const char *code, const char *message, ASTNode *node,
                         const char *label, const char *help) {
  diagnosticError("error", code, message,
                  node != NULL ? node->line : 0,
                  node != NULL ? node->column : 1,
                  label, help);
}

static void emitCName(FILE *output, const char *name) {
  for (const char *p = name != NULL ? name : ""; *p != '\0'; ++p) {
    if (*p == '.' || *p == ':') fputc('_', output);
    else fputc((unsigned char)*p, output);
  }
}

static Type nodeType(ASTNode *node) {
  Type type;
  type.kind = TYPE_VOID;
  type.width = 0;
  if (node == NULL) return type;
  if (node->declaredType.kind >= TYPE_INT && node->declaredType.kind <= TYPE_VOID) {
    type = node->declaredType;
  } else {
    switch (node->type) {
    case AST_INT_DECL: type.kind = TYPE_INT; break;
    case AST_FP_DECL: type.kind = TYPE_FP; break;
    case AST_STR_DECL: type.kind = TYPE_STR; break;
    default: break;
    }
  }
  return type;
}

static const char *cType(Type type) {
  if (type.kind == TYPE_STR) return "char *";
  if (type.kind == TYPE_FP) {
    if (type.width >= 64) return "double";
    return "float";
  }
  if (type.kind == TYPE_VOID) return "void";
  if (type.width == 8) return "int8_t";
  if (type.width == 16) return "int16_t";
  if (type.width == 32) return "int32_t";
  if (type.width == 64) return "int64_t";
  return "int";
}

static int pointerDepth(int flags) {
  if ((flags & DECL_FLAG_DOUBLE_POINTER) != 0) return 2;
  if ((flags & DECL_FLAG_POINTER) != 0) return 1;
  return 0;
}

static int methodHasFlag(const Method *method, const char *wanted) {
  char copy[MAX_NAME];
  if (method == NULL || wanted == NULL || method->flags[0] == '\0') return 0;
  snprintf(copy, sizeof(copy), "%s", method->flags);
  char *cursor = strtok(copy, ",");
  while (cursor != NULL) {
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    size_t length = strlen(cursor);
    while (length > 0 && (cursor[length - 1] == ' ' || cursor[length - 1] == '\t')) {
      cursor[--length] = '\0';
    }
    if (strcmp(cursor, wanted) == 0) return 1;
    cursor = strtok(NULL, ",");
  }
  return 0;
}

/* `inline` is emitted with internal linkage in this single-translation-unit
 * backend, avoiding C11's external-inline linkage surprises. */
static void emitFunctionFlags(FILE *output, const Method *method) {
  int isStatic = methodHasFlag(method, "static");
  int isInline = methodHasFlag(method, "inline");
  if (isStatic || isInline) fputs("static ", output);
  if (isInline) fputs("inline ", output);
}

static int isSimpleStaticExpression(ASTNode *node) {
  if (node == NULL) return 1;
  switch (node->type) {
  case AST_NUM:
  case AST_STRING:
  case AST_BOOL:
    return 1;
  case AST_UNARY:
    if (node->operatorType == TOKEN_AT && node->child != NULL && node->child->type == AST_VAR) return 1;
    return (node->operatorType == TOKEN_SUB || node->operatorType == XOR ||
            node->operatorType == NOT) && isSimpleStaticExpression(node->child);
  case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV: case AST_MOD:
  case AST_SHIFT_LEFT: case AST_SHIFT_RIGHT: case AST_BIT_OR:
  case AST_BIT_AND: case AST_BIT_XOR: case AST_EQUAL: case AST_NOT_EQUAL:
  case AST_LESS: case AST_GREATER: case AST_LESS_EQUAL: case AST_GREATER_EQUAL:
    return isSimpleStaticExpression(node->child) && isSimpleStaticExpression(node->right);
  default:
    return 0;
  }
}

static void writeTypeAndModifiers(FILE *output, Type type, int flags) {
  if ((flags & DECL_FLAG_CONST) != 0) fprintf(output, "const ");
  if ((flags & DECL_FLAG_STATIC) != 0) fprintf(output, "static ");
  fprintf(output, "%s", cType(type));
  int pointers = pointerDepth(flags);
  for (int i = 0; i < pointers; ++i) fprintf(output, " *");
  if (pointers == 0) fputc(' ', output);
}

static int isFixedStringArray(ASTNode *node) {
  return node != NULL && node->type == AST_ARRAY_DECL &&
         nodeType(node).kind == TYPE_STR && node->typeSize > 0 &&
         pointerDepth(node->storageFlags) == 0;
}

static int arrayHasOnlyStringLiterals(ASTNode *node) {
  if (node == NULL || node->child == NULL) return 0;
  for (ASTNode *item = node->child; item != NULL; item = item->next) {
    if (item->type != AST_STRING) return 0;
  }
  return 1;
}

static int expressionHasSideEffects(ASTNode *node) {
  if (node == NULL) return 0;
  if (node->type == AST_CALL || node->type == AST_ASSIGN ||
      node->type == AST_UPDATE) return 1;
  return expressionHasSideEffects(node->child) ||
         expressionHasSideEffects(node->right) ||
         expressionHasSideEffects(node->third);
}

static void generateExpression(ASTNode *node, FILE *output);
static void generateStatement(ASTNode *node, FILE *output, int indent);
static void generateStatementList(ASTNode *node, FILE *output, int indent);
static void generateBlock(ASTNode *node, FILE *output, int indent);
static void generateMethod(ASTNode *node, FILE *output, int emitGlobalInitializers);
static void generateTopLevelInitializer(ASTNode *node, FILE *output, int indent);

static void indent(FILE *output, int depth) {
  for (int i = 0; i < depth; ++i) fputs("    ", output);
}

static void emitOutputCall(ASTNode *argument, FILE *output) {
  const char *format = "%lld\\n";
  ValueType type = argument != NULL ? argument->valueType : VALUE_VOID;
  int pointer = argument != NULL && ((argument->storageFlags & (DECL_FLAG_POINTER | DECL_FLAG_DOUBLE_POINTER)) != 0);
  if (pointer) format = "%p\\n";
  else if (type == VALUE_STR) format = "%s\\n";
  else if (type == VALUE_FP) format = "%.9g\\n";
  fprintf(output, "printf(\"%s\", ", format);
  if (pointer) fputs("(void *)", output);
  else if (type != VALUE_STR && type != VALUE_FP) fputs("(long long)", output);
  generateExpression(argument, output);
  fputc(')', output);
}

static void generateExpression(ASTNode *node, FILE *output) {
  if (node == NULL) {
    codegenError("E4000", "missing expression reached code generation", NULL,
                 NULL, "this is usually a compiler bug; check the parser and semantic-analysis stages.");
  }

  switch (node->type) {
  case AST_NUM:
    if (node->valueType == VALUE_FP) {
      fprintf(output, "%.9g", node->fpValue);
    } else if (node->value == LLONG_MIN) {
      /* Avoid an out-of-range positive integer literal in generated C. */
      fputs("(-9223372036854775807LL - 1LL)", output);
    } else {
      fprintf(output, "%lldLL", node->value);
    }
    return;
  case AST_STRING:
    fprintf(output, "\"%s\"", node->strValue);
    return;
  case AST_BOOL:
    fputs(node->value ? "1" : "0", output);
    return;
  case AST_VAR:
    fputs(node->name, output);
    return;
  case AST_MEMBER:
    generateExpression(node->child, output);
    fprintf(output, ".%s", node->name);
    return;
  case AST_NAMESPACE_ACCESS:
    generateExpression(node->child, output);
    fprintf(output, "_%s", node->name);
    return;
  case AST_INDEX:
    generateExpression(node->child, output);
    fputc('[', output);
    generateExpression(node->right, output);
    fputc(']', output);
    return;
  case AST_UNARY: {
    const char *op = node->op;
    switch (node->operatorType) {
    case NOT: op = "!"; break;
    case XOR: op = "~"; break;
    case TOKEN_SUB: op = "-"; break;
    case TOKEN_AT: op = "&"; break;
    case TOKEN_DEREF: op = "*"; break;
    case TOKEN_DDEREF: op = "**"; break;
    default:
      codegenError("E4001", "unsupported unary operator", node,
                   "this operator has no C translation yet",
                   "use a supported arithmetic, logical, address-of (`@`), or dereference (`<-`) operator.");
    }
    fprintf(output, "%s", op);
    generateExpression(node->child, output);
    return;
  }
  case AST_ASSIGN:
    if (node->operatorType == TOKEN_EQUAL && node->child != NULL &&
        (node->child->type == AST_VAR || node->child->type == AST_INDEX) &&
        (node->child->storageFlags & DECL_FLAG_STRING_BUFFER) != 0) {
      if (node->child->type == AST_INDEX &&
          expressionHasSideEffects(node->child->right)) {
        codegenError("E4019", "string-row assignment has a side-effecting index", node->child,
                     "the C translation would need to evaluate this index more than once",
                     "store the index first, then assign through it; for example, `int i = nextIndex()` followed by `words[i] = \"text\"`.");
      }
      fputs("(snprintf(", output);
      generateExpression(node->child, output);
      fputs(", sizeof(", output);
      generateExpression(node->child, output);
      fputs("), \"%s\", ", output);
      generateExpression(node->right, output);
      fputs("), ", output);
      generateExpression(node->child, output);
      fputc(')', output);
      return;
    }
    fputc('(', output);
    generateExpression(node->child, output);
    fprintf(output, " %s ", node->op[0] != '\0' ? node->op : "=");
    generateExpression(node->right, output);
    fputc(')', output);
    return;
  case AST_UPDATE:
    fputc('(', output);
    generateExpression(node->child, output);
    fprintf(output, "%s)", node->op[0] != '\0' ? node->op : "++");
    return;
  case AST_CALL:
    if (strcmp(node->name, "io.out") == 0) {
      ASTNode *arg = node->child;
      if (arg == NULL || arg->next != NULL) {
        codegenError("E4002", "io.out expects exactly one argument", node, NULL,
                     "write `io.out(value)` with one value.");
      }
      if (arg->valueType == VALUE_STR) {
        fputs("uc_io_out_str((const char *)", output);
        generateExpression(arg, output);
        fputs(")", output);
      } else if (arg->valueType == VALUE_FP) {
        fputs("uc_io_out_fp((double)", output);
        generateExpression(arg, output);
        fputs(")", output);
      } else {
        fputs("uc_io_out_int((long long)", output);
        generateExpression(arg, output);
        fputs(")", output);
      }
      return;
    }
    if (strcmp(node->name, "io.in") == 0) {
      fputs("uc_io_in()", output);
      return;
    }
    if (strcmp(node->name, "std.out") == 0 || strcmp(node->name, "std::out") == 0 ||
        strcmp(node->name, "out") == 0) {
      if (node->child == NULL || node->child->next != NULL) {
        codegenError("E4002", "`std.out` expects exactly one argument", node, NULL,
                     "write `std.out(value)` with one value-producing expression.");
      }
      emitOutputCall(node->child, output);
      return;
    }
    emitCName(output, node->name);
    fputc('(', output);
    for (ASTNode *arg = node->child; arg != NULL; arg = arg->next) {
      generateExpression(arg, output);
      if (arg->next != NULL) fputs(", ", output);
    }
    fputc(')', output);
    return;
  case AST_EQUAL: case AST_NOT_EQUAL: case AST_LESS: case AST_GREATER:
  case AST_LESS_EQUAL: case AST_GREATER_EQUAL:
    if (node->child != NULL && node->right != NULL &&
        node->child->valueType == VALUE_STR && node->right->valueType == VALUE_STR) {
      fputs("(strcmp(", output);
      generateExpression(node->child, output);
      fputs(", ", output);
      generateExpression(node->right, output);
      fprintf(output, ") %s 0)", node->op);
    } else {
      fputc('(', output);
      generateExpression(node->child, output);
      fprintf(output, " %s ", node->op);
      generateExpression(node->right, output);
      fputc(')', output);
    }
    return;
  case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV: case AST_MOD:
  case AST_SHIFT_LEFT: case AST_SHIFT_RIGHT: case AST_BIT_OR:
  case AST_BIT_AND: case AST_BIT_XOR:
    fputc('(', output);
    generateExpression(node->child, output);
    fprintf(output, " %s ", node->op);
    generateExpression(node->right, output);
    fputc(')', output);
    return;
  case AST_SPECIAL_BITWISE:
    codegenError("E4003", "operator `~?` has no defined code-generation semantics", node,
                 "the parser recognizes this operator, but its meaning is not specified",
                 "define the operator's behavior before compiling expressions that use it.");
    return;
  case AST_LOGICAL_AND: case AST_LOGICAL_OR: case AST_LOGICAL_NOR:
  case AST_LOGICAL_XOR: case AST_LOGICAL_XNOR: case AST_LOGICAL_NAND:
    if (node->type == AST_LOGICAL_XOR || node->type == AST_LOGICAL_XNOR) {
      /* `~` is logical XOR: compare truth values, not raw numeric values. */
      fputs("((!!(", output);
      generateExpression(node->child, output);
      fputs(")) ", output);
      fputs(node->type == AST_LOGICAL_XOR ? "!= " : "== ", output);
      fputs("(!!(", output);
      generateExpression(node->right, output);
      fputs(")))", output);
      return;
    }
    fputc('(', output);
    if (node->type == AST_LOGICAL_NOR || node->type == AST_LOGICAL_NAND) fputc('!', output);
    fputc('(', output);
    generateExpression(node->child, output);
    const char *logicalOp = node->type == AST_LOGICAL_AND || node->type == AST_LOGICAL_NAND ? "&&" : "||";
    fprintf(output, " %s ", logicalOp);
    generateExpression(node->right, output);
    fputs("))", output);
    return;
  case AST_LOGICAL_NOT:
    fputc('!', output);
    generateExpression(node->child, output);
    return;
  case AST_ARRAY_DECL:
    codegenError("E4004", "array initializer cannot be used as a standalone expression", node,
                 "array literals are supported only as declaration initializers",
                 "declare an array, for example `int values<3> = [1, 2, 3]`.");
    return;
  default:
    codegenError("E4005", "unsupported expression reached code generation", node, NULL,
                 "this construct is parsed but does not yet have a C translation.");
  }
}

static void generateDeclaration(FILE *output, ASTNode *node, int local,
                                int withInitializer) {
  Type type = nodeType(node);
  int flags = node->storageFlags;
  if ((node->storageFlags & DECL_FLAG_GLOBAL) != 0 && local) flags &= ~DECL_FLAG_GLOBAL;

  if (node->type == AST_ARRAY_DECL) {
    if (node->name[0] == '\0') {
      codegenError("E4006", "anonymous array initializer used as a declaration", node, NULL,
                   "give the array a name, such as `int values<3> = [1, 2, 3]`.");
    }
    int count = node->arrayCount;
    if (count <= 0) {
      count = 0;
      for (ASTNode *item = node->child; item != NULL; item = item->next) ++count;
      if (count <= 0) count = 1;
    }
    if (isFixedStringArray(node)) {
      if ((flags & DECL_FLAG_CONST) != 0 && withInitializer && node->child != NULL &&
          !arrayHasOnlyStringLiterals(node)) {
        codegenError("E4019", "constant string arrays require literal initializers", node,
                     "this backend cannot fill a `const` character matrix at runtime",
                     "use string literals in the array initializer, or remove `const` if the rows must be filled at runtime.");
      }
      if ((flags & DECL_FLAG_CONST) != 0) fputs("const ", output);
      if ((flags & DECL_FLAG_STATIC) != 0) fputs("static ", output);
      fprintf(output, "char %s[%d][%d]", node->name, count, node->typeSize + 1);
      if (withInitializer && arrayHasOnlyStringLiterals(node)) {
        fputs(" = {", output);
        int comma = 0;
        for (ASTNode *item = node->child; item != NULL; item = item->next) {
          if (comma++) fputs(", ", output);
          generateExpression(item, output);
        }
        fputc('}', output);
      } else if (local && (node->child == NULL || !withInitializer)) {
        /* Zero-fill omitted rows before applying any runtime initializers. */
        fputs(" = {{0}}", output);
      }
      fputs(";\n", output);
      return;
    }
    writeTypeAndModifiers(output, type, flags);
    fprintf(output, "%s[%d]", node->name, count);
    if (withInitializer && node->child != NULL) {
      fputs(" = {", output);
      int comma = 0;
      for (ASTNode *item = node->child; item != NULL; item = item->next) {
        if (comma++) fputs(", ", output);
        generateExpression(item, output);
      }
      fputc('}', output);
    }
    fputs(";\n", output);
    return;
  }

  if (node->type == AST_VEC_DECL) {
    codegenError("E4007", "dynamic vectors are not implemented in the C backend", node,
                 "`vec` is recognized by the parser but has no runtime representation yet",
                 "use a fixed-size array for now, or add the vector runtime and its allocation/access rules.");
  }

  if (type.kind == TYPE_VOID) {
    codegenError("E4008", "cannot generate a variable with type `void`", node, NULL,
                 "use a value type such as `int`, `fp`, or `str` for variables.");
  }

  if (type.kind == TYPE_STR && pointerDepth(flags) == 0) {
    int capacity = node->typeSize;
    if (capacity <= 0) capacity = node->child != NULL && node->child->type == AST_STRING
                                     ? (int)strlen(node->child->strValue) : MAX_NAME - 1;
    if ((flags & DECL_FLAG_CONST) != 0) fputs("const ", output);
    if ((flags & DECL_FLAG_STATIC) != 0) fputs("static ", output);
    fprintf(output, "char %s[%d]", node->name, capacity + 1);
    if (withInitializer && node->child != NULL && node->child->type == AST_STRING) {
      fputs(" = ", output);
      generateExpression(node->child, output);
    } else if (local && withInitializer && node->child == NULL) {
      fputs(" = \"\"", output);
    }
    fputs(";\n", output);
    return;
  }

  writeTypeAndModifiers(output, type, flags);
  fprintf(output, "%s", node->name);
  if (withInitializer && node->child != NULL) {
    fputs(" = ", output);
    generateExpression(node->child, output);
  }
  fputs(";\n", output);
}

static int hasRuntimeInitializer(ASTNode *node) {
  if (node == NULL || node->child == NULL) return 0;
  if (node->type == AST_ARRAY_DECL) {
    for (ASTNode *item = node->child; item != NULL; item = item->next) {
      if (!isSimpleStaticExpression(item)) return 1;
    }
    return 0;
  }
  if (node->type == AST_STR_DECL && node->child->type == AST_STRING) return 0;
  return !isSimpleStaticExpression(node->child);
}

static void generateTopLevelInitializer(ASTNode *node, FILE *output, int depth) {
  if (node == NULL || node->child == NULL) return;
  if (node->type == AST_ARRAY_DECL) {
    if (!hasRuntimeInitializer(node)) return;
    int index = 0;
    for (ASTNode *item = node->child; item != NULL; item = item->next, ++index) {
      indent(output, depth);
      if (isFixedStringArray(node)) {
        fprintf(output, "snprintf(%s[%d], sizeof(%s[%d]), \"%%s\", ",
                node->name, index, node->name, index);
        generateExpression(item, output);
        fputs(");\n", output);
      } else {
        fprintf(output, "%s[%d] = ", node->name, index);
        generateExpression(item, output);
        fputs(";\n", output);
      }
    }
    return;
  }
  if (node->type == AST_STR_DECL && node->child->type == AST_STRING) return;
  if (node->type == AST_STR_DECL) {
    indent(output, depth);
    fprintf(output, "snprintf(%s, sizeof(%s), \"%%s\", ", node->name, node->name);
    generateExpression(node->child, output);
    fputs(");\n", output);
    return;
  }
  if (hasRuntimeInitializer(node)) {
    indent(output, depth);
    fprintf(output, "%s = ", node->name);
    generateExpression(node->child, output);
    fputs(";\n", output);
  }
}

static void generateOutputStatement(ASTNode *node, FILE *output, int depth) {
  ASTNode *expression = node->child;
  if (expression == NULL) {
    codegenError("E4009", "output statement has no expression", node, NULL,
                 "write `out(value)` with a value inside the parentheses.");
  }
  indent(output, depth);
  const char *format = "%lld\\n";
  int pointer = (expression->storageFlags & (DECL_FLAG_POINTER | DECL_FLAG_DOUBLE_POINTER)) != 0;
  if (pointer) format = "%p\\n";
  else if (expression->valueType == VALUE_STR) format = "%s\\n";
  else if (expression->valueType == VALUE_FP) format = "%.9g\\n";
  fprintf(output, "printf(\"%s\", ", format);
  if (pointer) fputs("(void *)", output);
  else if (expression->valueType != VALUE_STR && expression->valueType != VALUE_FP) fputs("(long long)", output);
  generateExpression(expression, output);
  fputs(");\n", output);
}

static void generateBlock(ASTNode *node, FILE *output, int depth) {
  if (node != NULL && node->type == AST_BLOCK) {
    fputs("{\n", output);
    generateStatementList(node->child, output, depth + 1);
    indent(output, depth);
    fputs("}", output);
  } else {
    fputs("{\n", output);
    generateStatementList(node, output, depth + 1);
    indent(output, depth);
    fputs("}", output);
  }
}

static void generateCaseList(ASTNode *node, FILE *output, int depth) {
  for (ASTNode *item = node; item != NULL; item = item->next) {
    indent(output, depth);
    if (item->flags[0] == 'd') {
      fputs("default:\n", output);
    } else {
      fputs("case ", output);
      generateExpression(item->child, output);
      fputs(":\n", output);
    }
    generateStatementList(item->right, output, depth + 1);
  }
}

static int expressionAlreadyParenthesized(ASTNode *node) {
  if (node == NULL) return 0;
  switch (node->type) {
  case AST_ASSIGN: case AST_UPDATE:
  case AST_ADD: case AST_SUB: case AST_MUL: case AST_DIV: case AST_MOD:
  case AST_SHIFT_LEFT: case AST_SHIFT_RIGHT: case AST_BIT_OR:
  case AST_BIT_AND: case AST_BIT_XOR:
  case AST_LOGICAL_AND: case AST_LOGICAL_OR: case AST_LOGICAL_NOR:
  case AST_LOGICAL_XOR: case AST_LOGICAL_XNOR: case AST_LOGICAL_NAND:
  case AST_EQUAL: case AST_NOT_EQUAL: case AST_LESS: case AST_GREATER:
  case AST_LESS_EQUAL: case AST_GREATER_EQUAL:
    return 1;
  default:
    return 0;
  }
}

static void generateIfCondition(ASTNode *expression, FILE *output) {
  if (expressionAlreadyParenthesized(expression)) {
    fputs("if ", output);
    generateExpression(expression, output);
  } else {
    fputs("if (", output);
    generateExpression(expression, output);
    fputc(')', output);
  }
}

static void generateIfChain(ASTNode *node, FILE *output, int depth, int indentFirst) {
  if (node == NULL) return;
  if (indentFirst) indent(output, depth);
  generateIfCondition(node->child, output);
  fputc(' ', output);
  generateBlock(node->right, output, depth);
  if (node->third != NULL) {
    fputs(" else ", output);
    if (node->third->type == AST_IF) {
      generateIfChain(node->third, output, depth, 0);
    } else {
      generateBlock(node->third, output, depth);
    }
  }
}

static void generateStatement(ASTNode *node, FILE *output, int depth) {
  if (node == NULL) return;
  switch (node->type) {
  case AST_INT_DECL: case AST_FP_DECL: case AST_STR_DECL: case AST_DECL:
  case AST_ARRAY_DECL: case AST_VEC_DECL:
    if ((node->storageFlags & DECL_FLAG_STATIC) != 0 && hasRuntimeInitializer(node)) {
      codegenError("E4018", "a `static` local cannot use a runtime initializer in this backend", node,
                   "this initializer would run each time the function is entered, not once at static initialization",
                   "use a constant initializer, or remove `static` if the value should be initialized on each call.");
    }
    indent(output, depth);
    if (node->type == AST_ARRAY_DECL && isFixedStringArray(node) &&
        node->child != NULL && !arrayHasOnlyStringLiterals(node)) {
      if ((node->storageFlags & DECL_FLAG_CONST) != 0) {
        codegenError("E4019", "constant string arrays require literal initializers", node,
                     "this backend cannot fill a `const` character matrix at runtime",
                     "use string literals in the array initializer, or remove `const` if the rows must be filled at runtime.");
      }
      generateDeclaration(output, node, 1, 0);
      int index = 0;
      for (ASTNode *item = node->child; item != NULL; item = item->next, ++index) {
        indent(output, depth);
        fprintf(output, "snprintf(%s[%d], sizeof(%s[%d]), \"%%s\", ",
                node->name, index, node->name, index);
        generateExpression(item, output);
        fputs(");\n", output);
      }
    } else if (node->type == AST_STR_DECL && node->child != NULL &&
        node->child->type != AST_STRING && pointerDepth(node->storageFlags) == 0) {
      generateDeclaration(output, node, 1, 0);
      indent(output, depth);
      fprintf(output, "snprintf(%s, sizeof(%s), \"%%s\", ", node->name, node->name);
      generateExpression(node->child, output);
      fputs(");\n", output);
    } else {
      generateDeclaration(output, node, 1, 1);
    }
    return;
  case AST_OUT:
    generateOutputStatement(node, output, depth);
    return;
  case AST_RETREAT:
    indent(output, depth);
    if (node->child == NULL) {
      if (activeMethod != NULL && activeMethod->isMain) fputs("return 0;\n", output);
      else fputs("return;\n", output);
    } else {
      fputs("return ", output);
      generateExpression(node->child, output);
      fputs(";\n", output);
    }
    return;
  case AST_KILL:
    indent(output, depth);
    if (strstr(node->flags, "-error") != NULL || strstr(node->flags, "-panic") != NULL) fputs("exit(1);\n", output);
    else fputs("exit(0);\n", output);
    return;
  case AST_EXPRESSION_STATEMENT:
    indent(output, depth);
    if (node->child != NULL && node->child->type == AST_ASSIGN &&
        node->child->operatorType == TOKEN_EQUAL && node->child->child != NULL &&
        (node->child->child->type == AST_VAR || node->child->child->type == AST_INDEX) &&
        (node->child->child->storageFlags & DECL_FLAG_STRING_BUFFER) != 0) {
      ASTNode *target = node->child->child;
      if (target->type == AST_INDEX && expressionHasSideEffects(target->right)) {
        codegenError("E4019", "string-row assignment has a side-effecting index", target,
                     "the C translation would need to evaluate this index more than once",
                     "store the index first, then assign through it; for example, `int i = nextIndex()` followed by `words[i] = \"text\"`.");
      }
      fputs("snprintf(", output);
      generateExpression(target, output);
      fputs(", sizeof(", output);
      generateExpression(target, output);
      fputs("), \"%s\", ", output);
      generateExpression(node->child->right, output);
      fputs(");\n", output);
    } else {
      generateExpression(node->child, output);
      fputs(";\n", output);
    }
    return;
  case AST_IF:
    generateIfChain(node, output, depth, 1);
    fputc('\n', output);
    return;
  case AST_SWITCH:
    indent(output, depth); fputs("switch (", output); generateExpression(node->child, output); fputs(") {\n", output);
    generateCaseList(node->right, output, depth + 1);
    indent(output, depth); fputs("}\n", output);
    return;
  case AST_BREAK:
    indent(output, depth); fputs("break;\n", output); return;
  case AST_CONT:
    indent(output, depth); fputs("continue;\n", output); return;
  case AST_DURING:
    indent(output, depth); fputs("while (", output); generateExpression(node->child, output); fputs(") ", output);
    generateBlock(node->right, output, depth); fputc('\n', output); return;
  case AST_REPEAT:
    indent(output, depth); fputs("for (", output);
    if (node->child != NULL) {
      if (node->child->type == AST_INT_DECL || node->child->type == AST_FP_DECL ||
          node->child->type == AST_STR_DECL || node->child->type == AST_DECL) {
        ASTNode *init = node->child;
        Type type = nodeType(init);
        if (type.kind == TYPE_VOID || type.kind == TYPE_STR || pointerDepth(init->storageFlags) != 0) {
          codegenError("E4010", "this declaration cannot be used as a `repeat` initializer", init,
                       "the first `repeat` field must be a scalar declaration or assignment",
                       "use an integer initializer such as `repeat (int i = 0, i < 10, i++)`.");
        }
        writeTypeAndModifiers(output, type, init->storageFlags);
        fprintf(output, "%s", init->name);
        if (init->child != NULL) { fputs(" = ", output); generateExpression(init->child, output); }
      } else generateExpression(node->child, output);
    }
    fputs("; ", output);
    if (node->right != NULL) generateExpression(node->right, output);
    fputs("; ", output);
    if (node->third != NULL) generateExpression(node->third, output);
    fputs(") ", output);
    generateBlock(node->body, output, depth); fputc('\n', output); return;
  case AST_ATTEMPT:
    codegenError("E4011", "`attempt/incase` has no C runtime implementation yet", node,
                 "the parser accepts this error-handling construct, but its runtime semantics are not defined",
                 "use ordinary conditionals for now, or implement a defined error/exception model before compiling this construct.");
    return;
  case AST_DATA: {
    char generatedName[MAX_NAME];
    if (node->name[0] != '\0') snprintf(generatedName, sizeof(generatedName), "%s", node->name);
    else snprintf(generatedName, sizeof(generatedName), "UnderscoreData%d", ++generatedDataTypeCount);
    fprintf(output, "typedef struct %s {\n", generatedName);
    for (ASTNode *field = node->child; field != NULL; field = field->next) {
      if (field->type != AST_INT_DECL && field->type != AST_FP_DECL && field->type != AST_STR_DECL && field->type != AST_DECL) {
        codegenError("E4012", "data aggregates can contain field declarations only", field, NULL,
                     "move executable statements out of the `data` definition.");
      }
      indent(output, 1);
      Type type = nodeType(field);
      if (type.kind == TYPE_STR && pointerDepth(field->storageFlags) == 0) {
        int capacity = field->typeSize > 0 ? field->typeSize + 1 : MAX_NAME;
        fprintf(output, "char %s[%d]", field->name, capacity);
      } else {
        fprintf(output, "%s %s", cType(type), field->name);
      }
      fputs(";\n", output);
    }
    fprintf(output, "} %s;\n\n", generatedName);
    return;
  }
  case AST_SET:
    fputs("enum { ", output);
    for (ASTNode *item = node->child; item != NULL; item = item->next) {
      fputs(item->name, output);
      if (item->next != NULL) fputs(", ", output);
    }
    fputs(" };\n", output); return;
  case AST_ALIAS:
    if (node->child != NULL && node->child->type == AST_SET) {
      fprintf(output, "typedef enum { ");
      for (ASTNode *item = node->child->child; item != NULL; item = item->next) {
        fputs(item->name, output);
        if (item->next != NULL) fputs(", ", output);
      }
      fprintf(output, " } %s;\n", node->name);
    } else {
      Type type = node->declaredType;
      fprintf(output, "typedef %s %s;\n", cType(type), node->name);
    }
    return;
  case AST_CLASS:
    codegenError("E4013", "class methods and access control are not implemented in the C backend", node,
                 "the class declaration is parsed, but C layout/dispatch semantics are not yet defined",
                 "use `data` for plain fields, or implement a class layout and method-mangling strategy.");
    return;
  case AST_INCLUDE:
    if (strcmp(node->name, "std") != 0) {
      char message[320]; snprintf(message, sizeof(message), "library `%s` has no configured C runtime mapping", node->name);
      codegenError("E4014", message, node, NULL,
                   "add the library's C header/runtime mapping, or use the built-in `std` library.");
    }
    return;
  default:
    codegenError("E4015", "unsupported statement reached code generation", node, NULL,
                 "the construct is parsed, but the C backend does not yet implement it.");
  }
}

static void generateStatementList(ASTNode *node, FILE *output, int depth) {
  for (ASTNode *item = node; item != NULL; item = item->next) {
    generateStatement(item, output, depth);
  }
}

static void generateMethodPrototype(ASTNode *node, FILE *output) {
  Method *method = node->method;
  if (method == NULL || method->isMain) return;
  emitFunctionFlags(output, method);
  fprintf(output, "%s ", cType(method->returnType));
  emitCName(output, method->name);
  fputc('(', output);
  if (method->paramCount == 0) fputs("void", output);
  for (int i = 0; i < method->paramCount; ++i) {
    if (i > 0) fputs(", ", output);
    Param *param = &method->params[i];
    if ((param->storageFlags & DECL_FLAG_CONST) != 0) fputs("const ", output);
    fprintf(output, "%s%s %s", cType(param->type),
            (param->storageFlags & DECL_FLAG_DOUBLE_POINTER) ? " **" :
            (param->storageFlags & DECL_FLAG_POINTER) ? " *" : " ", param->name);
  }
  fputs(");\n", output);
}

static void generateMethod(ASTNode *node, FILE *output, int emitGlobalInitializers) {
  Method *method = node->method;
  if (method == NULL) return;
  if (method->isMain && method->paramCount != 0) {
    codegenError("E4016", "`main` cannot have parameters in the current C backend", node,
                 "C's generated program entry point takes no arguments",
                 "declare `void main() <void> { ... }` with an empty parameter list.");
  }
  if (method->isMain) fputs("int main(void)\n", output);
  else {
    emitFunctionFlags(output, method);
    fprintf(output, "%s ", cType(method->returnType));
    emitCName(output, method->name);
    fputc('(', output);
    if (method->paramCount == 0) fputs("void", output);
    for (int i = 0; i < method->paramCount; ++i) {
      if (i > 0) fputs(", ", output);
      Param *param = &method->params[i];
      if ((param->storageFlags & DECL_FLAG_CONST) != 0) fputs("const ", output);
      fprintf(output, "%s%s %s", cType(param->type),
              (param->storageFlags & DECL_FLAG_DOUBLE_POINTER) ? " **" :
              (param->storageFlags & DECL_FLAG_POINTER) ? " *" : " ", param->name);
    }
    fputs(")\n", output);
  }
  fputs("{\n", output);
  Method *previousMethod = activeMethod;
  activeMethod = method;
  if (emitGlobalInitializers) {
    for (ASTNode *top = topLevelNodes; top != NULL; top = top->next) {
      if (top->type == AST_INT_DECL || top->type == AST_FP_DECL ||
          top->type == AST_STR_DECL || top->type == AST_DECL || top->type == AST_ARRAY_DECL) {
        generateTopLevelInitializer(top, output, 1);
      }
    }
    /* Execute top-level statements before the explicit main body, as module initialization. */
    for (ASTNode *top = topLevelNodes; top != NULL; top = top->next) {
      if (top->type != AST_METHOD && top->type != AST_INT_DECL &&
          top->type != AST_FP_DECL && top->type != AST_STR_DECL &&
          top->type != AST_DECL && top->type != AST_ARRAY_DECL &&
          top->type != AST_DATA && top->type != AST_SET && top->type != AST_ALIAS &&
          top->type != AST_INCLUDE) {
        generateStatement(top, output, 1);
      }
    }
  }
  generateStatementList(method->body, output, 1);
  if (method->isMain) fputs("    return 0;\n", output);
  fputs("}\n\n", output);
  activeMethod = previousMethod;
}

static int hasStaticInitializer(ASTNode *node) {
  if (node == NULL || node->child == NULL) return 0;
  if (node->type == AST_ARRAY_DECL) {
    for (ASTNode *item = node->child; item != NULL; item = item->next) {
      if (!isSimpleStaticExpression(item)) return 0;
    }
    return 1;
  }
  return isSimpleStaticExpression(node->child);
}

static void generateGlobalDeclaration(ASTNode *node, FILE *output) {
  int withInitializer = hasStaticInitializer(node);
  if ((node->storageFlags & DECL_FLAG_CONST) != 0 && !withInitializer) {
    codegenError("E4017", "constant global cannot be initialized at runtime", node,
                 "C requires a const object initializer at its declaration",
                 "initialize this constant with a literal/constant expression, or remove `const` and initialize it inside `main`.");
  }
  generateDeclaration(output, node, 0, withInitializer);
}

void generateC(ASTNode *program, FILE *output) {
  if (program == NULL || output == NULL) {
    codegenError("E4099", "invalid input to C code generation", NULL, NULL,
                 "pass a valid AST root and an open output stream.");
  }
  generatedDataTypeCount = 0;
  topLevelNodes = program->child;
  for (ASTNode *node = program->child; node != NULL; node = node->next) {
  }

  fputs("#include <stdio.h>\n#include <stdint.h>\n#include <stdlib.h>\n#include <string.h>\n\n", output);
  fputs("static int uc_io_out_int(long long value) { return printf(\"%lld\\n\", value); }\n", output);
  fputs("static int uc_io_out_fp(double value) { return printf(\"%.9g\\n\", value); }\n", output);
  fputs("static int uc_io_out_str(const char *value) { return printf(\"%s\\n\", value != NULL ? value : \"(null)\"); }\n", output);
  fputs("static char uc_io_input_buffer[4096];\n", output);
  fputs("static char *uc_io_in(void) { size_t n; if (fgets(uc_io_input_buffer, (int)sizeof(uc_io_input_buffer), stdin) == NULL) { uc_io_input_buffer[0] = '\\0'; return uc_io_input_buffer; } n = strlen(uc_io_input_buffer); while (n && (uc_io_input_buffer[n-1] == '\\n' || uc_io_input_buffer[n-1] == '\\r')) uc_io_input_buffer[--n] = '\\0'; return uc_io_input_buffer; }\n\n", output);

  /* Type definitions must precede function prototypes and global declarations. */
  for (ASTNode *node = program->child; node != NULL; node = node->next) {
    if (node->type == AST_DATA || node->type == AST_SET ||
        node->type == AST_ALIAS || node->type == AST_INCLUDE) {
      /* Validate include mappings even though a configured library may emit no C. */
      generateStatement(node, output, 0);
    }
  }
  fputc('\n', output);

  for (ASTNode *node = program->child; node != NULL; node = node->next) {
    if (node->type == AST_METHOD) generateMethodPrototype(node, output);
  }
  fputc('\n', output);

  for (ASTNode *node = program->child; node != NULL; node = node->next) {
    if (node->type == AST_INT_DECL || node->type == AST_FP_DECL ||
        node->type == AST_STR_DECL || node->type == AST_DECL ||
        node->type == AST_ARRAY_DECL || node->type == AST_VEC_DECL) {
      generateGlobalDeclaration(node, output);
    }
  }
  fputc('\n', output);

  for (ASTNode *node = program->child; node != NULL; node = node->next) {
    if (node->type == AST_METHOD && !(node->method != NULL && node->method->isMain)) {
      generateMethod(node, output, 0);
    }
  }

  ASTNode *explicitMain = NULL;
  for (ASTNode *node = program->child; node != NULL; node = node->next) {
    if (node->type == AST_METHOD && node->method != NULL && node->method->isMain) explicitMain = node;
  }
  if (explicitMain != NULL) {
    generateMethod(explicitMain, output, 1);
    return;
  }

  fputs("int main(void)\n{\n", output);
  for (ASTNode *node = program->child; node != NULL; node = node->next) {
    if (node->type == AST_INT_DECL || node->type == AST_FP_DECL ||
        node->type == AST_STR_DECL || node->type == AST_DECL ||
        node->type == AST_ARRAY_DECL) {
      generateTopLevelInitializer(node, output, 1);
    }
  }
  for (ASTNode *node = program->child; node != NULL; node = node->next) {
    if (node->type != AST_METHOD && node->type != AST_INT_DECL &&
        node->type != AST_FP_DECL && node->type != AST_STR_DECL &&
        node->type != AST_DECL && node->type != AST_ARRAY_DECL &&
        node->type != AST_DATA && node->type != AST_SET && node->type != AST_ALIAS &&
        node->type != AST_INCLUDE) {
      generateStatement(node, output, 1);
    }
  }
  fputs("    return 0;\n}\n", output);
}

void compileProgram(ASTNode *program, const char *sourceFile) {
  char baseName[MAX_NAME * 8];
  char cFile[MAX_NAME * 8 + 24];
  char exeFile[MAX_NAME * 8 + 32];
  const char *separator = strrchr(sourceFile, '/');
  const char *windowsSeparator = strrchr(sourceFile, '\\');
  if (windowsSeparator != NULL && (separator == NULL || windowsSeparator > separator)) separator = windowsSeparator;
  separator = separator != NULL ? separator + 1 : sourceFile;
  snprintf(baseName, sizeof(baseName), "%s", separator);
  char *extension = strrchr(baseName, '.');
  if (extension != NULL) *extension = '\0';
  snprintf(cFile, sizeof(cFile), "%s-obj.c", baseName);
  snprintf(exeFile, sizeof(exeFile), "%s%s", baseName, EXE_SUFFIX);

  FILE *output = fopen(cFile, "w");
  if (output == NULL) {
    char message[320]; snprintf(message, sizeof(message), "could not create generated C file `%.240s`", cFile);
    codegenError("E4020", message, NULL, NULL,
                 "check write permissions for the current directory.");
  }
  generateC(program, output);
  if (fclose(output) != 0) {
    char message[320]; snprintf(message, sizeof(message), "failed while writing generated C file `%.235s`", cFile);
    codegenError("E4021", message, NULL, NULL, "check available disk space and file permissions.");
  }

  if (debugObjs) {
    printf("\n--- Generated C: %s ---\n", cFile);
    FILE *generated = fopen(cFile, "r");
    if (generated != NULL) {
      char line[512];
      while (fgets(line, sizeof(line), generated) != NULL) fputs(line, stdout);
      fclose(generated);
    }
    printf("--- End generated C ---\n\n");
  }

  char command[4096];
#ifdef _WIN32
  snprintf(command, sizeof(command), "%s \"%s\" -o \"%s\"", C_COMPILER, cFile, exeFile);
#else
  snprintf(command, sizeof(command), "%s \"%s\" -o \"%s\"", C_COMPILER, cFile, exeFile);
#endif
  int result = system(command);
  if (result != 0) {
    char message[384]; snprintf(message, sizeof(message), "the C compiler failed while building `%.280s`", cFile);
    codegenError("E4022", message, NULL, NULL,
                 "inspect the generated C with `-objs`; if the error refers to an unsupported language construct, implement its code-generation rule first.");
  }

  if (debugObjs) printf("\n--- Program output ---\n");
#ifdef _WIN32
  char runCommand[4096]; snprintf(runCommand, sizeof(runCommand), "\"%s\"", exeFile);
#else
  char runCommand[4096]; snprintf(runCommand, sizeof(runCommand), "./\"%s\"", exeFile);
#endif
  result = system(runCommand);
  if (result != 0) {
    fprintf(stderr, "note: generated program exited with status %d\n", result);
  }
}
