#include "CG_NativeOP.h"

const bool isCommutative[14] = {true,  false, true, false, true,  true,  true,
                              false, false, true, true,  false, false, false};
const char* nativeOpToOpcode[14] = {"add", "sub", "mul",  "and",  "div",  "or",  "xor",
                                  "shl", "shr", "mulh", "mulq", "invq", "mov", "not"};

typedef enum
{
    RZ = 0xFFFF,
    R0 = 0,
    R1,
    R2,
    R3,
    R4,
    R5,
    R6,
    R7,
    Literal,
    SPRelative,
    SP,
    Addr_None,
} UsedAddr;

bool IsValidOperation(const Value* dst, const Value* srcA, const Value* srcB)
{
    bool threeOperands = false;

    if (!Value_Equals(dst, srcA))
        threeOperands = true;

    bool usesLiteral = false;

    UsedAddr addr = Addr_None;
    uint16_t usedLiteral = 0;

    const Value* operands[3] = {srcA, srcB, dst};

    size_t len = 2;
    if (threeOperands)
        len = 3;
    for (size_t i = 0; i < len; i++)
    {
        const Value* op = operands[i];
        if (op->addressType == AddressType_Memory)
        {
            if (usesLiteral && (usedLiteral != (int16_t)op->address))
                return false;
            if (addr != Addr_None && addr != Literal)
                return false;
            usesLiteral = true;
            addr = Literal;
            usedLiteral = (uint16_t)op->address;
        }
        else if (op->addressType == AddressType_Literal)
        {
            if (usesLiteral && usedLiteral != (int16_t)op->address)
                return false;
            usesLiteral = true;
            usedLiteral = (uint16_t)op->address;
        }
        else if (op->addressType == AddressType_MemoryRegister)
        {
            UsedAddr newAddr = Value_GetR0((Value*)op);
            if (addr != Addr_None && addr != newAddr)
                return false;
            addr = newAddr;
        }
        else if (op->addressType == AddressType_MemoryRelative)
        {
            int delta = Stack_GetOffset() + (int)op->address;
            // assert(delta >= 0);
            if (delta < 0 || delta > 255)
                return false;
            if (delta == 0)
            {
                if (addr != Addr_None && addr != SP)
                    return false;
                addr = SP;
            }
            else
            {
                if (usesLiteral && usedLiteral != delta)
                    return false;
                if (addr != Addr_None && addr != SPRelative)
                    return false;
                usesLiteral = true;
                addr = SPRelative;
                usedLiteral = (uint16_t)delta;
            }
        }

        if (((usedLiteral & (~0xFF)) || addr == Literal) && threeOperands)
            return false;

        // Can't use SP-relative addressing and the unmodified value of SP at the same time.
        if (addr == SPRelative && ((srcA->addressType == AddressType_Register && ((int)srcA->address) == Register_SP) ||
                                   (srcB->addressType == AddressType_Register && ((int)srcB->address) == Register_SP)))
            return false;
    }
    return true;
}

void GenerateValidOperation(const Value* dst, const Value* srcA, const Value* srcB, NativeOP op)
{
    assert(IsValidOperation(dst, srcA, srcB));

    OutWrite(nativeOpToOpcode[op]);
    OutWrite(" ");

    if (dst->address != srcA->address || dst->addressType != srcA->addressType || dst->size != srcA->size)
    {
        PrintValueAsOperand(dst);
        OutWrite(", ");
    }
    PrintValueAsOperand(srcA);
    OutWrite(", ");
    PrintValueAsOperand(srcB);
    OutWrite("\n");
}

