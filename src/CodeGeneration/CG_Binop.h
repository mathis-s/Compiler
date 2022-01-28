#pragma once

#include "../AST.h"
#include "../Scope.h"
#include "../Token.h"
#include "../Variables.h"

void BinopAdd32(Value* dstValue, Value* srcA, Value* srcB, bool srcAro, bool srcBro);

void BinopSub32(Value* dstValue, Value* srcA, Value* srcB, bool srcAro);
void BinopMod(Value* dstValue, Value* srcA, Value* srcB);

bool BinopShiftLeft(Value dstValue, Value srcValue, VariableType type);

bool BinopShiftRight(Value dstValue, Value srcValue, VariableType type);

void BinopEquality(BinOp op, Value* dstValue, Value* srcA, Value* srcB, bool srcAro, bool srcBro);

bool TryParseBinaryOperator(Token* b, int length, Scope* scope, AST_Expression_BinOp** outExpr);

void CodeGen_BinaryOperator(AST_Expression_BinOp* expr, Scope* scope, Value* oValue, VariableType** oType,
                            bool* oReadOnly);

/*
1	++ --	Suffix/postfix increment and decrement	Left-to-right
()	Function call
[]	Array subscripting
.	Structure and union member access
->	Structure and union member access through pointer
(type){list}	Compound literal(C99)
2	++ --	Prefix increment and decrement[note 1]	Right-to-left
+ -	Unary plus and minus
! ~	Logical NOT and bitwise NOT
(type)	Cast
*	Indirection (dereference)
&	Address-of
sizeof	Size-of[note 2]
_Alignof	Alignment requirement(C11)
3	* / %	Multiplication, division, and remainder	Left-to-right
4	+ -	Addition and subtraction
5	<< >>	Bitwise left shift and right shift
6	< <=	For relational operators < and ≤ respectively
> >=	For relational operators > and ≥ respectively
7	== !=	For relational = and ≠ respectively
8	&	Bitwise AND
9	^	Bitwise XOR (exclusive or)
10	|	Bitwise OR (inclusive or)
11	&&	Logical AND
12	||	Logical OR
13	?:	Ternary conditional[note 3]	Right-to-left
14[note 4]	=	Simple assignment
+= -=	Assignment by sum and difference
*= /= %=	Assignment by product, quotient, and remainder
<<= >>=	Assignment by bitwise left shift and right shift
&= ^= |=	Assignment by bitwise AND, XOR, and OR
15	,	Comma	Left-to-right
*/