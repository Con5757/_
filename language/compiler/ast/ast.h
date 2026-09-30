#ifndef AST_H
#define AST_H

#include "../global.h"

typedef enum {
    AST_PROGRAM,
    AST_INT_DECL,
    AST_NUM,
    AST_VAR,
    AST_OUT,
} ASTType;

typedef struct ASTNode {
    ASTType type;
    char name[MAX_NAME];

    int value;
    struct ASTNode* child;
} ASTNode;



#endif