# Underscore standard library

This folder contains source-level standard modules and the accompanying `std.ufg` registry.
The compiler patch also supplies several native modules through C runtime fragments.

## Ready in this iteration

- **io** — console input/output, newline, error stream, and flushing.
- **file** — integer file handles for line-oriented text files.
- **string** — byte-oriented NUL-terminated string inspection and transformation.
- **math** — arithmetic helpers, rounding, trigonometry, logarithms, interpolation, and clamping.
- **random** — C library pseudo-random numbers; not suitable for cryptographic use.
- **time** — Unix epoch seconds, local/UTC display text, and time differences.
- **memory.escape** — releases owned heap values returned by allocating string helpers; obvious literal/scalar/local-buffer frees are rejected.

The ownership rules and exact function contracts are recorded in `docs/API_STATUS.md` on the
`docs` branch and in `docs/libSyntaxList._`.

## Design-only modules

`vector._`, `system._`, `convert._`, `thread._`, `sync._`, `regex._`, `network._`, and
`encoding._` are reserved API design notes. Their candidate signatures also appear as comments in `std.ufg` so they are visible in the central registry without being activated as compiler declarations. They are not executable implementations; their
public contracts require decisions about generic types, ownership, platform behavior, or error
handling. Do not use calls from those modules in working programs yet.

## Owned strings

`string.copy`, `string.connect`, `string.substr`, `string.slice`, and `string.replace` return
heap-owned text. Release the returned value with `memory.escape` when finished. In-place
operations (`up`, `low`, and `trim`) require mutable storage, never a string literal.
