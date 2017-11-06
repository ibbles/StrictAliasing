# StrictAliasing in C++

Towards an understanding of the strict aliasing rules.


## Introduction

It has become clear to me that I do not understand the strict aliasing rules.
This text is an exploration into the standards, articles, blog posts, and
compiler output that describe when and where we are allowed to break the strict
aliasing rule.

The rules for C and C++ are different and I will be focusing on C++.

Assembly output shown in this text has been generated on Matt Godbolt's [Compiler
Explorer](https://godbolt.org) using Clang 5.0 and GCC 7.2 targeting x86_64,
compiled with `-O3 -std=c++14 -fstrict-aliasing -Wstrict-aliasing` unless
otherwise specified.

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

Notice that the text doesn't talk directly about which types of pointers may
alias each other, but instead which types of pointers may point to a particular
object. In extension, any pointer that may point to a particular object must be
assumed to possibly alias any other pointer that may point to the same object.

## Assembly primer

This text uses AT&T syntax. Registers are prefixed with `%`. Registers staring
with `%r` are 64 bit registers and registers starting with `%e` are 32 bit
registers. The lower 32 bits of an `%r` register IS the corresponding `%e`
register. A register with neither `r` nor `e` is a 16 bit or 8 bit register.

Integer and pointer arguments are passed in `%rdi`, `%rsi`, `%rdx`, `%rcx`,
`%r8`, and `%r9`. Floating point arguments are passed in `%xmm0` to `%xmm7`.
Return values from functions are stored in `%rax`. The special register `%rsp`
is the stack pointer. The current stack frame's data is stored in addresses
lower than where `%rsp` points.

Memory is addressed using `(<base>,<index>,<step_size>)` where `<base>` marks
the start of an array, `<index>` is the index in the array to access, and
`<step_size>` is the size in bytes of the array elements. For example,
`(%rdi,%rsi,4)` accesses the `%rsi`th 4-byte element in an array starting at
`%rdi`. Example code that could produce this memory access is

```c++
float array_lookup(float* data, uint64_t index) {
    return data[index];
}
```

Simpler forms of memory addressing exists as well. If we have a pointer directly
to the data then we can access it by `(<pointer>)`. An example is `(%rdi)`. If
we have a pointer to a struct or class and want to access a member variable then
we can use the `<byte_offset>(<pointer>)` adressing mode. To read the member `f` in

```c++
struct Data {
  uint32_t i;
  float f;
  float get_f() { return this->f;}
};
```

from a member function the compiler would insert a load of `4(%rdi)`. Remember
that `%rdi` holds the first argument passed to a function. The compiler passes the
`this` pointer as a hidden first argument. `4(%rdi)` hence loads whatever can be
found 4 bytes into the structure, which skips past the width of one `uint32_t`.

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

Neither Clang nor GCC does, however:

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
number. It does so by shifting the exponent bits down to the bottom and then
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

This is an example where Clang and GCC generates code that will produce different
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

Where Clang first writes to the `float*` and then reads from the `uint32_t*`, GCC
does it in the opposite order. If both pointers point to the same memory then
Clang will return 1065353216, the integer value of the bit pattern for 1.0f, and
GCC will return whatever integer was at that memory location originally.


### Vector set

This example demonstrates the benefit of enforcing the strict aliasing rule.

```c++
class Vector
{
private:
    float* data;
    uint64_t size;
public:
    void set(float value)
    {
      for (uint64_t i = 0; i < this->size; ++i)
      {
          data[i] = value;
      }
    }
};
```

With `-fstrict-aliasing`:
```c++
Vector::set(float): # @Vector::set(float)
  movq 8(%rdi), %r8   // Load the size of the vector.
  testq %r8, %r8      // Early out if
  je .LBB6_4          // size == 0.
  movq (%rdi), %rcx   // Load the float*.
  xorl %edx, %edx     // i = 0.
.LBB6_3: # =>This Inner Loop Header: Depth=1
  movss %xmm0, (%rcx,%rdx,4) // Store the argument to data[i].
  incq %rdx           // ++i.
  cmpq %r8, %rdx      // if i < size:
  jb .LBB6_3          //   Jump back up.
.LBB6_4:
  retq
```

The generated code follows the C++ code pretty much statement for statement.
Let's compare with what one get when strict aliasing is disabled.


With `-fno-strict-aliasing`:
```c++
Vector::set(float): # @Vector::set(float)
  cmpq $0, 8(%rdi) // Check the size of the vector.
  je .LBB6_3       // Early out if empty.
  xorl %eax, %eax  // i = 0.
.LBB6_2: # =>This Inner Loop Header: Depth=1
  movq (%rdi), %rcx           // Load the float*.
  movss %xmm0, (%rcx,%rax,4)  // Store the argument to data[i].
  incq %rax                   // ++i.
  cmpq 8(%rdi), %rax          // if i < size:
  jb .LBB6_2                  //    Jump back up
.LBB6_3:
  retq
```

Notice that the sequence of instructions is very similar to the previous
listing, but here nothing is cached in registers. Every time we need the
`float*` or the number of elements we load from memory via `%rdi`, the `this`
pointer. This is because the compiler must assume that `movss %xmm0,
(%rcx,%rax,4)`, the assignment to `data[i]`, may write to any writable part of
memory. Including the `Vector` instance itself. If the C++ code is allowed to
make any pointer point to any available memory regardless of their respective
types then we could do something like the following.

```c++
Vector v;
v.size = 16;
v.data = (float*)(&v - 2);
v.set(fabricated_float);
```

If an outsider has control over `fabricated_float` then the outsider can cause
`Vector::set` to write those four bytes to anywhere in memory since the outsider
will have control over what `v.data` will point to after it has been
overwritten. This is a security hole just waiting for someone to come and poke
at it.

In short, the type system is there for a reason. Please don't subvert it by
casting one type of pointer into another.

But wait, there's more. The `-fstrict-aliasing` version shown first was just a
snippet. If the compiler can know that the write done by one iteration will have
no influence on preceeding or succeeding iterations it can do all kinds of
trickery to make the loop go faster for large arrays. For example unroll the
loop and use SSE instructions to set 64 floats per iteration, as shown below.
Or 128 floats if you have an AVX capable compiler and CPU.

In the below there is talk about aligned floats. In this case I do not mean that
the floats' addresses in memory are aligned, but that _the number of floats_ is
aligned to some number suitable for efficient SIMD execution. That is, the
number of floats not counting any stragglers that will need to be handled in a
clean-up loop after the main SIMD loop is done.

With `-fstrict-aliasing`:
```c++
Vector::set(float): # @Vector::set(float)
  movq 8(%rdi), %r8  // Load size.
  testq %r8, %r8     // Check if empty.
  je .LBB6_12        // Early out if empty.
  movq (%rdi), %rcx  // Load float*.
  cmpq $7, %r8       // Check if we have more than 7 floats.
  ja .LBB6_3         // If so, jump to unrolled loop selection.
  xorl %edx, %edx    // i = 0.
  jmp .LBB6_11       // Jump to short-array version. The one shown previously.
.LBB6_3:
  movq %r8, %rdx       // Copy size to %rdx.
  andq $-8, %rdx       // Align size down to multiple of 8.
  movaps %xmm0, %xmm1  // Copy parameter to %xmm1.
  shufps $0, %xmm1, %xmm1 # xmm1 = xmm1[0,0,0,0] // Splat to all SIMD lanes of %xmm1.
  leaq -8(%rdx), %rax  // %rax = aligned_size - 8.
  movq %rax, %rdi      // Throw away 'this' pointer. Don't need it any more.
  shrq $3, %rdi        // Divide aligned_size by 8...
  leal 1(%rdi), %esi   // ...and add 1...
  andl $7, %esi        // ...and align down to multiple of 8 again. Not sure what all this produces.
  cmpq $56, %rax       // Do we have 56 or more aligned elements?
  jae .LBB6_5          //   If so, jump to more unrolled loop.
  xorl %edi, %edi      // If not:
  testq %rsi, %rsi     //    Do we have any aligned elements at all?
  jne .LBB6_8          //      If so, jump to less unrolled loop.
  jmp .LBB6_10         //      If not, jump to scalar copy loop.
.LBB6_5:
  leaq -1(%rsi), %rax  // Here is where we get if we had 56 or more aligned floats.
  subq %rdi, %rax      // Set up a few loop counters. To be honest I'm
  xorl %edi, %edi      // not entirely sure what's going on here.
.LBB6_6: # =>This Inner Loop Header: Depth=1
  movups %xmm1, (%rcx,%rdi,4)     // A whole bunch of SSE writes to memory.
  movups %xmm1, 16(%rcx,%rdi,4)
  movups %xmm1, 32(%rcx,%rdi,4)
  movups %xmm1, 48(%rcx,%rdi,4)
  movups %xmm1, 64(%rcx,%rdi,4)
  movups %xmm1, 80(%rcx,%rdi,4)
  movups %xmm1, 96(%rcx,%rdi,4)
  movups %xmm1, 112(%rcx,%rdi,4)
  movups %xmm1, 128(%rcx,%rdi,4)
  movups %xmm1, 144(%rcx,%rdi,4)
  movups %xmm1, 160(%rcx,%rdi,4)
  movups %xmm1, 176(%rcx,%rdi,4)
  movups %xmm1, 192(%rcx,%rdi,4)
  movups %xmm1, 208(%rcx,%rdi,4)
  movups %xmm1, 224(%rcx,%rdi,4)
  movups %xmm1, 240(%rcx,%rdi,4)
  addq $64, %rdi    // i += 64.
  addq $8, %rax     // Why add 8 here?
  jne .LBB6_6       // Why would %rax become 0 when we're done?
  testq %rsi, %rsi  // That is what's tested here, right?
  je .LBB6_10   // Done with massively parallel part, jump to clean-up loop.
.LBB6_8: // Here is where we get if we had less than 56 aligned floats.
  leaq 16(%rcx,%rdi,4), %rax // Don't know what
  negq %rsi                  // these two does.
.LBB6_9: # =>This Inner Loop Header: Depth=1 // Loop with fewer unrolled iterations.
  movups %xmm1, -16(%rax) // Loop that
  movups %xmm1, (%rax)    // writes 8
  addq $32, %rax          // elements per
  incq %rsi               // iteration.
  jne .LBB6_9
.LBB6_10:         // Here is where we get if we have no aligned floats.
  cmpq %rdx, %r8
  je .LBB6_12
.LBB6_11: # =>This Inner Loop Header: Depth=1
  movss %xmm0, (%rcx,%rdx,4) // Store a float.
  incq %rdx                  // ++i.
  cmpq %r8, %rdx             // if i < size:
  jb .LBB6_11                //    Jump back up.
.LBB6_12:
  retq
```



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

What we would like the optimizer to do is to store `src_start`, `dst_start`, and
the block size in registers while copying a block instead of reloading it from
the array for every float. Passing `-O3` produces quite a bit of code for this
function, so `-Os` is used instead.

First the version without strict aliasing, i.e., with `-fno-strict-aliasing`:

Clang:
```c++
blocked_reorder(
    float* src, // rdi
    float* dst, // rsi
    unsigned int* src_block_starts, // rdx
    unsigned int* dst_block_starts, // rcx
    unsigned int* block_sizes,      // r8
    unsigned int num_blocks):       // r9

  pushq %rbx // Store callee-saved register we will be using.

  testl %r9d, %r9d  // Exit immediately
  je .LBB8_6        // if num_blocks == 0.

  movl %r9d, %r9d   // Not sure what this is. Sometimes we move a register to
                    // itself to make sure the upper bits are set to zero, but
                    // it can also be used as a sized NOP to align the instruction
                    // stream for jump targets.

  xorl %r10d, %r10d // Set r10 to 0. This is block_idx.

  // This is the top of the block_idx loop.
.LBB8_2: # =>This Loop Header: Depth=1
  cmpl $0, (%r8,%r10,4) // Check if block_sizes[block_idx] is zero.
  je .LBB8_5            // Go to the next iteration if it is.

  xorl %r11d, %r11d  // Set r11 to 0. This is elem_idx.

  // This is the top of the elem_idx loop.
.LBB8_4: # Parent Loop BB8_2 Depth=1

  // Load and element from src.
  movl (%rdx,%r10,4), %eax // Load src_block_start[block_idx] into eax.
  addl %r11d, %eax         // Add elem_idx to eax.
  movl (%rdi,%rax,4), %eax // Load src[elem_idx + src_start[block_idx]] into eax.

  // Store the element to dst.
  movl (%rcx,%r10,4), %ebx // Load dst_block_start[block_idx] into ebx.
  addl %r11d, %ebx         // Add elem_idx to ebx.
  movl %eax, (%rsi,%rbx,4) // store eax, the value read from src, to dst[elem_idx + dst_block_start[block_idx]].

  // Inner loop postamble.
  incl %r11d               // ++elem_idx.
  cmpl (%r8,%r10,4), %r11d // elem_idx < block_sizes[block_idx].
  jb .LBB8_4               // Jump back up if there are more elements in this block.

  // Outer loop postamble.
.LBB8_5: # in Loop: Header=BB8_2 Depth=1
  incq %r10       // ++block_idx.
  cmpq %r9, %r10  // block_idx < num_blocks.
  jne .LBB8_2     // Jump back up if there are more blocks.

  // Return.
.LBB8_6:
  popq %rbx
  retq
```

The element copying loop, the part that could have been a call to `memcpy`, is
between the `LBB8_4` and `LBB8_5` jump labels. It contains five memory
references per copied float.

Now let's have a look at the version compiled with `-fstrict-aliasing`.

```c++
blocked_reorder(
    float* src, rdi
    float* dst, rsi
    unsigned int* src_block_starts, rdx
    unsigned int* dst_block_starts, rcx
    unsigned int* block_sizes, r8
    unsigned int num_blocks): r9

  // Stack management. There are a lot more registers being used this time.
  pushq %rbp
  pushq %r15
  pushq %r14
  pushq %rbx

  testl %r9d, %r9d // Return immediately if there
  je .LBB9_6       // are no blocks.

  movl %r9d, %r9d    // Zero extend or align instruction stream.

  xorl %r10d, %r10d // block_idx = 0.

  // This is the top of the block_idx loop.
.LBB9_2: # =>This Loop Header: Depth=1
  movl (%r8,%r10,4), %r11d  // Load block_size[block_idx].
  testq %r11, %r11          // Skip this iteration is the block is empty.
  je .LBB9_5

  movl (%rcx,%r10,4), %r14d // Load dst_block_starts[block_idx]. Let's call it dst_start.
  movl (%rdx,%r10,4), %r15d // Load src_block_starts[block_idx]. Let's call it src_start.
  xorl %eax, %eax           // elem_idx = 0.

  // This is the top of the elem_idx loop.
.LBB9_4: # Parent Loop BB9_2 Depth=1
  leal (%r15,%rax), %ebx   // Compute src_start + elem_idx.
  movl (%rdi,%rbx,4), %ebp // Load src[src_start + elem_idx].
  leal (%r14,%rax), %ebx   // Compute dst_start + elem_idx.
  movl %ebp, (%rsi,%rbx,4) // Store loaded value to dst[dst_start + elem_idx].

  // Inner loop postamble.
  incq %rax       // ++elem_idx.
  cmpq %r11, %rax // elem_idx < block_size[block_idx].
  jb .LBB9_4      // Jump back up if there are more more elements in this block.

  // Outer loop postamble.
.LBB9_5: # in Loop: Header=BB9_2 Depth=1
  incq %r10      // ++block_idx.
  cmpq %r9, %r10 // block_idx < num_blocks.
  jne .LBB9_2    // Jump back up if more blocks.

  // Return
.LBB9_6:
  popq %rbx
  popq %r14
  popq %r15
  popq %rbp
  retq
```

In this case the inner loop contains only two memory references per iteration.

Enough with the academics, let's do some measurements. For the measurements we
want the best possible performance that the compiler can give us and don't care
for assembly readability anymore. Therefore `-O3` is passed to the compiler
instead of `-Os`.

The image below graph the execution time per block for ten random reorderings of
an increasing number of blocks, with each block containing 30,000 floats (117
KiB). The large number of elements per block is to ensure that the overhead per
block is small in comparison.

![Performance graph](blocked_reorder.svg "Blocked reorder performance graph")

It is clear that the two `memcpy` versions are the performance winners, and that
`memcpy` has the same performance regaredless of whether the binary is built
with strict aliasing or not. I see two possible reasons for the independence on
strict aliasing in memcpy. One is that `memcpy` operates on raw memory and isn't
bound by the strict aliasing rules at all. In fact, using `memcpy` is one of the
correct ways in which we can move bits between objects of different types. The
second reason is that `memcpy` is part of a compiled library and doesn't care
about how we compile our application.

Looking at the two loop based versions we see a clear lead for the strict
aliasing version. It's almost as fast as the `memcpy` version.

A more comparatory view is given in the image below, where the execution time
per block is given in comparison to the loop-based version compiled with strict
aliasing enabled. In this worst-case scenario passing `-fno-strict-aliasing` to
the compiler increased the execution time of the application by almost four
times for runs with more than a handfull blocks.

![Performance graph](blocked_reorder_normalized.svg "Blocked reorder performance graph")


For the curios, the main loops, when compiled with `-O3`, are:

Loop, no-strict:
```c++
%rax: Index into src, or element from src.
%rdx: src_block_starts.
%rdi: src.
%rsi: dst.
%rcx: dst_block_starts.
%rbx: Index into dst.
%r10: block_idx.
%r11d: elem_idx, i.e., offset in current block.

mov    (%rdx,%r10,4),%eax // Load src_block_starts[block_idx].
add    %r11d,%eax         // Compute offset in src.
mov    (%rdi,%rax,4),%eax // Load src[%eax].
mov    (%rcx,%r10,4),%ebx // Load dst_block_starts[block_idx].
add    %r11d,%ebx         // Compute offset in dst.
mov    %eax,(%rsi,%rbx,4) // Store loaded element to dst[%ebx].
inc    %r11d              // ++elem_idx;
cmp    (%r8,%r10,4),%r11d // elem_idx < block_sizes[block_idx];
jb     0x400ce0
```

This is the same as with `-Os`.

Loop, with-strict:
```c++
%rdi: src.
%rsi: dst.
%r12: src_block_start.
%r15: dst_block_start.
%rax: elem_idx.
%rbp: Element to read or write, or its address.

lea    -0x3(%r12,%rax,1),%ebp // Compute src_start + elem_idx.
mov    (%rdi,%rbp,4),%ebp     // Load an element from src.
lea    -0x3(%r15,%rax,1),%ebx // Compute dst_start + elem_idx.
mov    %ebp,(%rsi,%rbx,4)     // Store the element to dst.

lea    -0x2(%r12,%rax,1),%ebx // Compute src_start + elem_idx.
mov    (%rdi,%rbx,4),%ebx     // Load an element from src.
lea    -0x2(%r15,%rax,1),%ebp // Compute dst_start + elem_idx.
mov    %ebx,(%rsi,%rbp,4)     // Store the element to dst.

lea    -0x1(%r12,%rax,1),%ebx // Compute src_start + elem_idx.
mov    (%rdi,%rbx,4),%ebx     // Load an element from dst.
lea    -0x1(%r15,%rax,1),%ebp // Compute dst_start + elem_idx.
mov    %ebx,(%rsi,%rbp,4)     // Store the element to dst.

lea    (%r12,%rax,1),%ebx     // Compute src_start + elem_idx.
mov    (%rdi,%rbx,4),%ebx     // Load an element from dst.
lea    (%r15,%rax,1),%ebp     // Compute dst_start + elem_idx.
mov    %ebx,(%rsi,%rbp,4)     // Store the element to dst.

lea    0x1(%r14,%rax,1),%rbx  // Compute block_idx.
add    $0x4,%rax              // elem_idx += 4;
cmp    %r11,%rbx              // elem_idx < block_size.
jb     0x400d40


// Register used outside of the innermost loop:

%r10: block_idx.
%r9:  num_blocks.
%r8:  Very large number. Block-sizes, I guess.
r11d: 4-byte value from block-indexed array %r8. Block size, I guess.
r12d: 4-byte value from block-indexed array %rdx. Block start, I guess.
r15d: 4-byte value from block-indexed array %rcx. Block start, I guesss.
r14:  r11d - 1. Block-size-ish.
r13:  r11. Block-size. AND-ed with 0x3. Align down to 4.
%rdx: Very large number. Block starts, I guess.
```

With strict aliasing the compiler is allowed to unroll the loop, but not use
vector instructions. We may cache the block meta-data in registers, but not the
block data itself. That is because `src` and `dst` are both `float*`, so a write
to `dst` may also be a write to `src`. That's why the write of the preceeding
floats has to finish before any of the succeeding reads can be done. In C the
`restrict` keyword is used to tell the compiler that two pointers can't alias,
but we don't have that in C++. Some compilers have extensions that does the same
thing, such as `__restrict__` or `__declspec(restrict)`.

When marking `src` and `dest` with `__restrict__` Clang produces the following for the inner loop:

```c++
.LBB9_12: # Parent Loop BB9_2 Depth=1
  movl %ebp, %r9d
  movups (%rdi,%r9,4), %xmm0   // Load 8 floats
  movups 16(%rdi,%r9,4), %xmm1 // at the time.
  movl %eax, %r9d
  movups %xmm0, (%rsi,%r9,4)   // Store 8 floats
  movups %xmm1, 16(%rsi,%r9,4) // at the time.
  addl $8, %eax    // Step src and dst
  addl $8, %ebp    // pointers by 8.
  addq $-8, %rbx   // Decrement num remaning to copy.
  jne .LBB9_12
```


```c++

```

`memcpy`, caller loop:
```c++
mov    -0xc(%r14),%eax
lea    (%rsi,%rax,4),%rdi
mov    -0xc(%r12),%eax
lea    (%rdx,%rax,4),%rsi
mov    -0xc(%r13),%edx
shl    $0x2,%rdx
callq  0x400b80 <memcpy@plt>
mov    -0x8(%r14),%eax
mov    -0x38(%rbp),%rcx
lea    (%rcx,%rax,4),%rdi
mov    -0x8(%r12),%eax
mov    -0x30(%rbp),%rcx
lea    (%rcx,%rax,4),%rsi
mov    -0x8(%r13),%edx
shl    $0x2,%rdx
callq  0x400b80 <memcpy@plt>
mov    -0x4(%r14),%eax
mov    -0x38(%rbp),%rcx
lea    (%rcx,%rax,4),%rdi
mov    -0x4(%r12),%eax
mov    -0x30(%rbp),%rcx
lea    (%rcx,%rax,4),%rsi
mov    -0x4(%r13),%edx
shl    $0x2,%rdx
callq  0x400b80 <memcpy@plt>
mov    (%r14),%eax
mov    -0x38(%rbp),%rcx
lea    (%rcx,%rax,4),%rdi
mov    (%r12),%eax
mov    -0x30(%rbp),%rcx
lea    (%rcx,%rax,4),%rsi
mov    0x0(%r13),%edx
shl    $0x2,%rdx
callq  0x400b80 <memcpy@plt>
mov    -0x30(%rbp),%rdx
mov    -0x38(%rbp),%rsi
add    $0x10,%r13
add    $0x10,%r12
add    $0x10,%r14
add    $0xfffffffffffffffc,%r15
jne    0x400e60 <blocked_reorder_memcpy(float*, float*, unsigned int*, unsigned int*, unsigned int*, unsigned int)+176>
```

`memcpy` itself:
```c++
prefetcht0 -0x1c0(%rsi)
prefetcht0 -0x280(%rsi)
movdqa -0x10(%rsi),%xmm0
movdqa -0x20(%rsi),%xmm1
movdqa -0x30(%rsi),%xmm2
movdqa -0x40(%rsi),%xmm3
movdqa -0x50(%rsi),%xmm4
movdqa -0x60(%rsi),%xmm5
movdqa -0x70(%rsi),%xmm6
movdqa -0x80(%rsi),%xmm7
lea    -0x80(%rsi),%rsi
sub    $0x80,%rdx
movdqa %xmm0,-0x10(%rdi)
movdqa %xmm1,-0x20(%rdi)
movdqa %xmm2,-0x30(%rdi)
movdqa %xmm3,-0x40(%rdi)
movdqa %xmm4,-0x50(%rdi)
movdqa %xmm5,-0x60(%rdi)
movdqa %xmm6,-0x70(%rdi)
movdqa %xmm7,-0x80(%rdi)
lea    -0x80(%rdi),%rdi
jae    0x7ffff72bbaf0 <__memcpy_ssse3+1088>
```

The version using `memcpy` can use both vector instructions and loop unrolling.
In fact, it unrolled both the byte copying inside `memcpy` and our loop over the
blocks, making four calls to `memcpy` per iteration. And some memory prefetching
for good measure.

I'm actually surprised that the elemnt-wise copying loop with strict aliasing
enabled was so close in runtime performance. That's the benefit of CPU caches
and store buffers, I guess. For large enough block counts we become completely
memory bandwidth bound.

I may make test with smaller block sizes as well. I believe that thousands of
blocks each holding 30'000 floats is a bit larger than the typical desktop work
load.


## The standard

Let's have a look at the [standard text](http://eel.is/c++draft/basic.lval#8)
again, this time including the list of types.

> If a program attempts to access the stored value of an object through a
> glvalue of other than one of the following types the behavior is undefined
>  - the dynamic type of the object,
>  - a cv-qualified version of the dynamic type of the object,
>  - a type similar to the dynamic type of the object,
>  - a type that is the signed or unsigned type corresponding to the dynamic type of the object,
>  - a type that is the signed or unsigned type corresponding to a cv-qualified version of the dynamic type of the object,
>  - an aggregate or union type that includes one of the aforementioned types among its elements or non-static data members (including, recursively, an element or non-static data member of a subaggregate or contained union),
>  - a type that is a (possibly cv-qualified) base class type of the dynamic type of the object,
>  - a char, unsigned char, or std​::​byte type.

Each of these require a subsection of its own.

### The dynamic type of the object

The [_dynamic type_](http://eel.is/c++draft/defns.dynamic.type) of an object
talks about the actual class type of an object in memory, regardless of the type
of the pointer or reference used to access it. It is the type of that object's
[most derived class](http://eel.is/c++draft/intro.object#6). It doesn't seem to
apply to [fundamental types](http://eel.is/c++draft/basic.fundamental).

An example clarifies.

```c++
class Matrix {};
class SparseMatrix : public Matrix {};

void process(Matrix& m)
{
  // The type of `m` is `Matrix`, but as can be seen in `main`, the reference is
  // to an instance of SparseMatrix. The dynamic type of `m` is therefore
  // `SparseMatrix`.
}

void main()
{
  SparseMatrix distances;
  process(distances);
}
```

In relation to strict aliasing, the dynamic part of the rule state that the
compiler must assume that a pointer of type `T*` can point to an object of type
`T`. Are we just stating the obvious?

In the example there is a pointer to `Matrix` that aliases `distances`, which is
of type `SpaseMatrix`. This is allowed because of a later rule.

The fundamental types, which cannot have inheritance, are their own dynamic
types.


### A cv-qualified version of the dynamic type of the objec

[_cv_](http://eel.is/c++draft/basic.type.qualifier) is short for `const
volatile` and a type being cv-qualified means that either `const`, `volative`,
both or neither has been added to the type. For example, in

```c++
void toggle(int& a, const int& b, volatile int& c, const volative int& d);
```

the compiler must assume that any of these references may alias any other in the
parameter list.


### A type similar to the dynamic type of the object

Not sure. It has to do with [types that are a sequence of pointers or arrays]
(http://eel.is/c++draft/conv.qual#def:similar_types), such as `const int **`.


### A type that is the signed or unsigned type corresponding to the dynamic type of the object,

This means that an `int*` may alias an `unsigned int*`.


### A type that is the signed or unsigned type corresponding to a cv-qualified version of the dynamic type of the object

This means that we may mix the cv-rule and the unsigned rule. For example,
`int*` may alias `const unsigned int*`.


### An aggregate or union type that includes one of the aforementioned types among its elements or non-static data members (including, recursively, an element or non-static data member of a subaggregate or contained union)

An [aggregate](http://eel.is/c++draft/dcl.init.aggr) is a POD type containing
elements of other types. There are two types of aggregates: array and class. The
elements of the array are, naturally, the array elements. The elements of an
aggregate class are the direct base classes and the non-static data members.

There are some requirements that a class must fulfill to be an aggregate:

- no user-provided, explicit, or inherited constructors  
  Meaning that instantiating an aggregate class is done by simply initializing the
  aggregates elements. No "custom code" may be run.
- no private or protected non-static data members  
  Meaning that every element must be visible.
- no virtual functions  
  Meaning that the target of any member function call must be known statically.
  I speculate that the reason for this requirement is that there should be no
  hidden virtual table pointer in the class instances.
- no virtual, private, or protected base classes.  
  Continuing the requirement that all members should be known and visible.

[cppreference.com](http://en.cppreference.com/w/cpp/language/reinterpret_cast#Type_aliasing)
say that this rule is a leftover from C and not necessary in C++. In C, aggregate
copy and assignment access the aggregate object as a whole, but in C++ such
actions are always performed through a member function call, which accesses the
individual subobjects rather than the entire object.

An example exemplifies.

```c++
struct System
{
  Matrix L;
  Matrix D;
  Matrix U;
}

void factor(Martix* m, System* s)
{
  // This rule say that 'm' and 's' may alias a 'Matrix' object because 'System'
  // contains a member of type 'Matrix'. If we didn't have this rule
}
```


### A type that is a (possibly cv-qualified) base class type of the dynamic type of the object

This is what makes the `Matrix`/`SparseMatrix` example work. There, the dyanmic
type of the argument to `process` is `SparseMatrix`. `Matrix` is a base class of
that dynamic type, so we are allowed to reference the `SpaceMatrix` through the
`Matrix` reference parameter.

[cppreference.com](http://en.cppreference.com/w/cpp/language/reinterpret_cast#Type_aliasing)
say that this rule is a leftover from C and not necessary in C++. Odd since C
doesn't have inheritance. It a bit vague on the details.


### A char, unsigned char, or std​::​byte type.

We may read and write any type as raw memory, and raw memory in C++ is `char`,
`unsigned char`, and `std::byte`. Not sure why `signed char`, which is different
from both `char` and `unsigned char`, isn't listed.


## A few examples

### Aliasing subobjects

Some matrix library did something like this. Don't remember what exactly, and I
don't remeber which library.

```c++
class Vector4
{
  public:
    float& operator[](int i);
  private:
    float v[4];
};

class Matrix4x4
{
  public:
    Vector4& get_column(int c)
    {
      return *(Vector4*)&m[c];
    }

  private:
    float m[4][4];
};
```

When used in the following way, we get two references to unrelated types
referencing the same bytes in memory so this seems illegal.

```c++
void strange_multiply(Matrix4x4 const& matrix, Vector4& vector)
{
  vector[0] = matrix.get_column(0)[0] * vector[0] +
              matrix.get_column(1)[0] * vector[1] +
              matrix.get_column(2)[0] * vector[2] +
              matrix.get_column(3)[0] * vector[3];

  vector[1] = matrix.get_column(0)[1] * vector[0] +
              matrix.get_column(1)[1] * vector[1] +
              matrix.get_column(2)[1] * vector[2] +
              matrix.get_column(3)[1] * vector[3];

  // And so on.
}

int main()
{
  Matrix4x4 matrix;
  Vector4& column = matrix.get_column(0);
  strange_multiply(matrix, column);
}
```

The compiler is allowed to assume that writes to the vector won't change the
contents of the matrix.


### Custom allocators

This example is about custom allocators. Given that we have somehow accuired a
contigious sequence of bytes, are we allowed to place an object in that memory?
If so, how do one go about doing so without violating the strict aliasion rules?
`unsigned char` is special in the sense that we can read and write any object
through such a pointer, but the inverse it not allowed. I.e., we may not cast a
pointer-to-char to a pointer-to-T and then use that pointer to acces the memory
as-if it was a T.


[GCC Bug 80593](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80593)


```c++
template<unsigned size, unsigned alignment>
struct aligned_storage
{
  union type
  {
    unsigned char __data[size];
    struct __attribute__((__aligned__((alignment)))) {} __align;
  };
};

aligned_storage<sizeof(int), alignof(int)>::type storage;

int main()
{
  *reinterpret_cast<int*>(&storage) = 42;
}
```

This is allowed. Why? There is a class with the same name in the standard
library that is used in a similar way. Is `std::aligned_storage` special, or is
the above listing an equivalent, and equally correct, way to produce a
"typeless" piece of memory?


### Changing type of a memory location

This is from a GCC bug report that I'm unable to find again. It is very similar
to one of the earliest examples in this text. The difference is we here order
the read and writes so that the read data is the one that was last written.

```c++
uint32_t write_write_read(uint32_t* i, float* f)
{
  *f = 1.0f;
  *i = 1u;
  return *i;
} 
```

Reading the aliasing rules one might come to believe that `i` and `f` cannot
alias because they point to the two unrelated types `uint32_t` and `float`.
However, the bug report was filed because the compiler had used the non-aliasing
assumption to reorder the two writes, placing the write through `f` before the
write through `i`. The ticket report submitter had called `write_write_read`
with the same pointer passed to both parameters and got unexpected results.

The reason why the aliasing is legal and the reodering illegal is because of the
`stored value` part of the standard text along with the fact that memory
returned by `malloc` is untyped. The submitter had done the following:


```c++
void work()
{
  void* memory = malloc(4);
  write_write_read(memory, memory);
}
```

What happens is that the memory start out without a type and remain typeless as
we enter `write_write_read`. After the `*f = 1.0f;` line type type of the
poited-to memory has become `float`. We have an `uint32_t*` pointer pointing to
that float which may seem like a violation of the strict aliasing rule, but the
rule doesn't say that we cannot have aliasing pointers of different types. It
says that we can't _access the stored value through a glvalue of the wrong
type_. And we never access the float through the `uint32_t*`. Instead we
_replace_ the `float` with an `uint32_t` with the `*i = 1u` line. The type of
the memory pointed to by the two pointers now changes from `float` to
`uint32_t`. A read through `f` at this point would be a violation of the strict
aliasing rule, but we don't do that. We read through `i`, which is a pointer of
the correct type.

Another bug report of this kind that I could find is [GCC bug
29286](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=29286). It talks about using
placement new instead of malloc'ed memory and assignments to change the type of
a memory location. Especially interesting is [Comment
15](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=29286#c15) and a few comments
forward.

The comment says that we can change the type of any memory location in this way,
not just memory allocated on the free store.

```c++
int32_t i = 0;
// The type of `i` is now `int32_t` and its value is 0.

*(float *)&i = 7.0;
// The type of the memory to which `i` refers is now `float` and its value is 7.0.
// Reading from `i` is undefined behavior because it breaks the strict aliasing rules
// since it's an lvalue of type `int32_t` being used to access a memory location
// holding a `float`.
//
// Some say that this case is only valid if `i` is actually part of a union holding
// both an `int32_t` and a `float`.
//
// The discussion ended before a clear conclusion was reached.
//
// A note is that C does not allow this because `i` has a declared type, and declared
// types cannot change in C.
```

We can also do

```c++
int32_t i = 0;
float* f = new(&i) float;
*f = 7.0;
```

with the same result. There seems to be less controversy for this case.
Placement new changes the dynamic type of the memory given to it.

In C, it seems to be allowed to change the effective type of _allocated memory_,
which I assume means memory returned by `malloc` and friends. That is, the
following is valid C:

```c
void *memory = malloc(4);
int32_t *ip = (uint32_t*)memory;
float *fp = (uint32_t*)memory;

// Here the four bytes have no declared type.

*ip = 1; // The type of the four bytes become 'int32_t`.
// Reading through 'fp' here would be undefined behavior.

*fp = 1.0f; // The type of the four bytes in changed from 'int32_t' to 'float'
// Reading through 'ip' here would be undefined behavior.
```

I do not know if C++ treat memory allocated by `malloc` and friends in the same
way.

[C99 and type-punning – making sense of a broken language specification](https://davmac.wordpress.com/2010/02/26/c99-revisited/)


### Misplaced object

https://stackoverflow.com/questions/33845965/reinterpret-cast-to-aggregate-type-in-c

```c++
struct bar {
  int a;
  float b;
  int c;
};

int main() {
  bar s {1,2,3};
  cout << hex << ((bar*)&s.b)->a << endl;
}
```




### Common initial sequence in union



#include <cassert>

https://gcc.gnu.org/bugzilla/show_bug.cgi?id=65892#c12

```c++
struct t1 { int m; };
struct t2 { int m; };

union U {
    t1 s1;
    t2 s2;
};

int f(t1 *p1, t2 *p2)
{
    if (p2->m < 0)
        p1->m = -p1->m;

    return p2->m;
}

int main (void)
{
    union U u = { { -1 } };

    int n = f (&u.s1, &u.s2);

    assert (1 == n);

    return 0;
}
```

Some say that `p1` and `p2` in `f` may alias, some say it may not.



### Can `size` be cached?

```c++
struct Data
{
    float* elements;
    int32_t size;
};

float mangle(float* ptr); /*
{
    return *ptr * *ptr;
}*/

float sum(Data* data)
{
    float result = 0.0f;
    for (int32_t i = 0; i < data->size; ++i) {
        result += mangle(&data->elements[i]);
    }
    return result;
}


float sum(Data data)
{
    float result = 0.0f;
    for (int32_t i = 0; i < data.size; ++i) {
        result += mangle(&data.elements[i]);
    }
    return result;
}



float sum(const Data& data)
{
    float result = 0.0f;
    for (int32_t i = 0; i < data.size; ++i) {
        result += mangle(&data.elements[i]);
    }
    return result;
}
```



## Resources

[C++ standard draft](http://eel.is/c++draft)

[What is the strict aliasing rule? StackOverflow](https://stackoverflow.com/questions/98650/what-is-the-strict-aliasing-rule)

[Strict Aliasing Rules and Placement New? StackOverflow](https://stackoverflow.com/questions/37230375/strict-aliasing-rules-and-placement-new)

[Understanding strict aliasing](http://cellperformance.beyond3d.com/articles/2006/06/understanding-strict-aliasing.html)

[Compiler Explorer](https://godbolt.org)

[Understanding C by learning assembly](https://www.recurse.com/blog/7-understanding-c-by-learning-assembly)

[Type aliasing on cppreference.com](http://en.cppreference.com/w/cpp/language/reinterpret_cast#Type_aliasing)

[GCC Bug 29286](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=29286)

[GCC Bug 80593](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80593)

[CppCon 2017: Scott Schurr “Type Punning in C++17: Avoiding Pun-defined Behavior”](https://www.youtube.com/watch?v=sCjZuvtJd-k)

[C99 and type-punning – making sense of a broken language specification](https://davmac.wordpress.com/2010/02/26/c99-revisited/)