void GenerateNativeOP(NativeOP op, Value* oValue, Value left, Value right, bool leftReadOnly, bool rightReadOnly)
{
    assert(op >= 0 && op <= NativeOP_Not);

    if (left.addressType == AddressType_Literal && left.address == 0)
        left = Value_FromRegister(-1);
    if (right.addressType == AddressType_Literal && right.address == 0)
        right = Value_FromRegister(-1);

    bool usingRequestedOutputValue = false;
    // If a destination has been requested, we use that, unless it is a flag.
    // Otherwise, we try to reuse one of the src values as dst,
    // if they're not passed as read only. If that doesn't work,
    // we allocate a new register.
    if (oValue->addressType == AddressType_None || oValue->addressType == AddressType_Flag)
    {
        if (!leftReadOnly && left.addressType != AddressType_Literal &&
            left.addressType != AddressType_MemoryRegister &&
            !(left.addressType == AddressType_Register && left.address == -1))
        {
            *oValue = left;
        }
        else if (!rightReadOnly && isCommutative[op] && right.addressType != AddressType_Literal &&
                 right.addressType != AddressType_MemoryRegister &&
                 !(right.addressType == AddressType_Register && right.address == -1))
        {
            Value temp = left;
            left = right;
            right = temp;
            *oValue = left;

            bool t = leftReadOnly;
            leftReadOnly = rightReadOnly;
            rightReadOnly = t;
        }
        else
            *oValue = Value_Register(1);
    }
    else
    {
        if (Value_Equals(&right, oValue) && isCommutative[op])
        {
            Value temp = left;
            left = right;
            right = temp;

            bool t = leftReadOnly;
            leftReadOnly = rightReadOnly;
            rightReadOnly = t;
        }

        usingRequestedOutputValue = true;
    }

    // (This is not strictly necessary, but if a sp-relative
    // source is used, but can't be reached via the sp-relative
    // addressing mode, we manually move the sp there here.
    // This is faster than what would happen if this wasn't here:
    // The sp would be moved there AND it would be copied into a register.)
    if (oValue->addressType == AddressType_MemoryRelative)
    {
        int delta = Stack_GetOffset() + (int)oValue->address;
        if (delta > 255 || delta < 0)
            Stack_ToAddress((int)oValue->address);
    }
    else if (right.addressType == AddressType_MemoryRelative)
    {
        int delta = Stack_GetOffset() + (int)right.address;
        if (delta > 255 || delta < 0)
            Stack_ToAddress((int)right.address);
    }
    else if (left.addressType == AddressType_MemoryRelative)
    {
        int delta = Stack_GetOffset() + (int)left.address;
        if (delta > 255 || delta < 0)
            Stack_ToAddress((int)left.address);
    }

    Value temp = NullValue;
    Value oldOValue = NullValue;

    // TODO promote smarter -> check if two values equal, if so, promote the third

    // If this constellation of operators would be an invalid instruction
    // (eg two literals in one instruction),
    // we successively promote (copy) operators to registers.
    while (!IsValidOperation(oValue, &left, &right))
    {
        // Spaghetti condition here means that right is only promoted first if it is the best choice:
        // We skip right (for now) if
        //     left doesn't equal the output value (that would mean 2-operand instr)
        //     left is neither a Register nor a Literal, so in Memory
        //     right is a literal
        if (right.addressType != AddressType_Register && !Value_Equals(oValue, &right) &&
            !(!Value_Equals(&left, oValue) && left.addressType != AddressType_Literal &&
              left.addressType != AddressType_Register && right.addressType == AddressType_Literal))
        {
            Value new = Value_Register(1);
            Value_GenerateMemCpy(new, right);
            if (!usingRequestedOutputValue && Value_Equals(oValue, &right))
                *oValue = new;
            if (!rightReadOnly)
                Value_FreeValue(&right);
            right = new;
            rightReadOnly = false;
        }
        else if (left.addressType != AddressType_Register)
        {
            Value new = Value_Register(1);
            Value_GenerateMemCpy(new, left);
            if (!usingRequestedOutputValue && Value_Equals(oValue, &left))
                *oValue = new;
            if (!leftReadOnly)
                Value_FreeValue(&left);
            left = new;
            leftReadOnly = false;
        }
        else if (oValue->addressType == AddressType_Memory)
        {
            temp = Value_Register(1);
            OutWrite("mov r%i, %i\n", Value_GetR0(&temp), oValue->address);
            temp.addressType = AddressType_MemoryRegister;
            temp.size = oValue->size;
            oldOValue = *oValue;
            *oValue = temp;
        }
        else if (oValue->addressType == AddressType_MemoryRelative)
        {
            Stack_ToAddress((int)oValue->address);
        }
        else
            assert(0);
    }

    GenerateValidOperation(oValue, &left, &right, op);

    if (temp.addressType != AddressType_None)
    {
        Value_FreeValue(&temp);
        *oValue = oldOValue;
    }

    if (!leftReadOnly && (!Value_Equals(oValue, &left)))
        Value_FreeValue(&left);
    if (!rightReadOnly && (!Value_Equals(oValue, &right)))
        Value_FreeValue(&right);

    return;
}