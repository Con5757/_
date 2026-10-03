#ifndef GLOBAL_H
#define GLOBAL_H

#define DEBUG_TOKENS 0
#define DEBUG_OBJS 0
#define DEBUG_PROGRESS 0

#define MAX_TOKENS 1024
#define MAX_AST_NODES 1024
#define MAX_NAME 64

#define C_COMPILER "gcc"
#define OBJ_SUFFIX "-obj.c"
#define EXE_SUFFIX "-c.exe"

extern int debugTokens;
extern int debugObjs;
extern int debugProgress;

void progress(const char *message);
void clearProgress(void);
char *readFile(const char *filename);
void printTokens();
void parseFlags(int argc, char **argv);

#endif