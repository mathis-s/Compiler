#pragma once
#include "../Outfile.h"
#include "../Value.h"

typedef enum
{
    NativeOP_Add = 0,
    NativeOP_Sub,
    NativeOP_Mul,
    NativeOP_And,
    NativeOP_Div,
    NativeOP_Or,
    NativeOP_Xor,
    NativeOP_ShiftLeft,
    NativeOP_ShiftRight,
    NativeOP_MulH,
    NativeOP_MulQ,
    NativeOP_InvQ,
    NativeOP_Mov,
    NativeOP_Not,

} NativeOP;

// Checks if a single-instruction native op using the dst, srcA and srcB
// would be valid. If so, the instruction can be generated; otherwise,
// it is split up into two or more instructions.
void GenerateNativeOP(NativeOP op, Value* oValue, Value left, Value right, bool leftReadOnly, bool rightReadOnly);