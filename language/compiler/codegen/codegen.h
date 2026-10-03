#ifndef CODEGEN_H
#define CODEGEN_H

#include "../global.h"
#include "../parse/parse.h"
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// helper
static const char *generateType(Type type);
static void generateExpression(ASTNode *node, FILE *output);
static void generateRetreat(ASTNode *node, FILE *output);
static void generateStatement(ASTNode *node, FILE *output);
static void generateMethod(Method *method, FILE *output);

// main
void generateC(ASTNode *program, FILE *output);
void compileProgram(ASTNode *program, const char *sourceFile);

#endif