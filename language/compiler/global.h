#ifndef GLOBAL_H
#define GLOBAL_H

extern int debugTokens;
extern int debugObjs;
extern int debugProgress;

#define MAX_TOKENS 1024
#define MAX_AST_NODES 1024
#define MAX_NAME 64

#define C_COMPILER "gcc"
#define OBJ_SUFFIX "-obj.c"
#define EXE_SUFFIX "-c.exe"

void progress(const char *message);
void clearProgress(void);
char *readFile(const char *filename);
void printTokens(void);

#endif