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
    char ->name
    int age
}

int func add(int a, int b) {
    retreat a + b
}
```

```ufg
?init
?macros
?


```

# Why use a config file

In many programming languages, header files or something in the same ball park can be very helpful for minimizing libraries and for closed source projects. 
This is due to the ability to hide all code, but keeping functions useable.

# How do I create a config file

Unlike a normal underscore file, which uses the .u or ._ extensions, config files can be created in two ways.

1. .ufg extension (manual)
2. compiler generation (automatic)

## .ufg

## compiler generation

# Syntax

## Prefixes

## Config Flags