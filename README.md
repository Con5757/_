# Language

This branch contains the core implementation of the Underscore programming language.

It is responsible for the compiler, language frontend, code generation, standard library, and other technical components required to build and run Underscore programs.

This branch is not intended to contain the language documentation . It represents the underlying implementation of the language itself.

## Current Implementation

Underscore currently follows a traditional compiler pipeline consisting of:

* Lexer — Converts source code into tokens.
* Parser — Converts tokens into the language’s syntax structure.
* AST — Represents parsed Underscore programs.
* Semantic Analysis — Handles language-level validation and meaning.
* Code Generation — Produces the current target output.
* Standard Library — Provides core functionality available to Underscore programs.
* Compiler Infrastructure — Handles compilation, diagnostics, configuration, and related functionality.

The implementation is actively evolving alongside the language itself.

## Language

Underscore is a C-inspired programming language with a strong focus on readable and implicit syntax.

The language is designed so that keywords and constructs generally use wording that reflects their purpose rather than relying entirely on traditional C syntax.

One example is its pointer syntax:

```u
int x = 20
int --->pNum = @x
io.out(<---pNum)
```

-> represents a reference, while <- represents a dereference.

Additional - characters represent additional levels of indirection, allowing pointer depth to be expressed directly within the syntax.

## Standard Library

The language branch currently contains the implementation of Underscore’s standard library.

The standard library will continue to expand alongside the language and is intended to provide functionality that would otherwise need to be repeatedly implemented by individual programs.

## Compiler

The Underscore compiler is being developed alongside the language itself.

The current goal is to maintain a relatively simple and understandable compiler implementation while the language is still being designed and established.

As the language matures, the compiler architecture will evolve alongside it.

## Bootstrapping

A major long-term goal of Underscore is to bootstrap the compiler.

The initial compiler implementation is being developed using existing languages and toolchains. Once Underscore is sufficiently mature, the compiler will progressively be rewritten in Underscore itself.

This will allow the language and its compiler to become increasingly self-hosting.

## LLVM

After bootstrapping, the compiler is planned to transition toward generating LLVM IR.

LLVM will provide a common intermediate representation between Underscore and its eventual machine-code targets.

The goal is to use LLVM to make Underscore portable across multiple devices and architectures without requiring a completely separate backend for every target.

The intended progression is roughly:

Underscore Source
       ↓
     Lexer
       ↓
     Parser
       ↓
      AST
       ↓
Semantic Analysis
       ↓
 Code Generation
       ↓
   LLVM IR
       ↓
LLVM Backend
       ↓
 Multiple Targets

This architecture allows the language frontend to remain largely independent from the hardware it ultimately runs on.

## Future Development

Development of this branch will continue to focus on the underlying language and compiler infrastructure.

Major goals include:

* Expanding the language
* Expanding the standard library
* Improving compiler infrastructure
* Bootstrapping the compiler
* Transitioning toward LLVM IR
* Supporting multiple architectures and devices
* Improving configuration-file support
* Continuing to refine Underscore’s syntax and language design

## Scope

The language branch should be thought of as the core of Underscore.

It contains the machinery that makes the language work rather than the documentation or external extensions surrounding it.

## Credit

Document last updated by: Con5757