#pragma once

#include "../AST.h"
#include "../Scope.h"
#include "../Token.h"

bool TryParseBinaryOperator(Token* b, int length, Scope* scope, AST_Expression_BinOp** outExpr, int precedence);
