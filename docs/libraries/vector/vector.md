# vector

# main 

to access vectors use the `vec` keyword. A vector is a type of dynamic storage.

## init syntax
### no preset
```u
vec name<type>
```

### preset
```u
vec name<int> = {1, 2, 3, 4, 5} 
```

### tuples
tuples are also able to be used, although its not strictly a vector keyword

```u
vec name<tuple> = {"Hello", 29, 1.5215}
```

## pointers
pointers modify the variable not a type or class, therefore attatch the pointer to the variable name

for more indepth info: branch:docs/libraries/vector/pointers.md

```u
vec ->name<int> 
```

## vector dimensions

This built in lib only has up to vec3 but you can include vecdimen for custom amounts, other external libs can be used as well.

in order to use a specific vector

For more accurate and updated info: branch:docs/libraries/vecdimen/vecdimen.md

```_
syntax

..prototype
vector<2> name<type> = {type: type, type2: type2, etc} 

..vector
vec<dimension> name<type>
```

# functions

list of functions that come with the vector library

```u
    give(item) ..puts it at the end of the vector

    take(item) ..takes the first time out

    insert(item, index)

    remove(index)

    get(index)
    set(item, index)
    vlen(vector) ..unique to vectors, NOT len(), fplen(), or slen()
    capacity()
    reserve()
    resize()
    clear()
    empty()
    front()
    back()
```