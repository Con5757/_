#ifndef CODEGEN_H
#define CODEGEN_H

#include "../global.h"
#include "../parse/parse.h"

#include <stdio.h>

void generateC(ASTNode *program, FILE *output);
void compileProgram(ASTNode *program, const char *sourceFile);

#endif
