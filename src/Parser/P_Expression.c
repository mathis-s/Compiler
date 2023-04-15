#include "P_Expression.h"
#include "../AST.h"
#include "../Function.h"
#include "../Optimizer.h"
#include "../Scope.h"
#include "../Token.h"
#include "../Util.h"
#include "P_Type.h"
#include <stdbool.h>

static void ParseExpressionPrec(Token* b, int length, size_t* i, Scope* scope, int minPrec, AST_Expression** outExpr);

bool TryParseTypeCast(Token* b, size_t length, size_t* i, Scope* scope, AST_Expression_TypeCast** expr)
{
    // Type Cast
    if (b[*i].type == RBrOpen && length > (*i + 1) && IsDataToken(b[*i + 1], scope))
    {
        P_Type_Inc(b, length, i);

        VariableType* newType = ParseVariableType(b, i, length, scope, NULL, false);

        P_Type_PopCur(b, length, i, RBrClose);

        AST_Expression_TypeCast* retval = xmalloc(sizeof(AST_Expression_TypeCast));
        ParseExpressionPrec(b, length, i, scope, 14 - 2, &retval->exprA);

        retval->newType = newType;
        retval->type = AST_ExpressionType_TypeCast;
        retval->loc = Token_GetLocationP(&b[*i]);

        *expr = retval;

        return true;
    }

    return false;
}

bool TryParseFunctionCall(Token* b, int length, size_t* i, Scope* scope, AST_Expression_FunctionCall** outExpr)
{
    if (b[*i].type == Identifier)
    {
        char* identifier = b[*i].data;
        if (length < 3 || b[*i + 1].type != RBrOpen)
            return false;

        Token* const exprEnd = &b[*i];
        P_Type_Inc(b, length, i);
        P_Type_Inc(b, length, i);

        Function* outFunc = NULL;

        // FIXME: after parsing variable cannot be found in scope yet!
        /*if ((funcPointer = Scope_FindVariable(scope, identifier)) != NULL &&
            funcPointer->type->token == FunctionPointerToken)
        {
            outFunc = &((VariableTypeFunctionPointer*)funcPointer->type)->func;
        }*/
        outFunc = Function_Find(identifier);
        if (outFunc)
            Optimizer_LogFunctionCall(outFunc->modifiedRegisters);
        else
            Optimizer_LogFunctionCall(0xFFFF);

        int inBracketR = 1;
        int inBracketC = 0;
        int inBracketA = 0;
        // Find closing bracket of call
        while (inBracketR || inBracketC || inBracketA)
        {
            switch (b[*i].type)
            {
                case RBrOpen: inBracketR++; break;
                case RBrClose: inBracketR--; break;
                case CBrOpen: inBracketC++; break;
                case CBrClose: inBracketC--; break;
                case ABrOpen: inBracketA++; break;
                case ABrClose: inBracketA--; break;
                default: break;
            }
            P_Type_Inc(b, length, i);
        }

        Token* exprBegin = &b[*i - 2];

        int curExprLength = 1;
        GenericList params = GenericList_Create(sizeof(AST_Expression*));

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
                    if (exprBegin->type == RBrClose)
                        break;

                    if (curExprLength <= 1)
                        ErrorAtToken("Invalid parameter", exprBegin);

                    AST_Expression* astExpr;
                    ParseExpression(exprBegin + 1, curExprLength - 1, scope, &astExpr);
                    GenericList_Append(&params, &astExpr);

                    // if (outFunc->parameters.count <= parameterIndex && !outFunc->variadicArguments)
                    //     ErrorAtToken("Invalid parameter count", exprBegin);

                    curExprLength = 0;
                }

                switch (exprBegin->type)
                {
                    case RBrOpen: inBracketR++; break;
                    case RBrClose: inBracketR--; break;
                    case CBrOpen: inBracketC++; break;
                    case CBrClose: inBracketC--; break;
                    case ABrOpen: inBracketA++; break;
                    case ABrClose: inBracketA--; break;
                    default: break;
                }

                curExprLength++;
                exprBegin--;
                if ((exprBegin) < exprEnd)
                    SyntaxErrorAtToken(b);
            }

        GenericList_ShrinkToSize(&params);
        AST_Expression_FunctionCall* retval = xmalloc(sizeof(AST_Expression_FunctionCall));
        retval->loc = Token_GetLocationP(&b[*i]);
        retval->type = AST_ExpressionType_FunctionCall;
        retval->id = identifier;

        retval->parameters = params.data;
        retval->numParameters = params.count;

        if (!outFunc)
            // Hacky cast here, but this works because of struct layout
            Optimizer_LogAccess((AST_Expression_VariableAccess*)retval);

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

