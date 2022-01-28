#pragma once

void Stack_Align();

void Stack_ToAddress(int addr);

void Stack_Reset();

void Stack_SetSize(int n);

void Stack_SetOffset(int n);

int Stack_GetSize();

int Stack_GetOffset();

// Adds n to current stack pointer offset
void Stack_Offset(int n);

// Adds n to current stack size
void Stack_OffsetSize(int n);

int Stack_GetDelta(int addr);