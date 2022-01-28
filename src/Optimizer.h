#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "AST.h"

void Optimizer_LogDeclaration(AST_Statement_Declaration* declaration);
void Optimizer_LogAccess(AST_Expression_VariableAccess* access);
void Optimizer_LogFunctionCall(uint16_t modifiedRegisters);
void Optimizer_EnterNewScope();
void Optimizer_ExitScope(uint16_t* const oPrefRegisters);
void Optimizer_EnterLoop();
void Optimizer_LogAddrOf(const char* idOfDerefdVar);
void Optimizer_ExitLoop(void* loop);
void Optimizer_LogInlineASM(void* asmNode);