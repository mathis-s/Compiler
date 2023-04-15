#include "P_Statement.h"
#include "../AST.h"
#include "../CodeGeneration/CG_Expression.h"
#include "../Function.h"
#include "../Optimizer.h"
#include "../Scope.h"
#include "../Token.h"
#include "../Util.h"
#include "P_Expression.h"
#include "P_Type.h"
#include <stdbool.h>

static bool TryParseReturnStatement(TokenArray* t, size_t* i, Scope* scope, AST_Statement_Return** outStmt)
{
    if (t->tokens[(*i)].type == ReturnKeyword)
    {
        VariableType* returnType = Function_GetCurrent()->returnType;
        AST_Statement_Return* retval = xmalloc(sizeof(AST_Statement_Return));
        retval->loc = Token_GetLocation(*i);

        Inc(i);

        retval->type = AST_StatementType_Return;
        if (IsDataType(returnType))
        {
            AST_Expression* expr;
            ParseNextExpression(t, i, scope, &expr);

            retval->expr = expr;
        }
        else
            retval->expr = NULL;

        *outStmt = retval;

        return true;
    }
    return false;
}

static bool TryParseIfStatement(TokenArray* t, size_t* i, Scope* scope, AST_Statement_If** outStmt)
{
    if (t->tokens[*i].type == IfKeyword)
    {
        AST_Statement_If* retval = xmalloc(sizeof(AST_Statement_If));
        retval->type = AST_StatementType_If;
        retval->loc = Token_GetLocation(*i);

        PopNextInc(i, RBrOpen);

        ParseNextExpressionWithSeparator(t, i, scope, &retval->cond, RBrClose, 1);

        Inc(i);

        ParseStatement(t, i, scope, &retval->ifTrue);

        if (t->tokens[*i].type == ElseKeyword)
        {
            Inc(i);
            ParseStatement(t, i, scope, &retval->ifFalse);
        }
        else
            retval->ifFalse = NULL;

        *outStmt = retval;

        return true;
    }

    return false;
}

static bool TryParseWhileLoop(TokenArray* t, size_t* i, Scope* scope, AST_Statement_While** outStmt)
{
    if (t->tokens[*i].type == WhileKeyword)
    {
        Optimizer_EnterLoop();
        AST_Statement_While* retval = xmalloc(sizeof(AST_Statement_While));
        retval->type = AST_StatementType_While;
        retval->loc = Token_GetLocation(*i);

        PopNextInc(i, RBrOpen);

        ParseNextExpressionWithSeparator(t, i, scope, &retval->cond, RBrClose, 1);

        PopCur(i, RBrClose);

        ParseStatement(t, i, scope, &retval->body);

        Optimizer_ExitLoop(retval);
        *outStmt = retval;

        return true;
    }
    return false;
}

static bool TryParseDoWhileLoop(TokenArray* t, size_t* i, Scope* scope, AST_Statement_Do** outStmt)
{
    if (t->tokens[*i].type == DoKeyword)
    {
        Optimizer_EnterLoop();
        AST_Statement_Do* retval = xmalloc(sizeof(AST_Statement_Do));
        retval->type = AST_StatementType_Do;
        retval->loc = Token_GetLocation(*i);

        Inc(i);

        ParseStatement(t, i, scope, &retval->body);

        PopCur(i, WhileKeyword);
        PopCur(i, RBrOpen);

        ParseNextExpressionWithSeparator(t, i, scope, &retval->cond, RBrClose, 1);

        PopNextInc(i, Semicolon);

        Optimizer_ExitLoop(retval);
        *outStmt = retval;

        return true;
    }
    return false;
}

static bool TryParseForLoop(TokenArray* t, size_t* i, Scope* scope, AST_Statement_For** outExpr)
{
    if (t->tokens[*i].type == ForKeyword)
    {
        AST_Statement_For* retval = xmalloc(sizeof(AST_Statement_For));
        retval->type = AST_StatementType_For;
        retval->loc = Token_GetLocation(*i);
        Optimizer_EnterNewScope();
        retval->statementScope = xmalloc(sizeof(Scope));
        *retval->statementScope = Scope_Create(scope);

        PopNextInc(i, RBrOpen);

        ParseStatement(t, i, retval->statementScope, &retval->init);

        Optimizer_EnterLoop();

        ParseNextExpression(t, i, retval->statementScope, &retval->cond);
        Inc(i);
        ParseNextExpressionWithSeparator(t, i, retval->statementScope, &retval->count, RBrClose, 1);
        Inc(i);
        ParseStatement(t, i, retval->statementScope, &retval->body);

        Optimizer_ExitLoop(retval);
        Optimizer_ExitScope(&retval->statementScope->preferredRegisters[0]);
        *outExpr = retval;
        return true;
    }
    
    return false;
}

