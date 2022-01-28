#include "Register.h"
#include "Function.h"
#include "Outfile.h"
#include "Stack.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t usedRegisters = (uint16_t)0;
const int NUM_REGISTERS = 8;
uint16_t preferredRegisters[8];

int16_t Registers_GetUsed()
{
    return usedRegisters;
}

// TODO make this pointer to array when supported
void Registers_SetPreferred(const uint16_t* prefRegs)
{
    memcpy(&preferredRegisters[0], prefRegs, sizeof(uint16_t) * 8);
}

int Registers_GetFree()
{
    int r = -1;
    uint16_t lastScore = 0;
    Function* const f = Function_GetCurrent();

    // Find most preferred register
    for (int i = 0; i < NUM_REGISTERS; i++)
        if (!(usedRegisters & (1 << i)) && preferredRegisters[i] > lastScore)
        {
            r = i;
            lastScore = preferredRegisters[i];
        }

    assert(r != -1);
    usedRegisters |= (1 << r);
    f->modifiedRegisters |= (1 << r);
    return r;
}
void Register_Free(int r)
{
    if (r == -1)
        return;
    usedRegisters &= ~(1 << r);
}
void Register_GetSpecific(int r)
{
    assert(!(usedRegisters & (1 << r)));
    usedRegisters |= (1 << r);
}
void Registers_FreeAll()
{
    usedRegisters = 0;
}
uint16_t Registers_PushAllUsed(int* num)
{
    for (int i = 0; i < NUM_REGISTERS; i++)
    {
        if ((usedRegisters & (1 << i)) != 0)
        {
            OutWrite("mov [sp++], r%i\n", i);
            if (num != NULL)
                (*num)++;
        }
    }
    return usedRegisters;
}
// Only pushes registers that have a 1 in the mask
uint16_t Registers_PushAllUsedMasked(int* num, uint16_t mask)
{
    *num = 0;
    for (int i = 0; i < NUM_REGISTERS; i++)
    {
        if ((usedRegisters & (1 << i)) && (mask & (1 << i)))
        {
            OutWrite("mov [sp++], r%i\n", i);
            if (num != NULL)
                (*num)++;
        }
    }
    return usedRegisters & mask;
}

// Get number of registers that are not modified by any function
// call in this scope, and as such can be used freely w/o needing to
// be pushed.
int Registers_GetNumPreferred()
{
    int num = 0;
    for (int i = 0; i < NUM_REGISTERS; i++)
    {
        if (!(usedRegisters & (1 << i)) && (preferredRegisters[i] == 0xFFFF))
            num++;
    }
    return num;
}

int Registers_GetNumUsed()
{
    int num = 0;
    for (int i = 0; i < NUM_REGISTERS; i++)
    {
        if ((usedRegisters & (1 << i)) != 0)
        {
            num++;
        }
    }
    return num;
}
// Only counts registers that have a 1 in the mask
int Registers_GetNumUsedMasked(uint16_t mask)
{
    int num = 0;
    for (int i = 0; i < NUM_REGISTERS; i++)
    {
        if ((usedRegisters & (1 << i)) && (mask & (1 << i)))
        {
            num++;
        }
    }
    return num;
}
int Registers_GetNumFree()
{
    int num = 0;
    for (int i = 0; i < NUM_REGISTERS; i++)
    {
        if ((usedRegisters & (1 << i)) == 0)
        {
            num++;
        }
    }
    return num;
}
void Registers_Pop(uint16_t pushedRegisters, int fromAddr)
{
    int offset = 1 + fromAddr;
    for (int i = NUM_REGISTERS - 1; i >= 0; i--)
    {
        if ((pushedRegisters & (1 << i)) != 0)
        {
            OutWrite("mov r%i, [sp-%u]\n", i, offset++);

            // check if any of the lower bits are set
            // if so, another register is going to be popped,
            // so we need to generate a nop
            // if ((usedRegisters & ((1 << i) - 1)) != 0) OutWrite("nop\n");
        }
    }
    usedRegisters |= pushedRegisters;
    // OutWrite("sub sp, %i\n", offset - 1);
    // OffsetStackPointer((offset - 1));
}
