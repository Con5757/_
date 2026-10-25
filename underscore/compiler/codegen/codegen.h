#ifndef CODEGEN_H
#define CODEGEN_H

#include "../global.h"
#include "../parse/parse.h"
#include "../diagnostic/diagnostic.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void generateC(ASTNode *program, FILE *output);
void compileProgram(ASTNode *program, const char *sourceFile);

#endif