bool TryParseNewScope(TokenArray* t, size_t* i, Scope* scope, AST_Statement_Scope** outStmt)
{
    if (t->tokens[*i].type == CBrOpen)
    {
        Optimizer_EnterNewScope();

        AST_Statement_Scope* retval = xmalloc(sizeof(AST_Statement_Scope));
        retval->type = AST_StatementType_Scope;
        retval->loc = Token_GetLocation(*i);

        GenericList statements = GenericList_Create(sizeof(AST_Statement*));
        Scope* newScope = xmalloc(sizeof(Scope));
        *newScope = Scope_Create(scope);

        Inc(i);

        while (t->tokens[(*i)].type != CBrClose)
        {
            AST_Statement* stmt;
            ParseStatement(t, i, newScope, &stmt);
            GenericList_Append(&statements, &stmt);
        }

        PopCur(i, CBrClose);

        GenericList_ShrinkToSize(&statements);
        retval->statements = statements.data;
        retval->numStatements = statements.count;
        retval->scope = newScope;

        Optimizer_ExitScope(&newScope->preferredRegisters[0]);
        *outStmt = retval;
        return true;
    }

    return false;
}

static bool TryParseSwitchCase(TokenArray* t, size_t* i, Scope* scope, AST_Statement_Switch** outStmt)
{
    if (t->tokens[*i].type == SwitchKeyword)
    {
        AST_Statement_Switch* retval = xmalloc(sizeof(AST_Statement_Switch));
        retval->type = AST_StatementType_Switch;
        retval->loc = Token_GetLocation(*i);
        retval->defaultCaseStmts = NULL;
        retval->numStmtsDefCase = 0;

        PopNextInc(i, RBrOpen);

        ParseNextExpressionWithSeparator(t, i, scope, &retval->selector, RBrClose, 1);

        PopNextInc(i, CBrOpen);

        Optimizer_EnterLoop();
        GenericList cases = GenericList_Create(sizeof(AST_Statement_Switch_SwitchCase));

        while (t->tokens[*i].type == CaseKeyword || t->tokens[*i].type == DefaultKeyword)
        {
            bool isDefaultCase = false;
            AST_Statement_Switch_SwitchCase c;
            GenericList list = GenericList_Create(sizeof(AST_Expression**));
            if (t->tokens[*i].type != DefaultKeyword)
            {
                Inc(i);
                // if (t->tokens[*i].type != IntLiteral)
                //     ErrorAtIndex("Expected int literal", *i);

                // (this might break some things, we're already cg-ing in the parser)
                AST_Expression* outExpr;
                ParseNextExpressionWithSeparator(t, i, scope, &outExpr, Colon, 0);
                Value outValue = NullValue;

                bool outReadOnly;
                CodeGen_Expression(outExpr, scope, &outValue, NULL, &outReadOnly);

                if (outValue.addressType != AddressType_Literal || outValue.size != 1)
                    ErrorAtIndex("Expected int16 constant", *i);

                c.id = (int)outValue.address;

                PopCur(i, Colon);
            }
            else
            {
                isDefaultCase = true;
                PopNextInc(i, Colon);
            }

            while (t->tokens[*i].type != CaseKeyword && t->tokens[*i].type != CBrClose &&
                   t->tokens[*i].type != DefaultKeyword)
            {

                AST_Statement* stmt;
                ParseStatement(t, i, scope, &stmt);
                GenericList_Append(&list, &stmt);

                // if (++(*i) >= t->curLength)
                //     SyntaxErrorAtIndex((*i) - 1);
            }

            GenericList_ShrinkToSize(&list);
            if (isDefaultCase)
            {
                retval->defaultCaseStmts = list.data;
                retval->numStmtsDefCase = list.count;
            }
            else
            {
                c.statements = list.data;
                c.numStatements = list.count;
                GenericList_Append(&cases, &c);
            }
        }

        Optimizer_ExitLoop(retval);

        GenericList_ShrinkToSize(&cases);

        retval->cases = cases.data;
        retval->numCases = cases.count;

        *outStmt = retval;

        PopCur(i, CBrClose);

        return true;
    }
    return false;
}

