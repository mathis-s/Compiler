
#pragma once
#include "Scope.h"
#include "Token.h"

void CompileStructDeclaration(TokenArray* t, size_t* i, Scope* scope);
void Compile(TokenArray t);