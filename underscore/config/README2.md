# Underscore UFG manifests (first working iteration)

A `.ufg` file is a package manifest and interface index. It lists source modules and groups the declarations they expose. A directory-based package must keep its `.ufg` file beside the `. _` files referenced by its `?files` section.

## Create a manifest

Create a manifest beside a source file:

```sh
compiler main.u -cufg
```

This creates `main.ufg` next to `main.u`, scans `. _` module files in that directory, and lists discovered variables, data/set/alias declarations, function signatures, and imports. `?macros` is emitted as `null` until source-level macro declarations have a defined AST representation.

Create a manifest inside an existing package folder:

```sh
compiler main.u -cufg--p logs
```

This creates `logs/logs.ufg`. The target folder must already exist. The file name comes from the folder name so the import shorthand remains predictable. Existing manifests are overwritten by this explicit generator command; review version-control changes before accepting the generated output.

## Manifest structure

```ufg
?init

?files
    "logger._"

?macros
    null

?includes
    null

?variables
@logger
    global int logCount

?structs
    null

?functions
@logger
    void writeLog(str message) <void>

?endit
```

Section names start with `?`. The `@name` marker groups declarations by source module. `?files` is the runtime-relevant section in this first version; declaration sections are an API catalogue for people and editor tooling. The source file listed under `?files` is loaded when the package is imported.

## Import a package

Both forms below load `<project>/logs/logs.ufg`, then import the `. _` modules listed by its `?files` section:

```u
?manifest logs
```

```u
&logs
```

Functions from the package are qualified with the folder name. For the example above, call `logs.writeLog("started")`.

The package manifest and source modules must be present locally. Header-only manifests with no implementation files are not linked as external ABI declarations yet; prototype-only semantic registration and external object linking are deferred.

## Standard library imports

Repeated `with` lines are supported:

```u
?std
    with math
    with io
```

The following single-line form is also supported:

```u
?std with math, io
```

`io.out(value)` and `io.in()` are compiler runtime built-ins. They work without a source import, so the editor's `u-main` snippet can start with `void main() <void> { ... }` directly.
