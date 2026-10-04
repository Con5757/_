# What is a config file?

A config file is an adapted version of a c header file. 
Typically a header contains macros, includes, external variables, structs, and function declarations.

## What's different from a header file?

Although similar, underscores philosophy is to make sure the programmer knows what's going on and what each piece of the puzzle does.
Header files typically only store code of one file, while a config file holds the entire project for that directory.

For example:

```c
#include "header.h" 

int add(int a, int b) {
    return a + b;
}
```

```h
#ifndef HEADER_H
#define HEADER_H

int add(int, int);

#endif
```

I always found this to be quite messy to be completely honest. 
It works and I understand the purpose, but in order to keep it small you need to diversify other functions into other headers.
Which overtime causes you to end up with 5+ header files and it can be annoying to navigate.
Config files are based in organization using tabs and spaces to add structure along with prefixes and flags.

For example:

```u
MACRO PI 3.141

int num = 20

data {
    str name = "bob"
    int age = 23
}

int func add(int a, int b) {
    retreat a + b
}
```

```ufg
?init
?files
    "file.u"

?macros
@file
    MACRO PI

?includes 
    null

?variables
@file
    global int num

?structs
@file
    data Person {
        str name
        int age
    }

?functions
@file
    int func add(int, int)

?endit
```

# Why use a config file

In many programming languages, header files or something in the same ball park can be very helpful for minimizing libraries and for closed source projects. 
This is due to the ability to hide all code, but keeping functions useable.
    Exclusively to underscore, it's also more optimized to use one, as it can significant increase comple and codegen speeds as it has a lot of information off the bat rather than rewriting each file each time.

# How do I create a config file

Unlike a normal underscore file, which uses the .u or ._ extensions, config files can be created in two ways.

1. .ufg extension (manual)
2. compiler generation (automatic)

## .ufg

As shown above a ufg file can be created manually by inserting the extension, and all necessary prefixes.
all prefixes are listed below

A manual .ufg file will be overriden by a generated one if either the name is `config.ufg`

## compiler generation

The more modern way, at compile time, adding the flag --ufg will automatically add all function declarations, all standard flags.
If a `.ufg` file isn't named `config` but you'd rather at compile time it be overridden and updated, you can use the flag --eugf name.
if a name is left blank it'll automatically fail.

Another alternative which isn't recommended is using an init flag.

For example:

```ufg
?init
    <manditoryInCompilePath> --eufg filename.extension
?endit
```

This is not not recommended for a couple reasons.

1. Very heavy to be constantly searching config.ufg and then compile
2. Makes speed of compilation very noticable, use --progress to view progress
3. Harsh on components as it's a very old indev concept.

# Syntax

Understanding how to create and use a config file is very important as it's been understood by now it's not a normal header.
A config file can significant improve your workflow while organizing the codebase
Personally the debugging aspect it provides can be very helpful.
From telling you which functions weren't used, to which files you can separate to minimize thousand line files

A typical example file would be:

```ufg
?init
?files
    "file._"

?macros
@file1
    MACRO PI 

?includes
@std
    io
    string
    memory

?variables
@file1
    global char ->name

?structures
@file1
    data Person {
        int height
        fp weight
    }

?functions
@file1
    void printName(str name)

?endit
```

## Prefixes

As seen before, there are certain prefixes which mean different things. 
All prefixes start with the `?` as seen inside library declaration
The current list is:

1. ?init
2. ?files

## Config Flags

## Credit

Last version of this document created by: Con5757