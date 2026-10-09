# file structure

underscore file structure is pretty straight forward.
It follows a similar structure to that of python.
If no main function is declared, the program will run from top to bottom.
If a main function is present, it'll execute from there.

## Best practice

A couple best practices are:

1. Always have your includes at the top.
2. If you have functions, place them before main, otherwise use prototypes.
2. If you don't have a main function, make sure to kill the file at the last line.
3. Always make sure to kill the file to clean memory and for overall workspace cleanliness.

### Without main

### With main

```u
?std
    with io

void printHello() <void>

int timesTwo(int input) <void> {
    retreat input * 2
}

void main() <void> {
    timesTwo(3)
    printHello()
    kill -silent
}

void printHello() <void> {
    io.out("hello")
}
```


## spacing/tabs