static void ParsePrimaryExpression(Token* b, size_t length, size_t* i, Scope* scope, AST_Expression** outExpr)
{
    assert(*i < length);

    switch (b[*i].type)
    {
        case RBrOpen:
        {
            if (TryParseTypeCast(b, length, i, scope, (AST_Expression_TypeCast**)outExpr))
                break;
            P_Type_Inc(b, length, i);
            ParseExpressionPrec(b, length, i, scope, 0, outExpr);
            P_Type_PopCur(b, length, i, RBrClose);
            break;
        }
        case CBrOpen:
        {
            GenericList expressions = GenericList_Create(sizeof(AST_Expression*));
            P_Type_Inc(b, length, i);

            while (b[*i].type != CBrClose)
            {
                AST_Expression* outExpr;
                ParseExpressionPrec(b, length, i, scope, 0, &outExpr);
                GenericList_Append(&expressions, &outExpr);

                if (b[*i].type == Comma)
                    P_Type_Inc(b, length, i);
                else if (b[*i].type != CBrClose)
                    SyntaxErrorAtToken(&b[*i]);
            }
            P_Type_Inc(b, length, i);

            GenericList_ShrinkToSize(&expressions);

            AST_Expression_ListLiteral* retval = xmalloc(sizeof(AST_Expression_ListLiteral));
            retval->type = AST_ExpressionType_ListLiteral;
            retval->expressions = expressions.data;
            retval->numExpr = expressions.count;
            retval->loc = Token_GetLocationP(&b[0]);
            *outExpr = (AST_Expression*)retval;
            break;
        }
        case IntLiteral:
        {
            int32_t* literal = (int32_t*)b[*i].data;

            AST_Expression_IntLiteral* retval = xmalloc(sizeof(AST_Expression_IntLiteral));
            retval->type = AST_ExpressionType_IntLiteral;
            retval->literal = *literal;
            retval->loc = Token_GetLocationP(b);
            *outExpr = (void*)retval;
            P_Type_Inc(b, length, i);
            break;
        }
        case StringLiteral:
        {
            AST_Expression_StringLiteral* retval = xmalloc(sizeof(AST_Expression_StringLiteral));
            retval->type = AST_ExpressionType_StringLiteral;
            retval->data = b[*i].data;
            retval->loc = Token_GetLocationP(b);
            *outExpr = (void*)retval;
            P_Type_Inc(b, length, i);
            break;
        }
        case Identifier:
        {
            if (TryParseFunctionCall(b, length, i, scope, (AST_Expression_FunctionCall**)outExpr))
                break;

            AST_Expression_VariableAccess* retval = xmalloc(sizeof(AST_Expression_VariableAccess));
            retval->type = AST_ExpressionType_VariableAccess;
            retval->id = b[*i].data;
            retval->loc = Token_GetLocationP(b);
            *outExpr = (void*)retval;
            Optimizer_LogAccess(retval);
            P_Type_Inc(b, length, i);
            break;
        }
        case SizeOfKeyword:
        {
            P_Type_PopNextInc(b, length, i, RBrOpen);

            VariableType* type = ParseVariableType(b, i, length, scope, NULL, false);
            int size = SizeInWords(type);
            Type_RemoveReference(type);

            AST_Expression_IntLiteral* retval = xmalloc(sizeof(AST_Expression_IntLiteral));
            retval->type = AST_ExpressionType_IntLiteral;
            retval->loc = Token_GetLocationP(&b[0]);
            if (size < 1)
                ErrorAtToken("Invalid variable type", &b[0]);
            retval->literal = (int32_t)size;
            *outExpr = (AST_Expression*)retval;

            P_Type_PopCur(b, length, i, RBrClose);
            break;
        }
        default:
        {
            // Prefix Unary Ops
            TokenType type = b[*i].type;
            if (type >= BitwiseNOT && type <= Ampersand)
            {
                AST_Expression_UnOp* unop = xmalloc(sizeof(AST_Expression_UnOp));
                unop->type = AST_ExpressionType_UnaryOP;
                unop->loc = Token_GetLocationP(&b[*i]);
                unop->op = b[*i].type - BitwiseNOT;
                P_Type_Inc(b, length, i);
                ParseExpressionPrec(b, length, i, scope, 14 - 2, &unop->exprA);
                *outExpr = (AST_Expression*)unop;

                if (type == Ampersand && unop->exprA->type == AST_ExpressionType_VariableAccess)
                    Optimizer_LogAddrOf(((AST_Expression_VariableAccess*)unop->exprA)->id);

                break;
            }
            else
                SyntaxErrorAtToken(&b[*i]);
        }
    }
}

