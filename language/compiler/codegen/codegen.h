#ifndef CODEGEN_H
#define CODEGEN_H

#include "../parse/parse.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Windows.h>

void generateC(ASTNode *program, FILE *output);
void compileProgram(ASTNode *program, const char *sourceFile);

#endif