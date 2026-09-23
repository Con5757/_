#include "lex.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

void printTokens(Token tokens) {
	for (int i = 0; tokens[i] ) {

	}
}

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
	Token token;

	int i = 0;
	while (fileContents[i] != '\0') { // 1. Start at the first character of fileContents
		if (isspace(fileContents[i])) {
			i++;
			continue;
		}

		if (isdigit(fileContents[i])) {
			
		}

		if (isalpha(fileContents[i])) { // any keywords
			char word[256];
			int wordIndex = 0;

			while (isalpha(fileContents[i])) {
				word[wordIndex] = fileContents[i];

				wordIndex++;
				i++;
			}

			word[wordIndex] = '\0';
			printf("%s\n", word);


		}
	}
}