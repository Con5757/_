#include "lex.h"

#include <stdio.h>
#include <stdlib.h>

char* readFile(char* fileName) {
	FILE* file = fopen(fileName, "r");
	if (file == NULL) {
		fclose(file);
		return NULL;
	}

	// size
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	rewind(file);

	// get contents
	char* contents = malloc(size + 1);
	if (contents == NULL) {
		fclose(file);
		return NULL;
	}

	fread(contents, 1, size, file);
	contents[size] = '\0';

	fclose(file);
	return contents;
}

Token lex(char* fileContents) {
	printf("got contents");
}