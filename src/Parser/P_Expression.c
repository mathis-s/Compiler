#include "P_Expression.h"
#include "../AST.h"
#include "../Function.h"
#include "../Optimizer.h"
#include "../Scope.h"
#include "../Token.h"
#include "../Util.h"
#include "P_BinOp.h"
#include "P_UnOp.h"
#include "P_Type.h"
#include <stdbool.h>

static bool TryParseListLiteral(Token* b, int length, Scope* scope, AST_Expression_ListLiteral** outExpr)
{
    if (b[0].type != CBrOpen)
        return false;

    GenericList expressions = GenericList_Create(sizeof(AST_Expression*));

    int index = 1;
    int inBracketR = 0;
    int inBracketC = 0;
    int inBracketA = 0;

    while (true)
    {
        int exprLen = 0;

        // List Literals might be nested, so count brackets to find the end.
        while (inBracketC + inBracketA + inBracketR > 0 || (b[index + exprLen].type != Comma))
        {
            switch (b[index + exprLen].type)
            {
                case RBrOpen:
                    inBracketR++;
                    break;
                case RBrClose:
                    inBracketR--;
                    break;
                case CBrOpen:
                    inBracketC++;
                    break;
                case CBrClose:
                    inBracketC--;
                    break;
                case ABrOpen:
                    inBracketA++;
                    break;
                case ABrClose:
                    inBracketA--;
                    break;
                default:
                    break;
            }

            if (inBracketC == -1)
                break;

            if ((++exprLen) + index >= length)
                SyntaxErrorAtToken(b);
        }
        if (exprLen == 0)
        {
            if (inBracketC != -1)
                SyntaxErrorAtToken(b);
        }
        else
        {
            AST_Expression* expr;
            ParseExpression(&b[index], exprLen, scope, &expr);
            GenericList_Append(&expressions, &expr);

            index += exprLen;
        }

        if (b[index].type == CBrClose)
            break;
        index++;
        if (b[index].type == CBrClose)
            break;
    }

    if (index != length - 1)
        SyntaxErrorAtToken(&b[index]);

    GenericList_ShrinkToSize(&expressions);

    AST_Expression_ListLiteral* retval = xmalloc(sizeof(AST_Expression_ListLiteral));
    retval->type = AST_ExpressionType_ListLiteral;
    retval->expressions = expressions.data;
    retval->numExpr = expressions.count;
    retval->loc = Token_GetLocationP(&b[0]);
    *outExpr = retval;
    return true;
}

bool TryParseArrayMemberAccess(Token* b, int length, Scope* scope, AST_Expression_BinOp** expr)
{
    if (length < 4 || b[length - 1].type != ABrClose)
        return false;

    int index = length - 1;
    {
        int inBracketR = 0;
        int inBracketC = 0;
        int inBracketA = -1;

        while ((inBracketA + inBracketC + inBracketR) != 0)
        {
            if (--index < 0)
                return false;

            switch (b[index].type)
            {
                case RBrOpen:
                    inBracketR++;
                    break;
                case RBrClose:
                    inBracketR--;
                    break;
                case CBrOpen:
                    inBracketC++;
                    break;
                case CBrClose:
                    inBracketC--;
                    break;
                case ABrOpen:
                    inBracketA++;
                    break;
                case ABrClose:
                    inBracketA--;
                    break;
                default:
                    break;
            }
        }
    }

    AST_Expression_BinOp* retval = xmalloc(sizeof(AST_Expression_BinOp));
    retval->loc = Token_GetLocationP(&b[index]);
    retval->type = AST_ExpressionType_BinaryOP;
    retval->op = BinOp_ArrayAccess;

    ParseExpression(b, index, scope, &retval->exprA);

    index++;

    size_t indexExprLen = length - (index)-1;
    if (indexExprLen == 0)
        SyntaxErrorAtToken(b);
    ParseExpression(b + index, indexExprLen, scope, &retval->exprB);

    *expr = retval;
    return true;
}

bool TryParseTypeCast(Token* b, int length, Scope* scope, AST_Expression_TypeCast** expr)
{
    // Type Cast
    if (b[0].type == RBrOpen && length > 1 && IsDataToken(b[1], scope))
    {
        size_t i = 1;
        VariableType* newType = ParseVariableType(b, &i, length, scope, NULL, false);

        if (b[i++].type != RBrClose)
            SyntaxErrorAtToken(b);

        AST_Expression_TypeCast* retval = xmalloc(sizeof(AST_Expression_TypeCast));
        ParseExpression(b + i, length - i, scope, &(retval->exprA));

        retval->newType = newType;
        retval->type = AST_ExpressionType_TypeCast;
        retval->loc = Token_GetLocationP(&b[0]);

        *expr = retval;

        return true;
    }

    return false;
}

