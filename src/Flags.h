#pragma once
typedef enum
{
    Flag_NZ,
    Flag_Z,
    Flag_NP,
    Flag_P,
    Flag_NS,
    Flag_S,
    Flag_NC,
    Flag_C,
    Flag_None,
} Flag;

void Flags_SetExpressionAsBoolInFlag(Flag f);
void Flags_ClearResult();
Flag Flags_GetResultAsFlag();
const char* Flags_FlagToString(Flag f);
Flag Flags_Invert(Flag f);