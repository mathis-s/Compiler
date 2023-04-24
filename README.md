# Compiler
A minimal C compiler for my custom 16-bit RISC architecture, capable of compiling itself.

## Example

<table>
<tr>
<th>Source</th>
<th>Assembly</th>
</tr>
<tr>
<td>
  
```c
struct coord
{
    int x;
    int y;
};

int foo (int n, struct coord* coords)
{
    int sum = 0;
    for (int i = 0; i < n; i++)
        sum += coords[i].x * coords[i].y;

    return sum;
}
```
  
</td>
<td>

```asm
_foo:
mov r0, 0
mov r1, 0
for_loop0:
sub rz, r1, [sp-2]
jmp_ns for_break0
mul r2, r1, 2
add r2, [sp-3]
mul r3, r1, 2
add r3, [sp-3]
add r3, 1
mov r5, [r3]
mul r4, [r2], r5
add r0, r4
for_continue0:
add r1, 1
jmp for_loop0
for_break0:
mov [sp-2], r0
sub sp, 1
mov ip, [sp]
```

</td>
</tr>
</table>

## Limitations
This is not a conformant C compiler (at all!). Here are the most important things that are missing or different
- Type conversion behaviour is different
- Parsing is not quite the same as in standard C
- Preprocessor only supports #include, #define, #ifdef, and #pragma once
- Primitives are:
  - `int` (alias `int16`, `int16_t`)
  - `uint` (alias `uint16`, `uint16_t`, `size_t`, `char`)
  - `int32` (alias `int32_t`)
  - `uint32` (alias `uint32_t`)
  - `fixed` (8.8 fixed point)
  
These are just off the top of my head, there surely are many others!

## Building
Just a CMake project, so 

```
> mkdir bin
> cd bin
> cmake ..
```
followed by
```
> cmake --build .
```

## Usage
There are no command-line switches as of now, so just
```
> ./comp [SOURCE FILES..]
```
This will generate assembly in `out.s` and data in `data.bin`.
