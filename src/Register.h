#pragma once
#include <stdint.h>

enum Register
{
	Register_R0 = 0,
	Register_R1 = 1,
	Register_R2 = 2,
	Register_R3 = 3,
	Register_R4 = 4,
	Register_R5 = 5,
	Register_R6 = 6,
	Register_R7 = 7,
	Register_IP = 8,
	Register_SP = 9,
};

int16_t Registers_GetUsed();
int Registers_GetFree();
void Register_Free(int r);
void Register_GetSpecific(int r);
void Registers_FreeAll();
uint16_t Registers_PushAllUsed(int* num);
uint16_t Registers_PushAllUsedMasked(int* num, uint16_t mask);
int Registers_GetNumUsed();
int Registers_GetNumUsedMasked(uint16_t mask);
int Registers_GetNumFree();
int Registers_GetNumPreferred();
void Registers_Pop(uint16_t pushedRegisters, int fromAddr);
void Registers_SetPreferred(const uint16_t* prefRegs);
