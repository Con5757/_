# Underscore compiler build.
# Command-line overrides remain supported: make CC=clang CFLAGS="...".

.DEFAULT_GOAL := all
CC = gcc
ifeq ($(origin CFLAGS), default)
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror
endif
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
	parse/ast_dump.c \
	semantic/semantic.c \
	codegen/codegen.c \
	imports/imports.c

HEADERS := \
	global.h \
	diagnostic/diagnostic.h \
	lex/lex.h \
	parse/parse.h \
	parse/ast_dump.h \
	semantic/semantic.h \
	codegen/codegen.h \
	imports/imports.h

.PHONY: all clean help test

all: $(PROGRAM)

$(PROGRAM): $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $@

# End-to-end regression tests require Python 3 and a C compiler.
test: $(PROGRAM)
	python3 tests/run_smoke.py

clean:
	$(RM) $(PROGRAM) compiler compiler.exe uc.exe
	$(RM) $(wildcard *-obj.c) $(wildcard *-c.exe)

help:
	@echo "Targets:"
	@echo "  all    Build the compiler (default)"
	@echo "  test   Run end-to-end compiler/runtime tests"
	@echo "  clean  Remove compiler and generated program files"
	@echo "  help   Show this help"