static void ParseExpressionPrec(Token* b, int length, size_t* i, Scope* scope, int minPrec, AST_Expression** outExpr)
{

    // Used for parsing binOps or postfix unOps
    const static char precTable[35] = {
        0x00 | (14 - 1),  // PostInc,
        0x00 | (14 - 1),  // PostDec,
        0x00 | (14 - 4),  // Plus,
        0x00 | (14 - 4),  // Minus,
        0x00 | (14 - 3),  // Star,
        0x00 | (14 - 8),  // Ampersand,
        0x00 | (14 - 3),  // Slash,
        0x00 | (14 - 10), // BitwiseOR,
        0x00 | (14 - 9),  // BitwiseXOR,
        0x00 | (14 - 5),  // ShiftLeft,
        0x00 | (14 - 5),  // ShiftRight,
        0x00 | (14 - 3),  // Percent,
        0x00 | (14 - 11), // LogicalAND,
        0x00 | (14 - 12), // LogicalOR,
        0x00 | (14 - 1),  // ABrOpen
        0x00 | (14 - 1),  // Dot,
        0x00 | (14 - 1),  // Arrow,
        0x00 | (14 - 6),  // LessThan,
        0x00 | (14 - 6),  // LessThanEq,
        0x00 | (14 - 6),  // GreaterThan,
        0x00 | (14 - 6),  // GreaterThanEq,
        0x00 | (14 - 7),  // Equals,
        0x00 | (14 - 7),  // NotEquals,
        0x80 | (14 - 14), // AssignmentAdd,
        0x80 | (14 - 14), // AssignmentSub,
        0x80 | (14 - 14), // AssignmentMul,
        0x80 | (14 - 14), // AssignmentDiv,
        0x80 | (14 - 14), // AssignmentAND,
        0x80 | (14 - 14), // AssignmentOR,
        0x80 | (14 - 14), // AssignmentXOR,
        0x80 | (14 - 14), // AssignmentShiftLeft,
        0x80 | (14 - 14), // AssignmentShiftRight,
        0x80 | (14 - 14), // AssignmentMod,
        0x80 | (14 - 14), // Assignment,
        0x80 | (14 - 13), // QuestionMark,
    };

    AST_Expression* left;
    ParsePrimaryExpression(b, length, i, scope, &left);

    SourceLocation loc = Token_GetLocationP(&b[*i]);

    // bool runtime = (Function_GetCurrent() != NULL);
    while (1)
    {
        Token next = b[*i];
        if (!(next.type >= Increment && next.type <= QuestionMark))
        {
            *outExpr = left;
            return;
        }

        char prec = precTable[next.type - Increment];
        bool rassoc = prec & 128;
        prec &= 127;

        if (prec < minPrec)
        {
            *outExpr = left;
            return;
        }

        P_Type_Inc(b, length, i);

        AST_Expression* right;
        if (next.type >= Increment && next.type <= Decrement)
        {
            // Postfix unary operators
            AST_Expression_UnOp* unop = xmalloc(sizeof(AST_Expression_UnOp));
            unop->type = AST_ExpressionType_UnaryOP;
            unop->loc = Token_GetLocationP(&b[*i]);
            unop->op = next.type - Increment + UnOp_PostIncrement;
            unop->exprA = left;
            *outExpr = (AST_Expression*)unop;
            
            left = (AST_Expression*)unop;
            continue;
        }
        else if (next.type == Arrow || next.type == Dot) 
        {
            // Only valid RHS for struct access is identifier
            AST_Expression_VariableAccess* rightAcc = xmalloc(sizeof(AST_Expression_VariableAccess));
            rightAcc->loc = Token_GetLocationP(&b[*i]);
            rightAcc->type = AST_ExpressionType_VariableAccess;
            rightAcc->id = P_Type_PopCur(b, length, i, Identifier); 
            right = (AST_Expression*)rightAcc;
        }
        else
        {
            // Regular Ops
            int subExPrec = rassoc ? prec : (prec + 1);
            if (next.type == ABrOpen)
                subExPrec = 0;
            ParseExpressionPrec(b, length, i, scope, subExPrec, &right);
        }

        // Ternary needs special handling
        if (next.type == QuestionMark)
        {
            P_Type_PopCur(b, length, i, Colon);
            AST_Expression_TernaryOp* op = xmalloc(sizeof(AST_Expression_TernaryOp));
            ParseExpressionPrec(b, length, i, scope, prec, &op->exprB);
            op->cond = left;
            op->exprA = right;
            op->loc = loc;
            op->type = AST_ExpressionType_TernaryOP;
            left = (AST_Expression*)op;
        }
        else
        {
            // could also check struct access right operand is identifier here?
            if (next.type == ABrOpen)
                P_Type_PopCur(b, length, i, ABrClose);

            AST_Expression_BinOp* op = xmalloc(sizeof(AST_Expression_BinOp));
            op->exprA = left;
            op->exprB = right;
            op->loc = loc;
            op->op = next.type - Plus;
            op->type = AST_ExpressionType_BinaryOP;
            left = (AST_Expression*)op;
        }
    }
}

void ParseExpression(Token* b, int length, Scope* scope, AST_Expression** outExpr)
{
    size_t i = 0;
    ParseExpressionPrec(b, length, &i, scope, 0, outExpr);
    if (i != (size_t)length)
        SyntaxErrorAtToken(&b[0]);

    // PrintExpressionTree(*outExpr, 0);
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
/*
void PrintExpressionTree(AST_Expression* expr, int indent)
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