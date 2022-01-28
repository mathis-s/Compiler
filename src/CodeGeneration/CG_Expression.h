#pragma once
#include <stdbool.h>

#include "../AST.h"
#include "../Scope.h"
#include "../Token.h"

void CodeGen_Expression(AST_Expression* expr, Scope* scope, Value* oValue, VariableType** oType, bool* oReadOnly);
void FreeExpressionTree(AST_Expression* expr, Scope* scope);
void PrintExpressionTree(AST_Expression* expr);

int GetLabelID();