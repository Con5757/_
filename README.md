# Underscore

Underscore is a low-level programming language focused on familiar, readable syntax without sacrificing the control and flexibility expected from a systems level language.

Underscore expands on ideas from C while introducing its own concepts and features, making it a separate language rather than simply another C implementation/reproduction.

The language is designed to be highly expandable, including support for external libraries and projects created by the community.

## Why Underscore?

Low-level programming and system design have always been a major interest to me.

Many programming languages require you to learn unfamiliar/explicit syntax, terminology, or libraries before you can understand what a program is actually doing. Underscore aims to make that process more approachable by using concepts and syntax that are easier to understand while still exposing the underlying systems.

Underscore is also a long-term personal project exploring language design, compilers, systems programming, and eventually computer architecture.

## Example

A simple program can look like:

```_
with io ..imports io functions like in and out

void func main() <void> { ..normal main function strucutre
    io.out("hello world!"); ..simple printf() or console.log() statement
    kill -silent -clean ..ends the program similar to return 0;
                        ..-silent, -clean are flags, meaning "dont say anything when you're done", and 'cls'
                        ..or 'clear' the terminal once you're done
}
```

The goal is for code to be readable without requiring the programmer to immediately search for what a particular piece of syntax means.

More examples and complete programs can be found in the docs branch.

## Current Status

> **Pre-alpha**

Underscore is currently in active development.

### Currently supported

- Data types
- File structure
- Variables
- Conditionals
- Loops
- I/O features
- More features are actively being developed

> **This section is intentionally kept short so it can be updated as the language develops.**

## Compiler

During the alpha stage, the Underscore compiler currently generates C code, which is then compiled using Clang.

The Underscore compiler handles this process automatically.

### Current pipeline

```text
Read File
    ↓
Lex
    ↓
AST / Parse
    ↓
C Code Generation / LLVM IR
```

The compiler architecture is actively evolving, and this pipeline will change as Underscore develops.

## Building

Build Underscore using:

```bash
uc file.extension
```

Additional compiler flags and options can be found in the docs branch.

## Running Underscore

Underscore is designed to be used from a terminal.

A typical workflow will look like:

```bash
uc program._ # other file extensions are 'u', 'uc', 'score', and 'ulang'
```

Underscore is primarily developed on macOS, with development and testing also taking place on Windows.

The language is intended to be available across:

- macOS
- Windows
- Linux

## Documentation

Documentation is available in the docs branch.

The documentation contains information about:

- Language syntax
- Compiler flags
- Language features
- Libraries
- Compiler architecture
- Examples and projects

## Examples & Projects

The repository contains projects written in Underscore, including:

- Simple calculators
- Terminal applications
- Terminal games
- Utilities
- Other experimental programs

These projects demonstrate the capabilities of the language as it develops.

## Roadmap

Refer to [`ROADMAP.md`](ROADMAP.md)

## Long-Term Projects

Underscore is also part of a broader set of programming and systems projects.

Some long-term projects include:

- Building a unique Underscore terminal application
- Creating games in Underscore
- Building a chess computer in Underscore and comparing it with a C implementation
- Developing an operating system
- Exploring computer architecture
- Eventually exploring quantum mechanics and quantum computing from a theoretical perspective

These projects are intended to push the language beyond simple demonstrations and explore what can actually be built with it.

## Contributing

Contributions, ideas, bug reports, and discussions are welcome.

Please read [`CONTRIBUTING.md`](CONTRIBUTING.md) before contributing to the project.

## Bug Reporting

For notifying vulnerabilities, bugs and actionable issues, refer to [`SECURITY.md`](SECURITY.md) along with [`WORKING.md`](WORKING.md) to see which actions and bugs are being worked on at the moment.

## License

Underscore is licensed under the **GNU General Public License v3.0**.

See [`LICENSE`](LICENSE) for the complete license text.

Copyright © 2026 Underscore contributors.

## Credit

Document written by: Con5757