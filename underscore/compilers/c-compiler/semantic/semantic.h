#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "../diagnostic/diagnostic.h"
#include "../global.h"
#include "../parse/parse.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int varExists(const char *name);
_Noreturn void semanticError(const char *message, const char *name);
void checkProgram(ASTNode *program);

#endif
