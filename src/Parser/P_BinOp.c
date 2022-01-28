#include "P_BinOp.h"
#include "../AST.h"
#include "../Scope.h"
#include "../Token.h"
#include "../Util.h"
#include "P_Expression.h"
#include <stdbool.h>

typedef enum
{
    LtR,
    RtL,
} Assoc;

typedef struct
{
    const TokenType* tokens;
    const size_t numTokens;
    const Assoc assoc;
} BinopGroup;

const TokenType prec0[] = {Dot, Arrow};
const TokenType prec2[] = {Star, Slash, Percent};
const TokenType prec3[] = {Plus, Minus};
const TokenType prec4[] = {ShiftLeft, ShiftRight};
const TokenType prec5[] = {LessThan, LessThanEq, GreaterThan, GreaterThanEq};
const TokenType prec6[] = {Equals, NotEquals};
const TokenType prec7[] = {Ampersand};
const TokenType prec8[] = {BitwiseXOR};
const TokenType prec9[] = {BitwiseOR};
const TokenType prec10[] = {LogicalAND};
const TokenType prec11[] = {LogicalOR};
const TokenType prec13[] = {Assignment,    AssignmentAdd,       AssignmentSub,       AssignmentMul,
                            AssignmentDiv, AssignmentAND,       AssignmentOR,        AssignmentXOR,
                            AssignmentMod, AssignmentShiftLeft, AssignmentShiftRight};

const BinopGroup binops[] = {
    {prec0, 2, LtR},  {NULL, 0, RtL},   {prec2, 3, RtL}, {prec3, 2, LtR},   {prec4, 2, LtR},
    {prec5, 4, LtR},  {prec6, 2, LtR},  {prec7, 1, LtR}, {prec8, 1, LtR},   {prec9, 1, LtR},
    {prec10, 1, LtR}, {prec11, 1, LtR}, {NULL, 0, RtL},  {prec13, 11, RtL},
};

/*void PrintExpressionTree(AST_Expression* expr, int indent)
{
    for (int i = 0; i < indent; i++)
        printf("  ");
    switch (expr->type)
    {
        case AST_ExpressionType_UnaryOP:
            printf("UnOp %i (\n", ((AST_Expression_UnOp*)expr)->op);
            PrintExpressionTree(((AST_Expression_UnOp*)expr)->exprA, indent + 1);
            for (int i = 0; i < indent; i++)
                printf("  ");
            printf(")\n");
            break;
        case AST_ExpressionType_BinaryOP:
            printf("BinOp %i (\n", ((AST_Expression_BinOp*)expr)->op);
            PrintExpressionTree(((AST_Expression_BinOp*)expr)->exprA, indent + 1);

            PrintExpressionTree(((AST_Expression_BinOp*)expr)->exprB, indent + 1);

            for (int i = 0; i < indent; i++)
                printf("  ");
            printf(")\n");
            break;
        case AST_ExpressionType_FunctionCall:;
            printf("Call %s (\n", ((AST_Expression_FunctionCall*)expr)->id);
            AST_Expression_FunctionCall* fcall = expr;
            for (size_t i = 0; i < fcall->numParameters; i++)
            {
                PrintExpressionTree(fcall->parameters[i], indent + 1);
            }
            for (int i = 0; i < indent; i++)
                printf("  ");
            printf(")\n");
            break;
        case AST_ExpressionType_ListLiteral:;
            AST_Expression_ListLiteral* list = expr;
            printf("List (\n");
            for (size_t i = 0; i < list->numExpr; i++)
            {
                PrintExpressionTree(list->expressions[i], indent + 1);
            }
            for (int i = 0; i < indent; i++)
                printf("  ");
            printf(")\n");
            break;
        case AST_ExpressionType_TypeCast:
            PrintExpressionTree(((AST_Expression_TypeCast*)expr)->exprA, indent + 1);
            break;
        case AST_ExpressionType_VariableAccess:
            printf("Var %s\n", ((AST_Expression_VariableAccess*)expr)->id);
            break;
        case AST_ExpressionType_IntLiteral:
            printf("Lit %i\n", ((AST_Expression_IntLiteral*)expr)->literal);
            break;
        default:
            printf("Atom %i\n", expr->type);
            break;
    }
}*/

bool TryParseBinaryOperator(Token* b, int length, Scope* scope, AST_Expression_BinOp** outExpr, int precedence)
{
    int inBracketR = 0;
    int inBracketC = 0;
    int inBracketA = 0;

    // Store all Binops with current precedence in buffer
    if (binops[precedence].numTokens == 0)
        return false;

    const TokenType* buffer = binops[precedence].tokens;
    size_t bufferLen = binops[precedence].numTokens;

    int i;
    int count;

    if (binops[precedence].assoc == LtR)
    {
        i = length - 1;
        count = -1;
    }
    else
    {
        i = 0;
        count = 1;
    }

    // Loop through tokens
    for (; i >= 0 && i < length; i += count)
    {
        switch (b[i].type)
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

        // If the left or the right argument is missing, the operation is not binary, therefore we skip it.
        if (i == 0 || i == length - 1)
            continue;

        // If ANY of the Operations with current precedence is found, we compile it.
        if (inBracketA + inBracketC + inBracketR == 0)
        {
            TokenType op = None;
            for (size_t j = 0; j < bufferLen; j++)
                if (buffer[j] == b[i].type)
                {
                    op = buffer[j];
                    break;
                }

            if (op != None)
            {
                // TODO Not sure if this is still necessary
                if (b[i - 1].type != IntLiteral && b[i - 1].type != Identifier && b[i - 1].type != RBrClose &&
                    b[i - 1].type != ABrClose && b[i - 1].type != CBrClose)
                    continue;

                AST_Expression_BinOp* retval = xmalloc(sizeof(AST_Expression_BinOp));
                retval->type = AST_ExpressionType_BinaryOP;

                // Binops in AST.h (enum BinOp) are in the same
                // Order as the corresponding tokens in Token.h
                // (enum TokenType) referring to those binops here.
                // This way, the large switch that would be here
                // otherwise can be replaced with arithmetic.
                retval->op = (BinOp)(op - Plus);
                assert(retval->op >= 0);
                assert(retval->op <= BinOp_Assignment);

                ParseExpression(b, i, scope, &retval->exprA);
                ParseExpression(b + i + 1, length - i - 1, scope, &retval->exprB);

                retval->loc = Token_GetLocationP(&b[i]);
                *outExpr = retval;
                return true;
            }
        }
    }
    return false;
}
