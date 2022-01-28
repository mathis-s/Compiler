#include "P_UnOp.h"
#include "../AST.h"
#include "../Optimizer.h"
#include "../Token.h"
#include "../Util.h"
#include "P_Expression.h"

typedef enum
{
    Left,
    Right,
} Side;

typedef struct Unop
{
    TokenType token;
    Side side;
} Unop;

typedef struct
{
    const Unop* unops;
    const size_t numUnops;
} UnopGroup;

const Unop unop_prec0[] = {{Increment, Left}, {Decrement, Left}};
const Unop unop_prec1[] = {{Minus, Right}, {LogicalNOT, Right}, {BitwiseNOT, Right}, {Ampersand, Right},
                           {Star, Right},  {Increment, Right},  {Decrement, Right}};

const UnopGroup unOps[] = {
    {unop_prec0, 2},
    {unop_prec1, 7},
};

bool TryParseUnaryOperator(Token* b, int length, Scope* scope, AST_Expression_UnOp** outExpr, int precedence)
{
    int inBracket = 0;

    if (precedence > 1)
        return false;

    const Unop* buffer = unOps[precedence].unops;
    size_t bufferLen = unOps[precedence].numUnops;

    for (int i = length - 1; i >= 0; i--)
    {
        if (b[i].type == RBrClose)
            ++inBracket;
        if (b[i].type == RBrOpen)
            --inBracket;

        // If ANY of the Operations with current precedence is found, we compile it.
        const Unop* op = NULL;
        for (size_t j = 0; j < bufferLen; j++)
        {
            if (buffer[j].token == b[i].type)
            {
                op = &buffer[j];

                if (op != NULL && inBracket == 0)
                {
                    if (op->side == Left && i != length - 1)
                        continue;

                    if (op->side == Right && i != 0)
                        continue;

                    AST_Expression* expr;

                    if (op->side == Left)
                        ParseExpression(b, length - 1, scope, &expr);
                    else
                        ParseExpression(&b[i + 1], length - 1, scope, &expr);

                    AST_Expression_UnOp* retval = xmalloc(sizeof(AST_Expression_UnOp));
                    retval->type = AST_ExpressionType_UnaryOP;
                    switch (op->token)
                    {
                        case Increment:
                            if (op->side == Left)
                                retval->op = UnOp_PostIncrement;
                            else
                                retval->op = UnOp_PreIncrement;
                            break;
                        case Decrement:
                            if (op->side == Left)
                                retval->op = UnOp_PostDecrement;
                            else
                                retval->op = UnOp_PreDecrement;
                            break;
                        case LogicalNOT:
                            retval->op = UnOp_LogicalNOT;
                            break;
                        case BitwiseNOT:
                            retval->op = UnOp_BitwiseNOT;
                            break;
                        case Minus:
                            retval->op = UnOp_Negate;
                            break;
                        case Ampersand:
                            retval->op = UnOp_AddressOf;
                            if (expr->type == AST_ExpressionType_VariableAccess)
                                Optimizer_LogAddrOf(((AST_Expression_VariableAccess*)expr)->id);
                            break;
                        case Star:
                            retval->op = UnOp_Dereference;
                            break;
                        default:
                            ErrorAtToken("Invalid unop", &b[i]);
                            break;
                    }
                    retval->exprA = expr;
                    retval->loc = Token_GetLocationP(b);

                    *outExpr = retval;

                    return true;
                }
            }
        }
    }

    return false;
}