# Underscore compiler build
# Override defaults from the command line when needed, e.g. `make CC=clang`.

.DEFAULT_GOAL := all

CC = gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS ?=
LDFLAGS ?=
ifeq ($(OS),Windows_NT)
PROGRAM ?= compiler.exe
PYTHON ?= python
else
PROGRAM ?= compiler
PYTHON ?= python3
endif

SOURCES := \
	main.c \
	diagnostic.c \
	lex/lex.c \
	parse/parse.c \
	semantic/semantic.c \
	codegen/codegen.c

HEADERS := \
	global.h \
	diagnostic.h \
	lex/lex.h \
	parse/parse.h \
	semantic/semantic.h \
	codegen/codegen.h

.PHONY: all test clean help

all: $(PROGRAM)

$(PROGRAM): $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $@

test:
	$(PYTHON) tests/run_tests.py

clean:
	$(RM) $(PROGRAM) compiler compiler.exe
	$(RM) $(wildcard *-obj.c) $(wildcard *-c.exe)

help:
	@echo "Targets:"
	@echo "  all    Build the compiler (default)"
	@echo "  test   Run the regression suite"
	@echo "  clean  Remove compiler and generated program files"
	@echo "  help   Show this help"
	@echo ""
	@echo "Overrides: make CC=clang, make PYTHON=python, make PROGRAM=mycompiler"
