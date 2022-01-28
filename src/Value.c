#include "Value.h"
#include "Outfile.h"
#include "Register.h"
#include "Stack.h"
#include "Variables.h"

#include <stdbool.h>

int Value_GetR0(const Value* v)
{
    assert(v->addressType == AddressType_Register || v->addressType == AddressType_MemoryRegister);
    return ((uint16_t)v->address) % 8;
}

int Value_GetR1(const Value* v)
{
    assert(v->addressType == AddressType_Register || v->addressType == AddressType_MemoryRegister);
    assert(v->address != (int32_t)(-1));
    if (v->size < 2)
        return -1;
    return ((uint16_t)v->address) / 8;
}

void Value_ValueAddrToStackPointer(const Value* v)
{
    assert(v->addressType == AddressType_MemoryRelative);
    Stack_ToAddress((int)v->address);
}

Value Value_FromRegister(int r0)
{
    return (Value){(int32_t)r0, AddressType_Register, 1};
}

Value Value_FromRegisters(int r0, int r1)
{
    return (Value){(int32_t)(r0 + r1 * 8), AddressType_Register, 2};
}

Value Value_Register(int size)
{
    if (size == 1)
    {
        return Value_FromRegister(Registers_GetFree());
    }
    if (size == 2)
    {
        return Value_FromRegisters(Registers_GetFree(), Registers_GetFree());
    }
    assert(false);
}

Value Value_GetLowerWord(const Value* value)
{
    assert(value->addressType == AddressType_Literal || value->size == 2 ||
           (value->addressType == AddressType_Register && value->address == -1));
    Value retval = *value;
    retval.size = 1;
    switch (retval.addressType)
    {
        case AddressType_Literal:
            retval.address &= 0xFFFF;
            break;
        case AddressType_Register:
            if (retval.address != -1)
                retval.address = (int32_t)(((int16_t)retval.address) % 8);
            break;
        default:
            break;
    }
    return retval;
}

Value Value_GetUpperWord(const Value* value, bool* oReadOnly)
{
    assert(value->addressType == AddressType_Literal || value->size == 2 ||
           (value->addressType == AddressType_Register && value->address == -1));
    Value retval = *value;

    switch (retval.addressType)
    {
        case AddressType_Register:
            if (retval.address != -1)
                retval.address = (int32_t)(((int16_t)retval.address) / 8);
            *oReadOnly = true;
            break;
        case AddressType_Memory:
            retval.address += 1;
            *oReadOnly = true;
            break;
        case AddressType_Literal:
#ifndef CUSTOM_COMP
            retval.address >>= 16;
#endif
            *oReadOnly = true;
            break;
        case AddressType_MemoryRegister:
            retval = Value_Register(1);
            OutWrite("add r%i, r%i, 1\n", Value_GetR0(&retval), Value_GetR0(value));
            *oReadOnly = false;
            break;
        case AddressType_MemoryRelative:
            retval.address -= 1;
            *oReadOnly = true;
            break;
        default:
            assert(0);
    }

    retval.size = 1;
    return retval;
}

void Value_FreeValue(Value* value)
{
    if (value->addressType == AddressType_Register || value->addressType == AddressType_MemoryRegister)
    {
        assert(Value_GetR0(value) != -1);
        Register_Free(Value_GetR0(value));
        if (value->size == 2 && value->addressType != AddressType_MemoryRegister)
            Register_Free(Value_GetR1(value));
        if (value->addressType == AddressType_Register)
            assert(value->size <= 2);
    }

    // Not freeing anything from the stack anymore unless scope ends!

    // (We only free values on top of the stack, for lower values we would have to
    // shift everything above down which takes too long. Still very useful to remove
    // temporary stack allocations.)
    /*if(value->addressType == AddressType_MemoryRelative && value->address == value->size)
    {
        ShiftAddressSpace(variables, -value->size);
        curStackPointerOffset += value->size;
        curFuncStackSize -= value->size;
    }*/
}

Value Value_MemoryRelative(int addr, int size)
{
    return (Value){(int32_t)addr, AddressType_MemoryRelative, size};
}

Value Value_Memory(uint16_t addr, int size)
{
    return (Value){(int32_t)addr, AddressType_Memory, size};
}

Value Value_Literal(int32_t literal)
{
#ifndef CUSTOM_COMP
    return (Value){literal, AddressType_Literal, (uint32_t)literal > 0xFFFF ? 2 : 1};
#endif
// TODO replace this with ternary once implemented.
#ifdef CUSTOM_COMP
    int size;
    if (literal > 0xFFFF)
        size = 2;
    else
        size = 1;
    return (Value){literal, AddressType_Literal, size};
#endif
}

