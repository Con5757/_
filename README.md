# Underscore documentation

This branch stores language references, library API descriptions, examples, and development notes.
The reference pages describe the evolving language; they distinguish implemented compiler/runtime
features from syntax or library ideas that still need a stable contract.

## Start here

- [`docs/syntaxlist._`](docs/syntaxlist._) — grouped syntax and operator reference, with examples.
- [`docs/libSyntaxList._`](docs/libSyntaxList._) — standard-library API catalogue, ownership notes, and examples.
- [`docs/API_STATUS.md`](docs/API_STATUS.md) — functions currently implemented versus design-stage proposals.
- [`docs/filestructure.md`](docs/filestructure.md) — source-file layout, imports, and entry points.

The language and compiler are under active development. A recognized token or parser construct does
not by itself guarantee complete semantic analysis, generated C, or runtime behavior.