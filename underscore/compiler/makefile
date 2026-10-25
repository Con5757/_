# Underscore compiler build.
# Override defaults from the command line when needed, e.g. `make CC=clang`.

.DEFAULT_GOAL := all

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS ?=
LDFLAGS ?=
ifeq ($(OS),Windows_NT)
PROGRAM ?= uc.exe
else
PROGRAM ?= compiler
endif

SOURCES := \
	main.c \
	diagnostic/diagnostic.c \
	lex/lex.c \
	parse/parse.c \
	semantic/semantic.c \
	codegen/codegen.c \
	imports/imports.c
	runtime/io_file_runtime.fragment.c

HEADERS := \
	global.h \
	diagnostic/diagnostic.h \
	lex/lex.h \
	parse/parse.h \
	semantic/semantic.h \
	codegen/codegen.h \
	imports/imports.h

.PHONY: all clean help

all: $(PROGRAM)

$(PROGRAM): $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $@

clean:
	$(RM) $(PROGRAM) compiler compiler.exe uc.exe
	$(RM) $(wildcard *-obj.c) $(wildcard *-c.exe)

help:
	@echo "Targets:"
	@echo "  all    Build the compiler (default)"
	@echo "  clean  Remove compiler and generated program files"
	@echo "  help   Show this help"
	@echo ""
	@echo "Overrides: make CC=clang, make PROGRAM=mycompiler"
