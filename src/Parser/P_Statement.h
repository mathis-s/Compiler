#pragma once
#include "../AST.h"
#include "../Scope.h"
#include "../Token.h"
#include "../Variables.h"

bool TryParseNewScope(TokenArray* t, size_t* i, Scope* scope, AST_Statement_Scope** outStmt);
void ParseStatement(TokenArray* t, size_t* i, Scope* scope, AST_Statement** outStmt);