static bool TryParseDeclaration(TokenArray* t, size_t* i, Scope* scope, AST_Statement_Declaration** outStmt)
{
    if (IsDataToken(t->tokens[*i], scope))
    {
        AST_Statement_Declaration* retval = xmalloc(sizeof(AST_Statement_Declaration));
        retval->loc = Token_GetLocation(*i);
        retval->type = AST_StatementType_Declaration;

        char* identifier;
        VariableType* type = ParseVariableType(t->tokens, i, t->curLength, scope, &identifier, false);

        if (identifier == NULL)
        {
            PopCur(i, Semicolon);
            return true;
        }

        if (type == NULL)
            ErrorAtToken("Invalid variable type!", &t->tokens[*i]);

        if (Scope_NameIsUsed(scope, identifier))
            ErrorAtIndex("Identifier already used!", *i);

        retval->variableName = identifier;

        retval->variableType = type;

        if (t->tokens[(*i)].type == Assignment)
        {
            Inc(i);
            ParseNextExpression(t, i, scope, &retval->value);
        }
        else
            retval->value = NULL;

        Optimizer_LogDeclaration(retval);
        *outStmt = retval;

        return true;
    }
    return false;
}

void ParseStatement(TokenArray* t, size_t* i, Scope* scope, AST_Statement** outStmt)
{
    switch (t->tokens[*i].type)
    {
        // Most Statements are already uniquely identified by the first token
        case ReturnKeyword:
            if (!TryParseReturnStatement(t, i, scope, (AST_Statement_Return**)outStmt))
                SyntaxErrorAtIndex(*i);
            PopCur(i, Semicolon);
            break;
        case IfKeyword:
            if (!TryParseIfStatement(t, i, scope, (AST_Statement_If**)outStmt))
                SyntaxErrorAtIndex(*i);
            break;
        case WhileKeyword:
            if (!TryParseWhileLoop(t, i, scope, (AST_Statement_While**)outStmt))
                SyntaxErrorAtIndex(*i);
            break;
        case ForKeyword:
            if (!TryParseForLoop(t, i, scope, (AST_Statement_For**)outStmt))
                SyntaxErrorAtIndex(*i);
            break;
        case DoKeyword:
            if (!TryParseDoWhileLoop(t, i, scope, (AST_Statement_Do**)outStmt))
                SyntaxErrorAtIndex(*i);
            break;
        case SwitchKeyword:
            if (!TryParseSwitchCase(t, i, scope, (AST_Statement_Switch**)outStmt))
                SyntaxErrorAtIndex(*i);
            break;
        case CBrOpen:
            if (!TryParseNewScope(t, i, scope, (AST_Statement_Scope**)outStmt))
                SyntaxErrorAtIndex(*i);
            break;
        case Semicolon:
        {
            AST_Statement* stmt = xmalloc(sizeof(AST_Statement));
            stmt->type = AST_StatementType_Empty;
            stmt->loc = Token_GetLocation(*i);
            *outStmt = stmt;
            Inc(i);
            break;
        }
        case AsmKeyword:
        {
            AST_Statement_ASM* stmt = xmalloc(sizeof(AST_Statement_ASM));
            Optimizer_LogInlineASM(stmt);
            stmt->type = AST_StatementType_ASM;
            stmt->code = t->tokens[*i].data;
            *outStmt = (AST_Statement*)stmt;
            Inc(i);
            break;
        }
        case ContinueKeyword:
        case BreakKeyword:
        {
            AST_Statement* stmt = xmalloc(sizeof(AST_Statement));

            if (t->tokens[*i].type == ContinueKeyword)
                stmt->type = AST_StatementType_Continue;
            else
                stmt->type = AST_StatementType_Break;

            stmt->loc = Token_GetLocation(*i);
            *outStmt = stmt;
            PopNextInc(i, Semicolon);
            break;
        }
        default:
        {
            if (TryParseDeclaration(t, i, scope, (AST_Statement_Declaration**)outStmt))
            {
                PopCur(i, Semicolon);
            }
            else
            {
                AST_Statement_Expr* stmt = xmalloc(sizeof(AST_Statement_Expr));
                stmt->type = AST_StatementType_Expr;
                ParseNextExpression(t, i, scope, &stmt->expr);
                *outStmt = (AST_Statement*)stmt;

                PopCur(i, Semicolon);
            }
            break;
        }
    }
}