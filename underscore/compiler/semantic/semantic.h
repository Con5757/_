#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "../global.h"
#include "../parse/parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// helper
static ValueType typeFromType(Type type);
static ASTNode *findMethod(ASTNode *program, const char *name);
static Param *findParam(Method *method, const char *name);
static ValueType getVariableType(const char *name);

// checks
static ValueType checkExpression(ASTNode *expr, ASTNode *program,
                                 Method *method);
static void checkMethod(ASTNode *program, ASTNode *methodNode);

// semantic
int varExists(const char *name);
void semanticError(const char *message, const char *name);
void checkProgram(ASTNode *program);

#endif