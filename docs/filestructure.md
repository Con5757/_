# Source files and execution

Underscore source examples use the `.u` extension. Reusable source modules use `._`. The language
branch currently transpiles source to C before compiling the generated program.

## Entry point

If a `main` function is defined, the compiler uses it as the entry point. `main.u` is a useful
project naming convention, not a special keyword or required filename. When there is no `main`, the
current backend executes supported top-level statements in source order.

```u
?std 
    with io

int timesTwo(int input) <void> {
    retreat input * 2
}

void main() <void> {
    io.out(timesTwo(21))
}
```

## Imports

Place imports at the top of a source file. Standard-library modules use `?std with <module>`;
multiple modules may be comma-separated. Direct helper-file imports use the quoted path form.

```u
?std 
    with io, math
?with "helpers._"

void main() <void> {
    io.out(math.sqrt(81.0))
    io.out(helpers.greeting())
}
```

Project imports resolve relative to the importing file in the patched compiler. Circular imports are
rejected. Imported modules should define helpers, not their own `main` entry point.

## Recommendations

1. Keep imports near the top of each source file.
2. Put reusable helpers before the entry point or in focused module files.
3. Use explicit return values for ordinary error handling; the `attempt/incase` construct is not yet a complete runtime exception model.
4. Close file handles and release heap-owned string results when finished with them.
5. Check [`API_STATUS.md`](API_STATUS.md) before using a library function that may still be in design.

## Indentation and statements

Newlines separate statements, braces delimit blocks, and indentation is used for readability. Tabs or
spaces can be used consistently. Semicolons begin line comments; `;- ... -;` creates a block comment.
