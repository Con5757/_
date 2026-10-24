#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "../global.h"
#include "../parse/parse.h"

int varExists(const char *name);
_Noreturn void semanticError(const char *message, const char *name);
void checkProgram(ASTNode *program);

#endif
