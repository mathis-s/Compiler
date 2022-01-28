#pragma once
#include <stdbool.h>

#include "../AST.h"
#include "../Scope.h"
#include "../Token.h"
#include "P_UnOp.h"

void ParseExpression(Token* b, int length, Scope* scope, AST_Expression** expr);
void ParseNextExpression(TokenArray* t, size_t* i, Scope* scope, AST_Expression** outExpr);
void ParseNextExpressionWithSeparator(TokenArray* t, size_t* i, Scope* scope, AST_Expression** outExpr,
                                      TokenType separator, int inBracket);
bool TryParseArrayMemberAccess(Token* b, int length, Scope* scope, AST_Expression_BinOp** expr);