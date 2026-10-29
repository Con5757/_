#ifndef IMPORTS_H
#define IMPORTS_H

#include "../parse/parse.h"
#include "../diagnostic/diagnostic.h"
#include "../lex/lex.h"
#include "../parse/parse.h"
#include "../global.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Expand top-level ?root / with file directives into the program AST. */
ASTNode *expandImports(ASTNode *program, const char *sourceFile,
                       const char *executablePath);

#endif
