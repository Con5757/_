#include "ast_dump.h"

static const char *astTypeName(ASTType type) {
  switch (type) {
  case AST_PROGRAM: return "AST_PROGRAM";
  case AST_BLOCK: return "AST_BLOCK";
  case AST_INT_DECL: return "AST_INT_DECL";
  case AST_FP_DECL: return "AST_FP_DECL";
  case AST_STR_DECL: return "AST_STR_DECL";
  case AST_DECL: return "AST_DECL";
  case AST_ARRAY_DECL: return "AST_ARRAY_DECL";
  case AST_VEC_DECL: return "AST_VEC_DECL";
  case AST_DATA: return "AST_DATA";
  case AST_SET: return "AST_SET";
  case AST_ALIAS: return "AST_ALIAS";
  case AST_CLASS: return "AST_CLASS";
  case AST_INCLUDE: return "AST_INCLUDE";
  case AST_NUM: return "AST_NUM";
  case AST_STRING: return "AST_STRING";
  case AST_BOOL: return "AST_BOOL";
  case AST_VAR: return "AST_VAR";
  case AST_MEMBER: return "AST_MEMBER";
  case AST_NAMESPACE_ACCESS: return "AST_NAMESPACE_ACCESS";
  case AST_INDEX: return "AST_INDEX";
  case AST_UNARY: return "AST_UNARY";
  case AST_ASSIGN: return "AST_ASSIGN";
  case AST_UPDATE: return "AST_UPDATE";
  case AST_OUT: return "AST_OUT";
  case AST_IN: return "AST_IN";
  case AST_CALL: return "AST_CALL";
  case AST_RETREAT: return "AST_RETREAT";
  case AST_KILL: return "AST_KILL";
  case AST_METHOD: return "AST_METHOD";
  case AST_ADD: return "AST_ADD";
  case AST_SUB: return "AST_SUB";
  case AST_MUL: return "AST_MUL";
  case AST_DIV: return "AST_DIV";
  case AST_MOD: return "AST_MOD";
  case AST_SHIFT_LEFT: return "AST_SHIFT_LEFT";
  case AST_SHIFT_RIGHT: return "AST_SHIFT_RIGHT";
  case AST_SPECIAL_BITWISE: return "AST_SPECIAL_BITWISE";
  case AST_BIT_OR: return "AST_BIT_OR";
  case AST_BIT_AND: return "AST_BIT_AND";
  case AST_BIT_XOR: return "AST_BIT_XOR";
  case AST_LOGICAL_NOT: return "AST_LOGICAL_NOT";
  case AST_LOGICAL_AND: return "AST_LOGICAL_AND";
  case AST_LOGICAL_OR: return "AST_LOGICAL_OR";
  case AST_LOGICAL_NOR: return "AST_LOGICAL_NOR";
  case AST_LOGICAL_XOR: return "AST_LOGICAL_XOR";
  case AST_LOGICAL_XNOR: return "AST_LOGICAL_XNOR";
  case AST_LOGICAL_NAND: return "AST_LOGICAL_NAND";
  case AST_EQUAL: return "AST_EQUAL";
  case AST_NOT_EQUAL: return "AST_NOT_EQUAL";
  case AST_LESS: return "AST_LESS";
  case AST_GREATER: return "AST_GREATER";
  case AST_LESS_EQUAL: return "AST_LESS_EQUAL";
  case AST_GREATER_EQUAL: return "AST_GREATER_EQUAL";
  case AST_IF: return "AST_IF";
  case AST_SWITCH: return "AST_SWITCH";
  case AST_CASE: return "AST_CASE";
  case AST_BREAK: return "AST_BREAK";
  case AST_CONT: return "AST_CONT";
  case AST_DURING: return "AST_DURING";
  case AST_REPEAT: return "AST_REPEAT";
  case AST_ATTEMPT: return "AST_ATTEMPT";
  case AST_EXPRESSION_STATEMENT: return "AST_EXPRESSION_STATEMENT";
  }
  return "AST_UNKNOWN";
}

static const char *typeName(Type type) {
  switch (type.kind) {
  case TYPE_INT: return type.width > 0 ? "int-width" : "int";
  case TYPE_FP: return type.width > 0 ? "fp-width" : "fp";
  case TYPE_STR: return type.width > 0 ? "str-width" : "str";
  case TYPE_VOID: return "void";
  }
  return "unknown";
}

static void indent(FILE *output, unsigned depth) {
  for (unsigned i = 0; i < depth; ++i) fputs("  ", output);
}

static void dumpChain(const ASTNode *node, FILE *output, unsigned depth,
                      const char *edge) {
  for (const ASTNode *item = node; item != NULL; item = item->next) {
    indent(output, depth);
    fprintf(output, "%s%s", edge != NULL ? edge : "", astTypeName(item->type));
    if (item->name[0] != '\0') fprintf(output, " name=\"%s\"", item->name);
    if (item->op[0] != '\0') fprintf(output, " op=\"%s\"", item->op);
    if (item->flags[0] != '\0') fprintf(output, " flags=\"%s\"", item->flags);
    if (item->type == AST_NUM) {
      if (item->valueType == VALUE_FP) fprintf(output, " value=%.17g", item->fpValue);
      else fprintf(output, " value=%lld", item->value);
    } else if (item->type == AST_BOOL) {
      fprintf(output, " value=%s", item->value ? "true" : "false");
    } else if (item->type == AST_STRING) {
      fprintf(output, " value=\"%s\"", item->strValue);
    }
    if (item->declaredType.kind != TYPE_VOID || item->type == AST_METHOD) {
      fprintf(output, " type=%s", typeName(item->declaredType));
      if (item->declaredType.width > 0) fprintf(output, "(%d)", item->declaredType.width);
    }
    if (item->method != NULL) {
      fprintf(output, " params=%d return=%s flags=\"%s\"",
              item->method->paramCount, typeName(item->method->returnType),
              item->method->flags);
    }
    fprintf(output, " @%d:%d\n", item->line, item->column);
    dumpChain(item->child, output, depth + 1, "child: ");
    dumpChain(item->right, output, depth + 1, "right: ");
    dumpChain(item->third, output, depth + 1, "third: ");
    dumpChain(item->body, output, depth + 1, "body: ");
  }
}

void dumpAST(const ASTNode *program, FILE *output) {
  if (output == NULL) return;
  if (program == NULL) {
    fputs("<null AST>\n", output);
    return;
  }
  fputs("AST (sibling lists are shown at the same indentation level):\n", output);
  dumpChain(program, output, 0, "");
}