bool TryParseFunctionCall(Token* b, int length, Scope* scope, AST_Expression_FunctionCall** outExpr)
{
    if (b[0].type == Identifier)
    {
        char* identifier = b[0].data;
        if (length < 3 || b[1].type != RBrOpen)
            return false;

        Variable* funcPointer = NULL;
        Function* outFunc = NULL;

        if ((funcPointer = Scope_FindVariable(scope, identifier)) != NULL &&
            funcPointer->type->token == FunctionPointerToken)
        {
            outFunc = funcPointer->type->structure;
        }
        else if ((outFunc = Function_Find(identifier)) == NULL)
            ErrorAtToken("Undefined identifier!", &b[0]);

        Optimizer_LogFunctionCall(outFunc->modifiedRegisters);

        // We tell the code generator to preferably use registers that are not modified
        // in function calls, such that we don't have to push/pop them.
        // Function_GetCurrent()->preferredRegisters &= ~(outFunc->modifiedRegisters);

        if (b[length - 1].type != RBrClose)
            ErrorAtToken("Invalid function call", &b[0]);

        Token* exprBegin = &b[length - 2];
        int curExprLength = 1;
        size_t parameterIndex = 0;
        GenericList params = GenericList_Create(sizeof(AST_Expression*));

        int inBracketR = 0;
        int inBracketC = 0;
        int inBracketA = 0;

        // We parse args backwards here as that is also the order they are compiled in.
        // The optimizer has problems when the order of variable accesses in the parser
        // is different from that in to Code Generator, this is simply the easiest way
        // to fix that.
        if (exprBegin->type != RBrOpen)
            while (inBracketR < 1)
            {
                if ((inBracketC + inBracketA + inBracketR == 0) &&
                    (exprBegin->type == Comma || exprBegin->type == RBrOpen))
                {
                    // Parse Expression
                    if (outFunc->parameters.count == 0 && exprBegin->type == RBrClose)
                        break;

                    if (curExprLength <= 1)
                        ErrorAtToken("Invalid parameter", exprBegin);

                    AST_Expression* astExpr;
                    ParseExpression(exprBegin + 1, curExprLength - 1, scope, &astExpr);
                    GenericList_Append(&params, &astExpr);

                    if (outFunc->parameters.count <= parameterIndex && !outFunc->variadicArguments)
                        ErrorAtToken("Invalid parameter count", exprBegin);

                    curExprLength = 0;
                }

                switch (exprBegin->type)
                {
                    case RBrOpen:
                        inBracketR++;
                        break;
                    case RBrClose:
                        inBracketR--;
                        break;
                    case CBrOpen:
                        inBracketC++;
                        break;
                    case CBrClose:
                        inBracketC--;
                        break;
                    case ABrOpen:
                        inBracketA++;
                        break;
                    case ABrClose:
                        inBracketA--;
                        break;
                    default:
                        break;
                }

                curExprLength++;
                exprBegin--;
                if ((exprBegin) < &b[0])
                    SyntaxErrorAtToken(b);
            }

        GenericList_ShrinkToSize(&params);
        AST_Expression_FunctionCall* retval = xmalloc(sizeof(AST_Expression_FunctionCall));
        retval->loc = Token_GetLocationP(&b[0]);
        retval->type = AST_ExpressionType_FunctionCall;
        retval->id = identifier;

        retval->parameters = params.data;
        retval->numParameters = params.count;

        *outExpr = retval;

        return true;
    }
    return false;
}