void Value_GenerateMemCpy(Value dstValue, Value srcValue)
{
    if (Value_Equals(&dstValue, &srcValue))
        return;

    // We can only quickly access values below the sp, so it make sense to move it to the top of the stack
    // when a value is above it.
    // if ((srcValue.addressType == AddressType_MemoryRelative && GetStackPointerDelta(srcValue.address) < 0) ||
    //    (dstValue.addressType == AddressType_MemoryRelative && GetStackPointerDelta(dstValue.address) < 0))
    //   AlignStack();

    // Quickly handle Literals
    if (srcValue.addressType == AddressType_Literal)
    {
        if (dstValue.addressType == AddressType_Register)
        {
            OutWrite("mov r%i, %i\n", Value_GetR0(&dstValue), srcValue.address & 0xFFFF);
            if (dstValue.size == 2)
                OutWrite("mov r%i, %i\n", Value_GetR1(&dstValue), srcValue.address >> 16);
        }
        else if (dstValue.addressType == AddressType_Memory)
        {
            int temp = Registers_GetFree();
            OutWrite("mov r%i, %i\n", temp, srcValue.address & 0xFFFF);
            OutWrite("mov [%i], r%i\n", dstValue.address, temp);
            if (dstValue.size == 2)
            {
                OutWrite("mov r%i, %i\n", temp, srcValue.address & 0xFFFF);
                OutWrite("mov [%i], r%i\n", dstValue.address + 1, srcValue.address >> 16);
            }
            Register_Free(temp);
        }

        else if (dstValue.addressType == AddressType_MemoryRelative)
        {

            Stack_ToAddress((int)dstValue.address);
            OutWrite("mov [sp++], %i\n", srcValue.address & 0xFFFF);
            Stack_Offset(1);
            if (dstValue.size == 2)
            {
                OutWrite("mov [sp++], %i\n", srcValue.address >> 16);
                Stack_Offset(1);
            }
        }
        else if (dstValue.addressType == AddressType_MemoryRegister)
        {
            OutWrite("mov [r%i], %i\n", Value_GetR0(&dstValue), srcValue.address & 0xFFFF);
            if (dstValue.size == 2)
            {
                OutWrite("add r%i, 1\n", Value_GetR0(&dstValue));
                OutWrite("mov [r%i], %i\n", Value_GetR0(&dstValue), srcValue.address >> 16);
                OutWrite("sub r%i, 1\n", Value_GetR0(&dstValue));
            }
        }
        else
            Error("Internal error!");
        return;
    }

    int size = dstValue.size;
    if (srcValue.size < size)
        size = srcValue.size;
    int regs[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    int block_size;

    if (srcValue.addressType == AddressType_Register)
    {
        block_size = srcValue.size;
        regs[0] = Value_GetR0(&srcValue);
        if (srcValue.size == 2)
            regs[1] = Value_GetR1(&srcValue);
    }
    else if (dstValue.addressType == AddressType_Register)
    {
        block_size = dstValue.size;
        regs[0] = Value_GetR0(&dstValue);
        if (dstValue.size == 2)
            regs[1] = Value_GetR1(&dstValue);
    }
    else
    {
        // block size > 1 only really necessary when we're copying from/to a far away location on the stack
        if ((srcValue.addressType == AddressType_MemoryRelative && Stack_GetDelta((int)srcValue.address) >= 256) ||
            (dstValue.addressType == AddressType_MemoryRelative && Stack_GetDelta((int)dstValue.address) >= 256))
        {

            block_size = Registers_GetNumFree();
            if (block_size > size)
                block_size = size;
        }
        else
            block_size = 1;

        assert(block_size >= 1);
        for (int i = 0; i < block_size; i++)
            regs[i] = Registers_GetFree();
    }

    for (int i = 0; i < size; NULL)
    {
        // if (srcValue.addressType == AddressType_MemoryRelative)
        //     StackPointerToAddress(srcValue.address - i);

        if (srcValue.addressType != AddressType_Register)
            for (int j = 0; j < block_size; j++)
            {
                if (i + j >= size)
                    break;
                if (srcValue.addressType == AddressType_MemoryRelative)
                {
                    int delta = Stack_GetDelta((int)srcValue.address);
                    if (delta >= 0 && delta <= 255)
                    {
                        OutWrite("mov r%i, [sp-%i]\n", regs[j], delta);
                    }
                    else
                    {
                        Stack_ToAddress((int)srcValue.address);
                        if (size > 1)
                        {
                            OutWrite("mov r%i, [sp++]\n", regs[j]);
                            Stack_Offset(1);
                        }
                        else
                        {
                            OutWrite("mov r%i, [sp]\n", regs[j]);
                        }
                    }
                    (srcValue.address)--;
                }
                else if (srcValue.addressType == AddressType_MemoryRegister)
                {
                    OutWrite("mov r%i, [r%i]\n", regs[j], Value_GetR0(&srcValue));
                    if (size - i - j != 1)
                        OutWrite("add r%i, 1\n", Value_GetR0(&srcValue));
                }
                else if (srcValue.addressType == AddressType_Memory)
                {
                    OutWrite("mov r%i, [%i]\n", regs[j], srcValue.address++);
                }
            }

        // if (dstValue.addressType == AddressType_MemoryRelative)
        //     StackPointerToAddress(dstValue.address - i);

        if (dstValue.addressType != AddressType_Register || srcValue.addressType == AddressType_Register)
            for (int j = 0; j < block_size; j++)
            {
                if (i + j >= size)
                    break;
                if (dstValue.addressType == AddressType_MemoryRelative)
                {
                    int delta = Stack_GetDelta((int)dstValue.address);
                    if (delta > 0 && delta <= 255)
                    {
                        OutWrite("mov [sp-%i], r%i\n", delta, regs[j]);
                    }
                    else
                    {
                        Stack_ToAddress((int)dstValue.address);
                        // if (size > 1)
                        {
                            OutWrite("mov [sp++], r%i\n", regs[j]);
                            Stack_Offset(1);
                        }
                        /*else
                        {
                            OutWrite("mov [sp], r%i\n", regs[j]);
                        }*/
                    }
                    dstValue.address--;
                }
                else if (dstValue.addressType == AddressType_MemoryRegister)
                {
                    OutWrite("mov [r%i], r%i\n", Value_GetR0(&dstValue), regs[j]);
                    if (size - i - j != 1)
                        OutWrite("add r%i, 1\n", Value_GetR0(&dstValue));
                }
                else if (dstValue.addressType == AddressType_Memory)
                {
                    OutWrite("mov [%i], r%i\n", dstValue.address++, regs[j]);
                }
                else if (dstValue.addressType == AddressType_Register)
                {
                    if (j == 0)
                        OutWrite("mov r%i, r%i\n", Value_GetR0(&dstValue), regs[j]);
                    else if (j == 1)
                        OutWrite("mov r%i, r%i\n", Value_GetR1(&dstValue), regs[j]);
                }
            }
        i += block_size;
    }

    // TODO this is unnecessaty for read-only
    if (srcValue.addressType == AddressType_MemoryRegister && size > 1)
        OutWrite("sub r%i, %i\n", Value_GetR0(&srcValue), size - 1);

    if (dstValue.addressType == AddressType_MemoryRegister && size > 1)
        OutWrite("sub r%i, %i\n", Value_GetR0(&dstValue), size - 1);

    // If we allocated registers previously, we free them
    if (srcValue.addressType != AddressType_Register && dstValue.addressType != AddressType_Register)
        for (int i = 0; i < block_size; i++)
            Register_Free(regs[i]);
}

void Value_Push(Value* value)
{
    Stack_Align();
    if (value->addressType == AddressType_Register)
    {
        OutWrite("mov [sp++], r%i\n", Value_GetR0(value));
        if (value->size == 2)
            OutWrite("mov [sp++], r%i\n", Value_GetR1(value));

        assert(value->size == 1 || value->size == 2);
    }
    else if (value->addressType == AddressType_Memory)
    {
        int tmp = Registers_GetFree();
        int32_t addr = value->address;
        for (int i = 0; i < value->size; i++)
        {
            OutWrite("mov r%i, [%i]\n", tmp, addr++);
            OutWrite("mov [sp++], r%i\n", tmp);
        }
        Register_Free(tmp);
    }
    else if (value->addressType == AddressType_MemoryRelative || value->addressType == AddressType_MemoryRegister)
    {
        Value dst = Value_MemoryRelative(0, value->size);
        Value_GenerateMemCpy(dst, *value);

        Stack_Offset(-value->size);
    }
    else if (value->addressType == AddressType_Literal)
    {
        assert(value->size == 1 || value->size == 2);
        OutWrite("mov [sp++], %i\n", value->address & 0xFFFF);
        if (value->size == 2)
            OutWrite("mov [sp++], %i\n", value->address >> 16);
    }

    Stack_OffsetSize(value->size);
}

void Value_MemSet(Value* value, uint16_t n)
{
    if (value->addressType == AddressType_Register)
    {
        OutWrite("mov r%i, %i\n", Value_GetR0(value), n);
        if (value->size == 2)
            OutWrite("mov r%i, %i\n", Value_GetR1(value), n);
    }

    if (value->addressType == AddressType_Memory)
    {
        for (int i = 0; i < value->size; i++)
            OutWrite("mov [%i], %i\n", value->address + (int32_t)i, n);
    }

    if (value->addressType == AddressType_MemoryRelative)
    {
        Stack_ToAddress((int)value->address);

        for (int i = 0; i < value->size; i++)
        {
            OutWrite("mov [sp++], %i\n", n);
            Stack_Offset(1);
        }
    }
}

void PrintValueAsOperand(const Value* val)
{
    switch (val->addressType)
    {
        case AddressType_Register:

            // TODO put this in the switch, once the implemenation
            // doesn't create a jump table from 8 to 0xFFFF
            if (val->address == -1)
            {
                OutWrite("rz"); // Zero register
                return;
            }

            switch ((int)val->address)
            {
                case Register_IP:
                    OutWrite("ip");
                    return;
                case Register_SP:
                    OutWrite("sp");
                    return;
                default:
                    OutWrite("r%i", val->address);
                    return;
            }
            break;
        case AddressType_Memory:
            OutWrite("[%i]", val->address);
            break;
        case AddressType_MemoryRegister:
            OutWrite("[r%i]", val->address);
            break;
        case AddressType_MemoryRelative:;
            int delta = Stack_GetOffset() + (int)val->address;
            assert(delta >= 0);

            if (delta == 0 && val->address == 0)
            {
                // If the address is at the top of the stack, increment
                // the sp as well, to keep the stack aligned.
                OutWrite("[sp++]", delta);
                Stack_Offset(1);
            }
            else if (delta == 0)
                OutWrite("[sp]", delta);
            else
                OutWrite("[sp-%i]", delta);
            break;
        case AddressType_Literal:
            OutWrite("%i", val->address);
            break;
        default:
            assert(0);
    }
}

// Stores whether the value is true or false in the zero flag
// Used for branches.
void Value_ToFlag(Value* value)
{
    if (value->addressType == AddressType_Register)
    {
        if (value->size == 1)
            OutWrite("add r%i, 0\n", Value_GetR0(value));
        else if (value->size == 2)
        {
            int temp = Registers_GetFree();
            OutWrite("mov r%i, r%i\n", temp, Value_GetR0(value));
            OutWrite("or r%i, r%i\n", temp, Value_GetR1(value));
            Register_Free(temp);
        }
        return;
    }
    if (value->addressType == AddressType_Memory)
    {
        if (value->size == 1)
            OutWrite("add [%i], rz\n", value->address);
        else if (value->size == 2)
        {
            int temp = Registers_GetFree();
            OutWrite("mov r%i, [%i]\n", temp, value->address);
            OutWrite("or r%i, [%i]\n", temp, value->address + (int32_t)1);
            Register_Free(temp);
        }
        return;
    }
    if (value->addressType == AddressType_MemoryRegister)
    {
        if (value->size == 1)
            OutWrite("add [r%i], 0\n", Value_GetR0(value));
        else if (value->size == 2)
        {
            int temp = Registers_GetFree();
            OutWrite("mov r%i, [r%i]\n", temp, Value_GetR0(value));
            OutWrite("add r%i, 1\n", Value_GetR0(value));
            OutWrite("or r%i, [r%i]\n", temp, Value_GetR0(value));
            OutWrite("sub r%i, 1\n", Value_GetR0(value));
            Register_Free(temp);
        }
        return;
    }
    if (value->addressType == AddressType_MemoryRelative)
    {
        if (value->size == 1)
        {
            Stack_ToAddress((int)value->address);
            OutWrite("add [sp], 0\n");
        }
        else if (value->size == 2)
        {
            Stack_ToAddress((int)value->address);
            int temp = Registers_GetFree();
            OutWrite("mov r%i, [sp]\n", temp);
            Stack_ToAddress((int)value->address + 1);
            OutWrite("or r%i, [sp]\n", temp);
            Register_Free(temp);
        }
        return;
    }

    if (value->addressType == AddressType_Literal)
    {
        if (value->address != 0)
            OutWrite("add rz, 1\n");
        else
            OutWrite("add rz, 0\n");
        return;
    }

    Error("Cannot use type as bool!");
}

bool Value_Equals(const Value* a, const Value* b)
{
    return a->address == b->address && a->addressType == b->addressType && a->size == b->size;
}

Value Value_Flag(Flag f)
{
    return (Value){(int32_t)f, AddressType_Flag, -1};
}
