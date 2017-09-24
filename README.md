# StrictAliasing in C++

Towards an understanding of the strict aliasing rules.


## Introduction

It has become clear to me that I do not understand the strict aliasing rules.
This text is an exploration into the standards, articles, blog posts, and 
compiler output that describe when and where we are allowed to break the strict 
aliasing rule.

The rules for C and C++ are different and I will be focusing on C++.

Asembly output shown in this text has been generated on Matt Godbolt's [Compiler 
Explorer](https://godbolt.org) using Clang 5.0 and GCC 7.2 targeting x86_64, using
  `-O3 -std=c++14 -fstrict-aliasing -Wstrict-aliasing`
unless otherwise specified. 

Let's start with the basics. The strict aliasing rule state that a program may 
not access a memory location as two different types. For example, if some 
collection of bits in memory hold a floating point value then we may not access 
those bits as an integer. Makes sence, but the question is; what makes two types 
different? And there are also exceptions we need to consider.

The wording used by [the standard draft](http://eel.is/c++draft/basic.lval#8) is 

> If a program attempts to access the stored value of an object through a 
glvalue of other than one of the following types the behavior is undefined:

And then follows a list of cases that will be detailed in this text. The "stored 
value" part will become important later. 


## Simple examples

Finding a simple, but still realistic, example that actually miscompiles has 
turned out to be more difficult than I expected. I believe all of these break 
the strict aliasing rule. 


### Swap halves

This is a classical example, taken from Mike Acton's [_Understanding strict 
aliasing_](http://cellperformance.beyond3d.com/articles/2006/06/understanding-strict-aliasing.html) 
article on CellPerformance.

```c++
 uint32_t 
 swap_words(uint32_t arg)
 {
   uint16_t* const sp = (uint16_t*)&arg;
   uint16_t        hi = sp[0];
   uint16_t        lo = sp[1];
   
   sp[1] = hi;
   sp[0] = lo;
 
   return arg;
 } 
```

Here we construct an `uint16_t*` that points to memory that holds an `uint32_t`. 
We then both read and write that memory through the wrongly typed pointer. At 
the end we return the supposedly modified `uint32_t` value. Since no write 
happens to or through something that is an `uint32_t`, the compiler is allowed 
to return `arg` unmodified.

Neigher Clang nor GCC does, however:

Clang:
```c
swap_words(uint32_t):
  roll $16, %edi
  movl %edi, %eax
  retq
```

The argument is passed in `%rdi` and the `roll` instructions rotates the bits to 
the left by 16, which is half of 32, which results in the two halves being 
swapped. Finally the rotated value is moved to `%eax`, which is where the 
calling function expects the return value. 


GCC:
```c
swap_words(uint32_t):
  movl %edi, %eax
  movw %di, -2(%rsp)
  shrl $16, %eax
  movw %ax, -4(%rsp)
  movl -4(%rsp), %eax
  ret
```

GCC has a different approach. It writes the lower half of the argument, i.e., 
`%di`, to the stack, shifts the upper half of the value right, or down, by 16 
bits and writes those to the stack as well, but on the "wrong side". An 
`uint32_t` with the halves swapped have now been created on the stack is is read 
back into `%eax` for the return.


## Resources

[C++ standard draft](http://eel.is/c++draft)
[What is the strict aliasing rule?, StackOverflow](https://stackoverflow.com/questions/98650/what-is-the-strict-aliasing-rule)
[Understanding strict aliasing](http://cellperformance.beyond3d.com/articles/2006/06/understanding-strict-aliasing.html)
[Compiler Explorer](https://godbolt.org)