#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "GenericList.h"
#include "Register.h"
#include "Stack.h"
#include "Token.h"
#include "Type.h"
#include "Value.h"

#ifndef CUSTOM_COMP
typedef struct Scope Scope;
#endif
#ifdef CUSTOM_COMP
typedef struct Scope {} Scope;
#endif

typedef struct
{
    VariableType* type;
    char* name;
    Value value;
    void* lastAccess;
} Variable;

void ShiftAddressSpace(Scope* scope, int offset);

void ShiftAddressSpaceLocal(GenericList* variables, int offset);

Value GetValueOnStack(int size, Scope* scope);
