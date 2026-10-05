# Language

Welcome to the language branch!
This branch holds the physical workings and backends behind underscore.
Code in this branch is very quickly being updated and changing with more significance than other branches.

## Current Implementation



## Language

Inside the language branch you can find the language. 
Who would of thought.
In all seriousness, for updated files, to see whats currently being worked on, and the most relivant changes or syntax, is here.
This is due to organization, and not mixing older possibly outdated or unsafe files from leaking into testing.

Underscores syntax is very unique and one very cool change following our languages morals to make things understandable is our pointer syntax

```u
int x = 20
int ->pX = @x
io.out(<-pX)
```

I personally love this syntax so much.
Its clear, understandable and really makes the script there.

Inside the script we can clearly see were pointing `->` to `pX` and its 'at' `@` `x`.
Theres many other examples like this inside the language and one of our main focuses is to keep expanding the language in this way.

I will make it clear, there is a significant bias toward english, and understanding english grammar to full understand the language and ways of translation and or changing syntax to make it understandable in more languages is a priority.

## Standard Library

As of now, STD or ?std is being worked on actively.
We plan to refine what is needed inside a standard library along with other external libraries.

As much as its easier to keep it small the library can always expand and the current focus is mainly compiler optimization.

## Compiler

Currently underscore runs off of a regular indev compiler setup

* Lex or Tokenize
* Parse and AST
* Semantic checks
* Codegen

Codegen is a little abstract currently as we're transpiling it into C right now.
Later versions will most likely use LLVM IR and its backend to facilitate a stronger language. 

## Bootstrapping

## LLVM

## Future Development

## Scope

## Credit

Document last updated by: Con5757