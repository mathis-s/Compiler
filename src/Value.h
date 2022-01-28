#pragma once
#include "Error.h"
#include "Flags.h"
#include "Register.h"
#include "Stack.h"
#include "assert.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    AddressType_Register,
    AddressType_Memory,
    AddressType_MemoryRegister,
    AddressType_MemoryRelative,
    AddressType_StructMember,
    AddressType_Literal,
    AddressType_Flag,
    AddressType_None = 0xFFFF,

} AddressType;

typedef struct
{
    int32_t address;
    AddressType addressType;
    int size;
} Value;

#ifndef CUSTOM_COMP
static const Value NullValue = {-1, AddressType_None, -1};
static const Value FlagValue = {Flag_None, AddressType_Flag, -1};
#endif

#ifdef CUSTOM_COMP
static const Value NullValue = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
static const Value FlagValue = {(int32)Flag_None, AddressType_Flag, 0xFFFF};
#endif

int Value_GetR0(const Value* v);

int Value_GetR1(const Value* v);

void Value_ValueAddrToStackPointer(const Value* v);

Value Value_FromRegister(int r0);
Value Value_FromRegisters(int r0, int r1);

Value Value_Register(int size);

void Value_FreeValue(Value* value);

Value Value_MemoryRelative(int addr, int size);

Value Value_Memory(uint16_t addr, int size);

Value Value_Literal(int32_t literal);

Value Value_GetLowerWord(const Value* value);

Value Value_GetUpperWord(const Value* value, bool* oReadOnly);

Value Value_Flag(Flag f);

// Copies srcValue into dstValue. Count of copied words
// is dstValue.size
void Value_GenerateMemCpy(Value dstValue, Value srcValue);

// Pushes passed value to the stack.
// Call ShiftAddressSpace() and set curStackPointerOffset to 0
// to make push permanent, otherwise the stack will be reset
// on the next AlignStack()
void Value_Push(Value* value);

// Moves n into all words of value
void Value_MemSet(Value* value, uint16_t n);

void PrintValueAsOperand(const Value* val);

// Sets zero flag if no bits in passed value are set.
// Otherwise, zero flags is unset. Used for conditional
// branches.
void Value_ToFlag(Value* value);

bool Value_Equals(const Value* a, const Value* b);