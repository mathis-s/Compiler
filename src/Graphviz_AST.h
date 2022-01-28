#pragma once
#include "AST.h"
#include "Function.h"
#include <stdio.h>

int graphviz_id = 0;

int ExpressionASTGraphviz(const AST_Expression* expr)
{
    const char* expressionTypes[] = {
        "ListLiteral", "StringLiteral", "IntLiteral", "TypeCast", "UnaryOP",
        "BinaryOP",    "FuncCall",      "VarAccess",  "Value",
    };

    const char* binops[] = {
        "Add",
        "Sub",
        "Mul",
        "Div",
        "And",
        "Or",
        "Xor",
        "ShiftLeft",
        "ShiftRight",
        "Mod",
        "LogicalAnd",
        "LogicalOr",
        "ArrayAccess",
        "StructAccessDot",
        "StructAccessArrow",
        "LessThan",
        "LessThanEq",
        "GreaterThan",
        "GreaterThanEq",
        "Equal",
        "NotEqual",
        "AssignmentAdd",
        "AssignmentSub",
        "AssignmentMul",
        "AssignmentDiv",
        "AssignmentAND",
        "AssignmentOR",
        "AssignmentXOR",
        "AssignmentShl",
        "AssignmentShr",
        "AssignmentMod",
        "Assignment",
        "MulH",
        "MulQ",
        "InvQ",
    };

    const char* unops[] = {
        "PreIncrement", "PostIncrement", "PreDecrement", "PostDecrement", "LogicalNOT",
        "BitwiseNOT",   "Negate",        "Dereference",  "AddressOf",
    };

    int id = graphviz_id++;

    switch (expr->type)
    {
        case AST_ExpressionType_VariableAccess:
        {
            printf("%i[fontname=Helvetica,label=<<FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">Expression</FONT><br/>%s<br/><FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">\"%s\"</FONT>>,shape=rectangle];\n",
                   id, expressionTypes[expr->type], ((AST_Expression_VariableAccess*)expr)->id);
            break;
        }

        case AST_ExpressionType_IntLiteral:
        {
            printf("%i[fontname=Helvetica,label=<<FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">Expression</FONT><br/>%s<br/><FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">%i</FONT>>,shape=rectangle];\n",
                   id, expressionTypes[expr->type], ((AST_Expression_IntLiteral*)expr)->literal);
            break;
        }
        case AST_ExpressionType_BinaryOP:
        {
            AST_Expression_BinOp* binop = ((AST_Expression_BinOp*)expr);
            printf("%i[fontname=Helvetica,label=<<FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">Expression</FONT><br/>%s<br/><FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">%s</FONT>>,shape=rectangle];\n",
                   id, expressionTypes[expr->type], binops[binop->op]);
            int id_left = ExpressionASTGraphviz(binop->exprA);
            printf("%i->%i[arrowhead=none];\n", id, id_left);
            int id_right = ExpressionASTGraphviz(binop->exprB);
            printf("%i->%i[arrowhead=none];\n", id, id_right);
            break;
        }
        case AST_ExpressionType_UnaryOP:
        {
            AST_Expression_UnOp* unop = ((AST_Expression_UnOp*)expr);
            printf("%i[fontname=Helvetica,label=<<FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">Expression</FONT><br/>%s<br/><FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">%s</FONT>>,shape=rectangle];\n",
                   id, expressionTypes[expr->type], unops[unop->op]);
            int id_expr = ExpressionASTGraphviz(unop->exprA);
            printf("%i->%i[arrowhead=none];\n", id, id_expr);
            break;
        }
        case AST_ExpressionType_FunctionCall:
        {
            AST_Expression_FunctionCall* call = ((AST_Expression_FunctionCall*)expr);
            printf("%i[fontname=Helvetica,label=<<FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">Expression</FONT><br/>%s<br/><FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">\"%s\"</FONT>>,shape=rectangle];\n",
                   id, expressionTypes[expr->type], call->id);

            for (size_t i = 0; i < call->numParameters; i++)
            {
                int id_expr = ExpressionASTGraphviz(call->parameters[i]);
                printf("%i->%i[arrowhead=none];\n", id, id_expr);
            }
            break;
        }
        default:
        {
            printf("%i[fontname=Helvetica,label=<<FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">Expression</FONT><br/>%s>,shape=rectangle];\n",
                   id, expressionTypes[expr->type]);
            break;
        }
    }
    return id;
}

