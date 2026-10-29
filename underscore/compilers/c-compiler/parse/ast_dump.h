#ifndef AST_DUMP_H
#define AST_DUMP_H

#include <stdio.h>
#include "parse.h"

void dumpAST(const ASTNode *program, FILE *output);

#endif
