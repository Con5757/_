# filestructure
# Language


# Simple Calculator
## used keywords

### function

`main`
`func`
`retreat`

### datatypes

`int`
`fp`
`void`

### imports/std

`out`
`in`
`std`
`with`
`io`

### conditional

`if`
`elif`
`else`
`count`

## Best practice

## spacing/tabs

## Code example:

```u
?std ..imports are the ONLY part of underscore that use tabs instead of typical scope
    with io ..you can add more using colon or newlines

fp func add(fp[2] input1, fp[2] input2) <void> {
    retreat[2] input1 + input2
} fp func sub(fp[2] input1, fp[2] input2) <void> {
    retreat[2] input1 - input2
} fp func mul(fp[2] input1, fp[2] input2) <void> {
    retreat[2] input1 * input2
} fp func div(fp[2] input1, fp[2] input2) <void> {
    retreat[2] input1 / input2
}

void func main() <void> {
    fp[2] num1 ..initalizing our numbers we'll collect from the users
    fp[2] num2
    int operator ..we can only use true/false/void if we add boolean from std 

    std.out("enter 2 numbers to calculate\n") ..
    std.in(num1, "\n") .. you can use either to create a newline
    std.in(num1).nl

    std.out("1. Add\n2. Subtract\n3. Multiply\n4. Divide: ")
    std.in(operator).nl

    count (operator) {
        arg 1 {
            std.out(add(num1, num2))
            end
        } arg 2 {
            std.out(sub(num1, num2))
            end
        } arg 3 {
            std.out(mul(num1, num2))
            end
        } arg 4 {
            std.out(div(num1, num2))
            end
        }

        incase {
            std.out("invalid operator")
        }
    }
}
```

### output:

testing inputs being `num1 = 2.15`, `num2 = 9.42` and `3` or `*`

```
enter 2 numbers to calculate
2.15
9.42
1. Add
2. Subtract
3. Multiply
4. Divide
3
20.25
```