int StatementASTGraphviz(const AST_Statement* statement)
{
    const char* statementTypes[] = {
        "Expression", "If",     "While", "Do",    "For",      "Switch", "Declaration",
        "Scope",      "Return", "Empty", "Break", "Continue", "ASM",
    };
    int id = graphviz_id++;

    switch (statement->type)
    {
        case AST_StatementType_Expr:
        {
            printf("%i[fontname=Helvetica,label=<<FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">Statement</FONT><br/>%s>,shape=rectangle];\n",
                   id, statementTypes[statement->type]);
            int id_expr = ExpressionASTGraphviz(((AST_Statement_Expr*)statement)->expr);
            printf("%i->%i[arrowhead=none];\n", id, id_expr);
            break;
        }
        case AST_StatementType_If:
        {
            printf("%i[fontname=Helvetica,label=<<FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">Statement</FONT><br/>%s>,shape=rectangle];\n",
                   id, statementTypes[statement->type]);
            AST_Statement_If* stmt = (AST_Statement_If*)statement;
            int id_expr = ExpressionASTGraphviz(stmt->cond);
            int id_stmt = StatementASTGraphviz(stmt->ifTrue);
            int id_stmt_else = -1;
            if (stmt->ifFalse)
                id_stmt_else = StatementASTGraphviz(stmt->ifFalse);
            printf("%i->%i[arrowhead=none];\n", id, id_expr);
            // printf("%i->%i[arrowhead=none, label=\"cond\", fontname=Helvetica, fontcolor=\"gray50\",
            // fontsize=12];\n", id, id_expr);
            printf("%i->%i[arrowhead=none];\n", id, id_stmt);
            // printf("%i->%i[arrowhead=none, label=\"true\", fontname=Helvetica, fontcolor=\"gray50\",
            // fontsize=12];\n", id, id_stmt);
            if (id_stmt_else != -1)
                printf("%i->%i[arrowhead=none, label=\"true\"];\n", id, id_stmt_else);
            // printf("%i->%i[arrowhead=none, label=\"false\", fontname=Helvetica, fontcolor=\"gray50\",
            // fontsize=12];\n", id, id_stmt_else);
            break;
        }
        case AST_StatementType_While:
        case AST_StatementType_Do:
        {
            printf("%i[fontname=Helvetica,label=<<FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">Statement</FONT><br/>%s>,shape=rectangle];\n",
                   id, statementTypes[statement->type]);
            AST_Statement_While* stmt = (AST_Statement_While*)statement;
            int id_expr = ExpressionASTGraphviz(stmt->cond);
            int id_stmt = StatementASTGraphviz(stmt->body);
            printf("%i->%i[arrowhead=none];\n", id, id_expr);
            printf("%i->%i[arrowhead=none];\n", id, id_stmt);
            break;
        }
        case AST_StatementType_For:
        {
            printf("%i[fontname=Helvetica,label=<<FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">Statement</FONT><br/>%s>,shape=rectangle];\n",
                   id, statementTypes[statement->type]);
            AST_Statement_For* stmt = (AST_Statement_For*)statement;

            int id_init = StatementASTGraphviz(stmt->init);
            int id_expr = ExpressionASTGraphviz(stmt->cond);
            int id_inc = ExpressionASTGraphviz(stmt->count);
            int id_stmt = StatementASTGraphviz(stmt->body);

            printf("%i->%i[arrowhead=none];\n", id, id_init);
            printf("%i->%i[arrowhead=none];\n", id, id_expr);
            printf("%i->%i[arrowhead=none];\n", id, id_inc);
            printf("%i->%i[arrowhead=none];\n", id, id_stmt);
            break;
        }
        case AST_StatementType_Declaration:
        {
            AST_Statement_Declaration* stmt = (AST_Statement_Declaration*)statement;
            printf("%i[fontname=Helvetica,label=<<FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">Statement</FONT><br/>%s<br/><FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">\"%s\"</FONT>>,shape=rectangle];\n",
                   id, statementTypes[statement->type], stmt->variableName);
            if (stmt->value)
            {
                int id_expr = ExpressionASTGraphviz(stmt->value);
                printf("%i->%i[arrowhead=none];\n", id, id_expr);
            }
            break;
        }
        case AST_StatementType_Scope:
        {
            printf("%i[fontname=Helvetica,label=<<FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">Statement</FONT><br/>%s>,shape=rectangle];\n",
                   id, statementTypes[statement->type]);
            AST_Statement_Scope* scope = (AST_Statement_Scope*)statement;

            for (size_t i = 0; i < scope->numStatements; i++)
            {
                int id_stmt = StatementASTGraphviz(scope->statements[i]);
                printf("%i->%i[arrowhead=none];\n", id, id_stmt);
            }
            break;
        }
        case AST_StatementType_Return:
        {
            AST_Statement_Return* stmt = (AST_Statement_Return*)statement;
            printf("%i[fontname=Helvetica,label=<<FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">Statement</FONT><br/>%s<br/>>,shape=rectangle];\n",
                   id, statementTypes[statement->type]);
            if (stmt->expr)
            {
                int id_expr = ExpressionASTGraphviz(stmt->expr);
                printf("%i->%i[arrowhead=none];\n", id, id_expr);
            }
            break;
        }
        default:
        {
            printf("%i[fontname=Helvetica,label=<<FONT POINT-SIZE=\"8\" "
                   "color=\"gray50\">Statement</FONT><br/>%s<br/>>,shape=rectangle];\n",
                   id, statementTypes[statement->type]);
            break;
        }
    }
    return id;
}

int FunctionASTGraphviz(const Function* func, GenericList statements)
{
    int id = graphviz_id++;
    printf("%i[fontname=Helvetica,label=<Function<br/><FONT POINT-SIZE=\"8\" "
           "color=\"gray50\">\"%s\"</FONT>>,shape=rectangle];\n",
           id, func->identifier);

    for (size_t i = 0; i < statements.count; i++)
    {
        int id_stmt = StatementASTGraphviz(*((AST_Statement**)GenericList_At(&statements, i)));
        printf("%i->%i[arrowhead=none];\n", id, id_stmt);
    }
    return id;
}