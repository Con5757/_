#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKENS 1024
#define MAX_AST_NODES 1024
#define MAX_NAME 64

// file
const char* TEST_SOURCE =
	"int = num 10\n"
	"out(num)\n";

// lexer
typedef enum {
	TOKEN_EOF,

	TOKEN_INT,

	TOKEN_OUT,

	TOKEN_IDENTIFIER,

	TOKEN_NUM,

	TOKEN_EQUAL,

	TOKEN_ADD,
	TOKEN_SUB,
	TOKEN_MUL,
	TOKEN_DIV,

	TOKEN_LEFT_PAREN,
	TOKEN_RIGHT_PAREN,

	TOKEN_NEWLINE
} TokenType;

typedef struct {
	TokenType type;
	char text[MAX_NAME];
	int line;
} Token;

Token tokens[MAX_TOKENS];
int token_count = 0;

// helper func for lex
void addToken(TokenType type, const char* text, int line) {
	if (token_count >= MAX_TOKENS) {
		printf("max_token error");
		return 1;
	}

	tokens[token_count].type = type;
	tokens[token_count].line = line;

	strncpy(tokens[token_count].text, text, MAX_NAME - 1);
	tokens[token_count].text[MAX_NAME - 1] = '\0';

	token_count++;
}

void lex(const char* source) {
	int i = 0;
	int line = 1;

	while (source[i] != '\0') {
		char c = source[i];
		
		// spaces and nl
		if (c == ' ' || c == '\t' || c == '\r') {
			i++;
			continue;
		} if (c == '\n') {
			addToken(TOKEN_NEWLINE, '\\n', line);
			line++;
			i++;
			continue;
		}

		// digit
		if (isdigit((unsigned char)c)) {
			char num[MAX_NAME];
			int j = 0;

			while (isdigit((unsigned char)source[i])) {
				if (j < MAX_NAME - 1) 
					num[j++] - source[i];

				i++;
			}

			num[j] = '\0';
			addToken(TOKEN_NUM, num, line);
			continue;
		}

		// alpha
		if (isalpha((unsigned char)c) || c == '_') {
			char word[MAX_NAME];
			int j = 0;

			while (isalnum((unsigned char)source[i]) || source[i] == '_') {
				if (j < MAX_NAME - 1)
					word[j++] = source[i];

				i++;
			}

			word[j] = '\0';

			// can add more keywords here
			// words
			if (strcmp(word, "int") == 0) {
				addToken(TOKEN_INT, word, line);
			} else if (strcmp(word, "out") == 0) {
				addToken(TOKEN_OUT, word, line);
			} else {
				addToken(TOKEN_IDENTIFIER, word, line);
			}

			continue;
 		}

		// single char 
		
		// operators 
		if (c == '+') {
			addToken(TOKEN_ADD, "+", line);
			i++;
			continue;
		}
		if (c == '-') {
			addToken(TOKEN_SUB, "-", line);
			i++;
			continue;
		}
		if (c == '*') {
			addToken(TOKEN_MUL, "*", line);
			i++;
			continue;
		}
		if (c == '/') {
			addToken(TOKEN_DIV, "/", line);
			i++;
			continue;
		}

		// equal
		if (c == '=') {
			addToken(TOKEN_EQUAL, "=", line);
			i++;
			continue;
		}

		// parens/brackets
		if (c == '(') { 
			add_token(TOKEN_LEFT_PAREN, "(", line); 
			i++; 
			continue; 
		}
		if (c == ')') { 
			add_token(TOKEN_RIGHT_PAREN, ")", line); 
			i++; 
			continue;
		}

		printf("lexer error on line %d: unknown char '%c\n", line, c);
		exit(i);
	}

	addToken(TOKEN_EOF, "", line);
}

// ast
typedef enum {
	AST_PROGRAM,
	AST_INT_DECL,
	AST_NUM,
	AST_VAR,
	AST_OUT
} ASTType;

typedef struct {
	ASTType type;
	char name[MAX_NAME];

	int value;
	struct ASTNode* child;
} ASTNode;

ASTNode ast_nodes[MAX_AST_NODES];
int ast_count = 0;

ASTNode* newAST(ASTType type) {
	if (ast_count >= MAX_AST_NODES) {
		printf("error, ast is too large\n");
			exit(1);
	}

	ASTNode* node = &ast_nodes[ast_count++];
	memset(node, 0, sizeof(ASTNode));
	node->type = type;

	return node;
}

// parser
int currentToken = 0;

Token* current(void) {
	return &tokens[currentToken];
}

int match(TokenType type) {
	if (current()->type == type) {
		currentToken++;
		return 1;
	}

	return 0;
}

void parserError(const char* message) {
	printf("parser error on line %d: %s\n", current()->line, message);
	exit(1);
}

Token* expect(TokenType type) {
	if (current()->type != type) {
		parserError("unexpected token");
	}

	return &tokens[currentToken++];
}

