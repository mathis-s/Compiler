#pragma once
#include "GenericList.h"
#include <stdint.h>

#ifndef CUSTOM_COMP
typedef struct VariableType VariableType;
#endif
#ifdef CUSTOM_COMP
typedef struct VariableType {} VariableType;
#endif

typedef struct Function
{
    char* identifier;
    GenericList parameters;
    VariableType* returnType;
    uint16_t modifiedRegisters;
    bool variadicArguments;
    bool isForwardDecl;
} Function;

Function* Function_GetCurrent();
void Function_SetCurrent(Function* f);

void Function_InitFunctions();

void Function_DeleteFunctions();

void* Function_Add(Function f);

Function* Function_Find(char* identifier);

Function Function_Create(char* identifier, VariableType* returnType);

void Function_Dispose(Function* this);
