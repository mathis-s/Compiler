#pragma once

#include "../AST.h"
#include "../Scope.h"
#include "../Token.h"
#include <stdbool.h>

bool TryParseUnaryOperator(Token* b, int length, Scope* scope, AST_Expression_UnOp** outExpr, int precedence);