ASTNode* parseIntDecl(void) { // int num = 10
	expect(TOKEN_INT);
	Token* name = expect(TOKEN_IDENTIFIER);
	expect(TOKEN_EQUAL);
	Token* number = expect(TOKEN_NUM);

	ASTNode* node = newast(AST_INT_DECL);
	strcpy(node->name, name->text);
	node->value = atoi(number->text);

	return node;
}

ASTNode* parseOut() { // out(num)
	expect(TOKEN_OUT);
	expect(TOKEN_LEFT_PAREN);
	Token* name = expect(TOKEN_IDENTIFIER);
	expect(TOKEN_RIGHT_PAREN);

	ASTNode* node = newast(AST_OUT);
	strcpy(node->name, name->text);
	
	return node;
}

ASTNode* parseStatement(void) {
	if (current()->type == TOKEN_INT) {
		return parseIntDecl();
	}
	if (current()->type == TOKEN_OUT) {
		return parseOut();
	}
	parserError("unknown statement");
	
	return NULL;
}

ASTNode* parseProgram(void) {
	ASTNode* program = new_ast(AST_PROGRAM);

	program->child = NULL;

	ASTNode* first = NULL;
	ASTNode* last = NULL;

	while (current()->type != TOKEN_EOF) {
		if (match(TOKEN_NEWLINE)) {
			continue;
		}

		ASTNode* statement = parseStatement();

		if (first == NULL) {
			first = statement;
		}
		else {
			last->child = statement;
		}
		last = statement;

		if (current()->type == TOKEN_NEWLINE) {
			match(TOKEN_NEWLINE);
		} else if (current()->type != TOKEN_EOF) {
			parserError("expected end of line");
		}
	}

	program->child = first;

	return program;
}

// semantic
char vars[MAX_AST_NODES][MAX_NAME];
int var_count = 0;

int varExists(const char* name) {
	for (int i = 0; i < var_count; i++) {
		if (strcmp(vars[i], name) == 0) {
			return 1;
		}
	}

	return 0;
}

void semanticError(const char* message, const char* name) { 
	printf("Semantic error: %s '%s'\n", message, name); 
	exit(1);
}

void checkProgram(ASTNode* program) {
	ASTNode* node = program->child;

	while (node != NULL) {
		if (node->type == AST_INT_DECL) {
			if (varExists(node->name)) {
				semanticError("variable already exists", node->name);
			}

			strcpy(vars[var_count++], node->name);
		} else if (node->type == AST_OUT) {
			if (!varExists(node->name)) {
				semanticError("undefined var");
			}
		}

		node = node->child;
	}
}

void generateC(ASTNode* program, FILE* output) {
	fprintf(output, "#include <stdio.h>\n\n");
	fprintf(output, "int main(void)\n");
	fprintf(output, "{\n");

	ASTNode* node = program->child;

	while (node != NULL) {
		if (node->type == AST_INT_DECL) {
			fprintf(output, "\tint %s = %d;\n", node->name, node->value);
		} else if (node->type == AST_OUT) { 
			fprintf(output, "\tprintf(\"%%d\\n\", %s);\n", node->name); 
		}

		node = node->child;
	}

	fprintf(output, "\treturn 0;\n");
	fprintf(output, "}\n");
}

char* readFile(const char* filename) {
	FILE* file = fopen(filename, "rb");
	if (file == NULL) {
		printf("could not open '%s'\n", filename);
		exit(1);
	}

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* buffer = malloc(size + 1);
	if (buffer == NULL) {
		printf("couldnt allocated source memory");
		fclose(file);
		exit(1);
	}

	fread(buffer, 1, size, file);
	buffer[size] = '\0';
	
	fclose(file);
	return buffer;
}

#define DEBUG_TOKENS 0

void printTokens() {
	for (int i = 0; i < token_count; i++) {
		printf("TOKEN: %-15d TEXT: %s\n", tokens[i].type, tokens[i].text);
	}
}

int main(int argc, char** argv) {
	const char* source = TEST_SOURCE;
	char* loadedSource = NULL;

	if (argc >= 2) {
		loadedSource = readFile(argv[1]);
		source = loadedSource;
	}

	lex(source);

#if DEBUG_TOKENS
	printTokens();
#endif	

	currentToken = 0;
	ASTNode* program = parseProgram();
	checkProgram(program);

	FILE* output = stdout;
	if (argc == 3) {
		output = fopen(argv[2], "w");
		if (output == NULL) {
			printf("could not crate an output file '%s'\n", argv[2]);
			free(loadedSource);
			return 1;
		}
	}

	generateC(program, output);

	if (output != stdout) {
		fclose(output);

		printf("compilation clean.\n");
		printf("generated: %s\n", argv[2]);
	}

	free(loadedSource);
	return 0;
}