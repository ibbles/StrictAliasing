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


## Asembly primer

Integer and pointer arguments are passed in `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`.
`%rsp` is the stack pointer. The current stack frame's data is stored in

[Understanding C by learning assembly](https://www.recurse.com/blog/7-understanding-c-by-learning-assembly)
by David Albert is a longer introduction to reading assembly.

## Simple examples

Finding a simple, but still realistic, example that actually miscompiles has
turned out to be more difficult than I expected. I believe all of these break
the strict aliasing rule.


### Swap halves

This is a classical example, taken from Mike Acton's [_Understanding strict
aliasing_](http://cellperformance.beyond3d.com/articles/2006/06/understanding-strict-aliasing.html)
article on CellPerformance, slightly altered to account for x86_64 being
little-endian and PowerPC being big-endian.

```c++
 uint32_t
 swap_words(uint32_t arg)
 {
   uint16_t* const sp = (uint16_t*)&arg;
   uint16_t        lo = sp[0];
   uint16_t        hi = sp[1];

   sp[1] = lo;
   sp[0] = hi;

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
```c++
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
```c++
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
`uint32_t` with the halves swapped have now been created on the stack it is read
back into `%eax` for the return.


### Extract exponent

This piece of code extracts the exponent bits from a 32-bit floating point
number. It does it by shifting the exponent bits down to the bottom and then
masking away the sign bit.

```c++
uint32_t extract_exponent(float value) {
    uint32_t bits = *(uint32_t*)&value;
    uint32_t exponent_and_sign = bits >> 23;
    uint32_t exponent = exponent_and_sign & ~(1<<8);
    return exponent;
}
```


Clang:
```c++
extract_exponent(float): # @extract_exponent(float)
  movd %xmm0, %eax
  shrl $23, %eax
  movzbl %al, %eax
  retq
```

The Clang version moves the float from `%xmm0` to `%eax` and then shifts the
sign and exponent bits down to the lowest bit positions. It then does the
masking away of the sign bit by zero-extending `%al`, which now holds the eight
exponent bits, into the full register.

GCC:
```c++
extract_exponent(float):
  movd %xmm0, %eax
  shrl $23, %eax
  andb $-2, %ah
  ret
```

GCC starts off in the same way, but does the sign bit masking by and-ing the
second byte of `%eax`, `%ah`, with `-2`, which is encoded as `1111'1110`. The
zero there aligns up with the sign bit at bit 9 of `%eax`.


### Read, write, read

This is an example where Clang and GCC produces code that will produce different
results.

```c++
uint32_t read_write_read(uint32_t* i, float* f) {
    uint32_t v = *i;
    *f = 1.0;
    return *i;
}
```

Clang:
```c++
read_write_read(unsigned int*, float*):
  movl $1065353216, (%rsi) # imm = 0x3F800000
  movl (%rdi), %eax
  retq
```

GCC:
```c++
read_write_read(unsigned int*, float*):
  movl (%rdi), %eax
  movl $0x3f800000, (%rsi)
  ret
```

Where Clang first writes to the `float*` and the reads from the `uint32_t*`, GCC
does it in the opposite order. If both pointers point to the same memory then
Clang will return 1065353216, the integer value of the bit pattern for 1.0f, and
GCC will return whatever integer was at that memory location originally.


### Blocked reorder

Here is a considerably more complicated example where the benefit of the strict
aliasing rule is demonstrated. The goal of this code is to reorder a set of
blocks of data, let's say that they are blocks in a sparse blocked matrix,
according to the permutation given as a set of source-distination indices.
Perhaps we want to optimize the storage for cache locality reasons, or perhaps
we want to create a block-transposed version of the matrix.

`src` contains the data we want to reorder and `dst` is the memory where we want
to place the reordered result. For each block, `src_block_start` and
`dst_block_start` contains an entry holding the start of that block in the `src`
and `dst` arrays; and `block_size` contains the number of floating points values
in that block.

The following statements may help you understand the relationship between the arguments.
- `src` and `dst` point to `float` arrays with an equal number of elements.
- `src_block_starts`, `dst_block_starts`, and `block_sizes` point to `uint32_t` arrays with an equal number of elements.
- `src_block_starts`, `dst_block_starts`, and `block_sizes` all contain `num_blocks` elements.
- The sum of all `uint32_t`s in `block_sizes` is equal to the number of `float`s in `src` and `dst`.
- The range [`src_block_starts[i]`, `src_block_starts[i]+block_sizes[i]`) for block `i` does not overlap the corresponding range for any other block.
- The range [`dst_block_starts[i]`, `dst_block_starts[i]+block_sizes[i]`) for block `i` does not overlap the corresponding range for any other block.
- No such range extend beyond the size of `src` or `dst`.
- A loop over the blocks in `src_block_starts` / `block_sizes` may visit the blocks in `src` in any order.
- A loop over the blocks in `dst_block_starts` / `block_sizes` may visit the blocks in `dst` in any order.
- A loop over the blocks in `src_block_starts` / `block_sizes` will visit each block in `src` exactly once.
- A loop over the blocks in `dst_block_starts` / `block_sizes` will visit each block in `dst` exactly once.

The block copying is probably best done using a call to `memcpy`, but let's
pretend that we're affraid of the performance cost of a function call.

```c++
void blocked_reorder(
    float* src,
    float* dst,
    uint32_t* src_block_starts,
    uint32_t* dst_block_starts,
    uint32_t* block_sizes,
    uint32_t num_blocks)
{
    for (uint32_t block_idx = 0; block_idx < num_blocks; ++block_idx) {
        for (uint32_t elem_idx = 0; elem_idx < block_sizes[block_idx]; ++elem_idx) {
            uint32_t src_start = src_block_starts[block_idx];
            uint32_t dst_start = dst_block_starts[block_idx];
            float f = src[src_start + elem_idx];
            dst[dst_start + elem_idx] = f;
        }
    }
}
```

What we would like the optimizer to do is to store `src_start` and `dst_start` 
in registers while copying a block instead of reloading it from the array for 
every float. Passing `-O3` produces quite a bit of code for this function, so 
`-O3 is used instead. 

First the version without strict aliasin:

Clang:
```c++
blocked_reorder(
    float* src, // rdi
    float* dst, // rsi
    unsigned int* src_block_starts, // rdx
    unsigned int* dst_block_starts, // rcx
    unsigned int* block_sizes,      // r8
    unsigned int num_blocks):       // r8
  pushq %rbx
  testl %r9d, %r9d  // Exit immediately
  je .LBB8_6        // if num_blocks == 0.
  movl %r9d, %r9d
  xorl %r10d, %r10d
.LBB8_2: # =>This Loop Header: Depth=1
  cmpl $0, (%r8,%r10,4)
  je .LBB8_5
  xorl %r11d, %r11d
.LBB8_4: # Parent Loop BB8_2 Depth=1
  movl (%rdx,%r10,4), %eax
  addl %r11d, %eax
  movl (%rdi,%rax,4), %eax
  movl (%rcx,%r10,4), %ebx
  addl %r11d, %ebx
  movl %eax, (%rsi,%rbx,4)
  incl %r11d
  cmpl (%r8,%r10,4), %r11d
  jb .LBB8_4
.LBB8_5: # in Loop: Header=BB8_2 Depth=1
  incq %r10
  cmpq %r9, %r10
  jne .LBB8_2
.LBB8_6:
  popq %rbx
  retq
```


## Resources

[C++ standard draft](http://eel.is/c++draft)
[What is the strict aliasing rule?, StackOverflow](https://stackoverflow.com/questions/98650/what-is-the-strict-aliasing-rule)
[Understanding strict aliasing](http://cellperformance.beyond3d.com/articles/2006/06/understanding-strict-aliasing.html)
[Compiler Explorer](https://godbolt.org)
[Understanding C by learning assembly](https://www.recurse.com/blog/7-understanding-c-by-learning-assembly)