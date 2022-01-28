#include "Stack.h"
#include "Outfile.h"

#include <stdio.h>

static int curFuncStackSize = 0;
static int curStackPointerOffset = 0;
void Stack_Align()
{
    if (curStackPointerOffset > 0)
        OutWrite("sub sp, %i\n", curStackPointerOffset);
    if (curStackPointerOffset < 0)
        OutWrite("add sp, %i\n", -curStackPointerOffset);

    curStackPointerOffset = 0;
}
void Stack_ToAddress(int addr)
{
    int delta = curStackPointerOffset + addr;

    if (delta > 0)
        OutWrite("sub sp, %i\n", delta);
    else if (delta < 0)
        OutWrite("add sp, %i\n", -delta);

    curStackPointerOffset = -addr;
}
void Stack_Reset()
{
    int delta = curFuncStackSize + curStackPointerOffset;
    if (delta > 0)
        OutWrite("sub sp, %i\n", delta);
    else if (delta < 0)
        OutWrite("add sp, %i\n", -delta);

    curStackPointerOffset = -curFuncStackSize;
}
int Stack_GetDelta(int addr)
{
    return curStackPointerOffset + addr;
}
void Stack_SetSize(int n)
{
    curFuncStackSize = n;
}
void Stack_SetOffset(int n)
{
    curStackPointerOffset = n;
}
int Stack_GetSize()
{
    return curFuncStackSize;
}
int Stack_GetOffset()
{
    return curStackPointerOffset;
}
void Stack_Offset(int n)
{
    curStackPointerOffset += n;
}
void Stack_OffsetSize(int n)
{
    curFuncStackSize += n;
}