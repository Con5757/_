# Contributing to Underscore

Thank you for your interest in contributing to Underscore.

Underscore is a low-level programming language and compiler under active development. Contributions are welcome, whether they involve code, documentation, examples, testing, bug reports, or ideas for the language.

## Before Contributing

Underscore is currently in **pre-alpha**, so the language and compiler architecture are still changing.

Before making a large change, please open an issue or discussion so the proposed change can be discussed before significant development work begins.

Small fixes, documentation improvements, tests, and examples can generally be submitted directly as pull requests.

## What You Can Contribute

Some useful areas for contribution include:

* Compiler development
* Lexer and parser improvements
* AST development
* IR and code generation
* Standard library development
* External libraries
* Documentation
* Examples
* Tests
* Bug fixes
* Compiler performance
* Cross-platform support
* Developer tooling

## Development

The Underscore compiler is currently developed primarily on macOS, with support for Windows and Linux.

The compiler is currently built around a pipeline similar to:

```text
Read File
    ↓
Lex
    ↓
AST / Parse
    ↓
C Code Generation / LLVM IR
```

The compiler architecture is actively evolving, so implementation details may change.

## Pull Requests

Before opening a pull request:

1. Make sure your changes build successfully.
2. Test the affected functionality.
3. Update documentation when necessary.
4. Add or update tests where appropriate.
5. Keep commits focused on the change being made.
6. Clearly describe what was changed and why.

Pull requests should explain:

* What was changed
* Why it was changed
* How it was tested
* Any known limitations

## Language Changes

Changes to the Underscore language itself should be discussed before implementation when they introduce new syntax, semantics, or major language features.

When proposing a language feature, explain:

* The problem it solves
* The proposed syntax
* How it behaves
* Examples of its use
* How it interacts with existing features
* Any alternatives that were considered

The goal is to keep Underscore understandable while maintaining the low-level control that the language is designed around.

## Code Style

Follow the existing style of the codebase.

In particular:

* Keep code readable.
* Prefer clear names over unnecessarily short names.
* Keep functions focused.
* Avoid unnecessary complexity.
* Comment code when the reasoning behind it is not obvious.
* Do not introduce dependencies without a clear reason.

## Documentation

Documentation is an important part of Underscore.

If you introduce or change a language feature, compiler option, library feature, or user-facing behavior, update the relevant documentation as well.

Examples are especially useful for demonstrating new features.

## Reporting Bugs

Bug reports are welcome.

When reporting a bug, include:

* Underscore version or commit
* Operating system
* Relevant source code
* Expected behavior
* Actual behavior
* Steps to reproduce the issue
* Compiler output or error messages

For security vulnerabilities, please follow the process described in [`SECURITY.md`](SECURITY.md) instead of creating a public issue.

## Contributions and Licensing

By submitting a contribution to Underscore, you agree that your contribution may be distributed as part of the Underscore project under the project's license.

Please ensure that you have the right to submit the code, documentation, or other material you contribute.

The full license can be found in [`LICENSE`](LICENSE).

## Questions and Discussions

If you are unsure whether an idea or change fits Underscore, open an issue or discussion before spending significant time implementing it.

Questions, experiments, and alternative approaches are welcome.

Thank you for helping develop Underscore.
