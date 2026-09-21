#include "lex.h"

#include <stdio.h>
#include <stdlib.h>

int main() {
	// filename
	char fileName[256];

	printf("enter a file: ");
	scanf("%255s", fileName);

	// compilerchain
	char* rfile = readFile(fileName);
	if (rfile != NULL) {
		printf("%s", rfile);
	}
	
	lex(rfile);

	free(rfile);
	return 0;
}