#pragma once
#include "../AST.h"
#include "../Scope.h"
#include "../Token.h"
#include "../Variables.h"

void CompileStatement(TokenArray* t, size_t* i, Scope* scope);
void CodeGen_Statement(AST_Statement* stmt, Scope* scope);