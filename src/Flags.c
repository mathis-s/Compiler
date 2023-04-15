#include "Flags.h"
#include <assert.h>
#include <stdlib.h>

static const char* flagToString[8] = {
    "_nz", "_z", "_np", "_p", "_ns", "_s", "_nc", "_c",
};

static Flag lastExpressionAsBoolInFlag;

void Flags_SetExpressionAsBoolInFlag(Flag f)
{
    lastExpressionAsBoolInFlag = f;
}

void Flags_ClearResult()
{
    lastExpressionAsBoolInFlag = Flag_None;
}

Flag Flags_GetResultAsFlag()
{
    return lastExpressionAsBoolInFlag;
}

const char* Flags_FlagToString(Flag f)
{
    assert(f <= Flag_C && f >= Flag_NZ);
    return flagToString[f];
}
Flag Flags_Invert(Flag f)
{
    assert(f <= Flag_C && f >= Flag_NZ);

    if ((f % 2) == 0)
        return f + 1;
    else
        return f - 1;
}