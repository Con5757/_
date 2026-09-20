// tokens
typedef enum {
	// debug
	TOKEN_UNKNOWN,

	TOKEN_ALPHA,
	TOKEN_DIGIT,

	// parans
	TOKEN_LN_PARAN,
	TOKEN_RN_PARAN,

	TOKEN_LS_PARAN,
	TOKEN_RS_PARAN,

	TOKEN_LC_PARAN,
	TOKEN_RC_PARAN,

	// keyword state
	TOKEN_OUT,
	TOKEN_IN,

	// identifiers
	TOKEN_INT, // int || integer
	TOKEN_FP, // floating point: fp || floatingPoint num1 = 1.2
	TOKEN_STR, // str || string

	// ptr
	TOKEN_PTR // -> normal and will scale inside the parser 
} TokenType;

typedef struct {
	TokenType type;
	char* value;
} Token;

char* readFile(char*);