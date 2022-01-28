# Compiler
A minimal C-Compiler for my 16-bit RISC Architecture.

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

## Limitations
This is not a standard C-Compiler! Here are the most important things that are missing or unusual
- Preprocessor only supports #include, #define, #ifdef, and #pragma once
- Parsing is not quite the same as in standard C
- Structs are CPP-style. `typedef struct { } ...;` definitions are also supported.
- There's no actual typedef though!
- No unions, bitfields, etc. Only structs, enums, arrays, pointers and primitives
- Primitives are:
  - `int` (alias `int16`, `int16_t`)
  - `uint` (alias `uint16`, `uint16_t`, `size_t`, `char`)
  - `int32` (alias `int32_t`)
  - `uint32` (alias `uint32_t`)
  - `fixed` (8.8 fixed point)
  
These are just off the top of my head, there surely are many others!

## Notes
This is my first major project in C, so please let me know if you have thoughts on improving the structure of the program, or anything else! 

Some weirdness is also due to the limitations of the compiler right now, and me still wanting to have it be able to compile itself, though.