bool TryParseSizeOf(Token* b, int length, Scope* scope, AST_Expression_IntLiteral** outExpr)
{
    if (b[0].type != SizeOfKeyword)
        return false;
    size_t i = 1;
    if (i >= (size_t)length || b[i].type != RBrOpen)
        SyntaxErrorAtToken(&b[0]);
    if ((i++) >= (size_t)length)
        SyntaxErrorAtToken(&b[0]);
    VariableType* type = ParseVariableType(b, &i, length, scope, NULL, false);
    int size = SizeInWords(type);
    Type_RemoveReference(type);

    AST_Expression_IntLiteral* retval = xmalloc(sizeof(AST_Expression_IntLiteral));
    retval->type = AST_ExpressionType_IntLiteral;
    retval->loc = Token_GetLocationP(&b[0]);
    if (size < 1)
        ErrorAtToken("Invalid variable type", &b[0]);
    retval->literal = (int32_t)size;
    *outExpr = retval;

    if (i >= (size_t)length || b[i].type != RBrClose)
        SyntaxErrorAtToken(&b[0]);
    return true;
}

void ParseExpression(Token* b, int length, Scope* scope, AST_Expression** outExpr)
{
    if (length == 1)
        switch (b[0].type)
        {
            case IntLiteral:
            {
                int32_t* literal = (int32_t*)b->data;

                AST_Expression_IntLiteral* retval = xmalloc(sizeof(AST_Expression_IntLiteral));
                retval->type = AST_ExpressionType_IntLiteral;
                retval->literal = *literal;
                retval->loc = Token_GetLocationP(b);
                *outExpr = (void*)retval;
                return;
            }
            case StringLiteral:
            {
                AST_Expression_StringLiteral* retval = xmalloc(sizeof(AST_Expression_StringLiteral));
                retval->type = AST_ExpressionType_StringLiteral;
                retval->data = b[0].data;
                retval->loc = Token_GetLocationP(b);
                *outExpr = (void*)retval;
                return;
            }
            case Identifier:
            {
                AST_Expression_VariableAccess* retval = xmalloc(sizeof(AST_Expression_VariableAccess));
                retval->type = AST_ExpressionType_VariableAccess;
                retval->id = b[0].data;
                retval->loc = Token_GetLocationP(b);
                *outExpr = (void*)retval;
                Optimizer_LogAccess(retval);
                return;
            }
            default:
                SyntaxErrorAtToken(b);
        }

    int precedence = 13;
    bool runtime = (Function_GetCurrent() != NULL);
    while (true)
    {
        AST_Expression* expr;

        if (precedence == 0)
        {
            if (runtime && TryParseArrayMemberAccess(b, length, scope, (void*)(&expr)))
            {
                *outExpr = expr;
                break;
            }
        }
        if (precedence <= 1 && runtime && TryParseUnaryOperator(b, length, scope, (void*)(&expr), precedence))
        {
            *outExpr = expr;
            break;
        }
        else if (runtime && TryParseBinaryOperator(b, length, scope, (void*)(&expr), precedence))
        {
            *outExpr = expr;
            break;
        }
        else if (precedence == 0)
        {
            if (TryParseListLiteral(b, length, scope, (void*)(&expr)))
            {
                *outExpr = expr;
                break;
            }
            else if (runtime && TryParseFunctionCall(b, length, scope, (void*)(&expr)))
            {
                *outExpr = expr;
                break;
            }
            else if (b[0].type == RBrOpen)
            {
                if (b[length - 1].type != RBrClose)
                    SyntaxErrorAtToken(&b[length - 1]);
                ParseExpression(b + 1, length - 2, scope, outExpr);
                break;
            }
        }
        else if (precedence == 1)
        {
            if (TryParseTypeCast(b, length, scope, (void*)(&expr)))
            {
                *outExpr = expr;
                break;
            }
            else if (TryParseSizeOf(b, length, scope, (void*)(&expr)))
            {
                *outExpr = expr;
                break;
            }
        }

        precedence--;
        if (precedence == -1)
            SyntaxErrorAtToken(b);
    }
}

void ParseNextExpressionWithSeparator(TokenArray* t, size_t* i, Scope* scope, AST_Expression** outExpr,
                                      TokenType separator, int inBracket)
{
    size_t length = 0;
    Token* b = &t->tokens[*i];
    while (true)
    {
        if (b[length].type == RBrOpen)
            inBracket++;
        if (b[length].type == RBrClose)
            inBracket--;

        if (b[length].type == separator && inBracket == 0)
            break;

        if (++length >= t->curLength)
            SyntaxErrorAtIndex(length - 1);
    }

    ParseExpression(b, length, scope, outExpr);

    (*i) += length;
}

void ParseNextExpression(TokenArray* t, size_t* i, Scope* scope, AST_Expression** outExpr)
{
    ParseNextExpressionWithSeparator(t, i, scope, outExpr, Semicolon, 0);
}