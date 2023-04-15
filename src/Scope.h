#pragma once
#include "Enum.h"
#include "Function.h"
#include "GenericList.h"
#include "Struct.h"
#include "Variables.h"
#include <string.h>

#ifndef CUSTOM_COMP
typedef struct Scope Scope;
#endif
#ifdef CUSTOM_COMP
struct Scope;
#endif

typedef struct
{
    VariableType* type;
    const char* name;
} Typedef;

typedef struct Scope
{
    Scope* parent;

    GenericList variables;
    GenericList structs;
    GenericList enums;
    GenericList typedefs;

    uint16_t preferredRegisters[8];
} Scope;

Scope Scope_Create(Scope* parent);
void Scope_Dispose(Scope* this);

bool Scope_NameIsUsed(Scope* this, char* id);
void Scope_DeleteVariable(Scope* this, Variable* var);
void Scope_DeleteVariablesAfterLoop(Scope* this, void* loop);

bool CompareVariableToID(const void* variable, const void* identifier);

Variable* Scope_FindVariable(Scope* this, char* id);
Variable* Scope_FindVariableByValue(Scope* this, Value* val);
Struct* Scope_FindStruct(Scope* this, char* id);
Enum* Scope_FindEnum(Scope* this, char* id);
Typedef* Scope_FindTypedef(Scope* this, char* id);

void Scope_AddVariable(Scope* this, Variable var);
void Scope_AddStruct(Scope* this, Struct* s);
void Scope_AddEnum(Scope* this, Enum e);
void Scope_AddTypedef (Scope* this, Typedef t);