#pragma once
#include "../Type.h"

VariableType* ParseVariableType(Token* tokens, size_t* i, size_t maxLen, Scope* scope, char** identifier,
                                bool allowVoid);
void P_Type_Inc(Token* tokens, const size_t maxLen, size_t* const i);
void* P_Type_PopCur(Token* tokens, const size_t maxLen, size_t* const i, TokenType type);
void* P_Type_PopNextInc(Token* tokens, const size_t maxLen, size_t* const i, TokenType type);
void* P_Type_PopNext(Token* tokens, const size_t maxLen, size_t* const i, TokenType type);