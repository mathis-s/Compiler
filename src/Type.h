#pragma once
#include "Function.h"
#include "Struct.h"
#include "Token.h"
#include <stdbool.h>
#include <string.h>

typedef enum
{
    Qualifier_None = 0,
    Qualifier_Const = 1,
    Qualifier_ConstPointer = 2,
    Qualifier_Restrict = 4,
    Qualifier_Register = 8,
    Qualifier_Static = 16,
    Qualifier_Stack = 32,
    // Stack and Register force,
    // these are just recommendations
    Qualifier_OptimizerStack = 64,
    Qualifier_OptimizerRegister = 128,
} Qualifiers;

// Currently in the process of transitioning VariableType
// from having a void* to additional data to polymorphism.
// Not completely done yet, that's why there's both right now.
typedef struct VariableType
{
    TokenType token;
    //int pointerLevel;
    Qualifiers qualifiers;
    int refCount;
} VariableType;

typedef struct VariableTypePtr
{
    TokenType token;
    //int pointerLevel;
    Qualifiers qualifiers;
    int refCount;
    VariableType* baseType;
} VariableTypePtr;

typedef struct
{
    TokenType token;
    //int pointerLevel;
    Qualifiers qualifiers;
    int refCount;
    int memberCount;
    VariableType* memberType;
} VariableTypeArray;

typedef struct
{
    TokenType token;
    //int pointerLevel;
    Qualifiers qualifiers;
    int refCount;
    Struct* str;
} VariableTypeStruct;

typedef struct
{
    TokenType token;
    //int pointerLevel;
    Qualifiers qualifiers;
    int refCount;
    Function func;
} VariableTypeFunctionPointer;

// FIXME, there's no extern, so currently these are defined and allocd for every TU
#ifndef CUSTOM_COMP
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
static VariableType AnyVariableType = {None, Qualifier_None, 1};
static VariableType MachineUIntType = {UintKeyword, Qualifier_None, 1};
static VariableType MachineIntType = {IntKeyword, Qualifier_None, 1};
static VariableType MachineInt32Type = {Int32Keyword, Qualifier_None, 1};
static VariableType MachineUInt32Type = {Uint32Keyword, Qualifier_None, 1};
#ifndef CUSTOM_COMP
#pragma GCC diagnostic pop
#endif

VariableType* Type_AddReference(VariableType* type);

bool Type_RemoveReference(VariableType* type);

VariableType* Type_Copy(VariableType* type);

int SizeInWords(const VariableType* type);

#ifndef CUSTOM_COMP
typedef struct Scope Scope;
#endif
#ifdef CUSTOM_COMP
typedef struct Scope {} Scope;
#endif
bool IsDataToken(Token token, Scope* scope);

bool IsDataType(const VariableType* type);

bool IsPrimitiveType(const VariableType* type);
bool Type_Check(const VariableType* a, const VariableType* b);