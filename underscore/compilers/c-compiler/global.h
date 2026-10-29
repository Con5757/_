#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int debugTokens;
extern int debugObjs;
extern int debugProgress;

#define MAX_TOKENS 1024
#define MAX_AST_NODES 16384
#define MAX_NAME 64

#define C_COMPILER "gcc"
#define OBJ_SUFFIX "-obj.c"
#define EXE_SUFFIX "-c.exe"

void progress(const char *message);
void clearProgress(void);
char *readFile(const char *filename);
void printTokens(void);
void printHelp(void);
void parseFlags(int argc, char **argv);

#endif
