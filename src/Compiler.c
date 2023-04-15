#include "Compiler.h"
#include "AST.h"
#include "CodeGeneration/CG_Expression.h"
#include "CodeGeneration/CG_Statement.h"
#include "Data.h"
#include "Error.h"
#include "Function.h"
#include "GenericList.h"
#include "Optimizer.h"
#include "Outfile.h"
#include "Parser/P_Expression.h"
#include "Parser/P_Statement.h"
#include "Parser/P_Type.h"
#include "Register.h"
#include "Scope.h"
#include "Stack.h"
#include "Struct.h"
#include "Token.h"
#include "Type.h"
#include "Value.h"
#include "Variables.h"
//#include "Graphviz_AST.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool CompileGlobalVariable(TokenArray* t, size_t* i, Scope* globalScope)
{
    // Declaration (and Assignment)
    if (IsDataToken(t->tokens[*i], globalScope))
    {
        int oldI = *i;
        char* id = NULL;

        VariableType* type = ParseVariableType(t->tokens, i, t->curLength, globalScope, &id, false);
        if (type == NULL || type->token == FunctionPointerToken)
        {
            *i = oldI;
            return false;
        }
        if (id == NULL)
        {
            PopCur(i, Semicolon);
            return false;
        }

        int size = SizeInWords(type);
        Variable v = {type, id, (Value){(int32_t)GetGlobalDataIndex(), AddressType_Memory, size}, NULL};

        if (t->tokens[(*i)].type == Assignment)
        {
            Inc(i);

            AST_Expression* outExpr;
            ParseNextExpression(t, i, globalScope, &outExpr);

            if (t->tokens[(*i)].type != Semicolon)
            {
                Type_RemoveReference(type);
                *i = oldI;
                return false;
            }

            VariableType* outType;
            bool outReadOnly;
            CodeGen_Expression(outExpr, globalScope, &v.value, &outType, &outReadOnly);

            // if(!VariableTypeCheck(outType, type))
            //     ErrorAtIndex("Invalid type", oldI);

            Type_RemoveReference(outType);

            if (size != -1 && v.value.size != size) ErrorAtIndex("Invalid size", oldI);

            if (v.value.addressType != AddressType_Literal && v.value.addressType != AddressType_Memory)
                ErrorAtIndex("Global variable must be compile-time constant", oldI);

            if (v.value.addressType == AddressType_Literal && !(type->qualifiers & Qualifier_Const))
            {
                v.value.addressType = AddressType_Memory;
                if (size == 1)
                    v.value.address = (int32_t)AllocateGlobalWord((uint16_t)v.value.address);
                else if (size == 2)
                    v.value.address = (int32_t)AllocateGlobalDoubleWord(v.value.address);
                else
                    assert(0);
            }
        }
        else
        {
            if (t->tokens[(*i)].type != Semicolon)
            {
                Type_RemoveReference(type);
                *i = oldI;
                return false;
            }

            if (size == -1) ErrorAtIndex("Invalid global variable", *i);
            v.value.address = (int32_t)AllocateGlobalValue(size);
            v.value.size = size;
            v.value.addressType = AddressType_Memory;
        }

        Inc(i);

        if (Scope_NameIsUsed(globalScope, id)) ErrorAtIndex("Identifier already used!", *i);

        Scope_AddVariable(globalScope, v);
        return true;
    }

    return false;
}

bool CompileTypedef(TokenArray* t, size_t* i, Scope* scope)
{
    if (t->tokens[*i].type != TypedefKeyword) return false;
    Inc(i);

    VariableType* type = ParseVariableType(t->tokens, i, t->maxLength, scope, NULL, true);

    if (type == NULL) ErrorAtIndex("Invalid type", *i);

    if (t->maxLength - *i < 2 || t->tokens[*i].type != Identifier || t->tokens[*i + 1].type != Semicolon)
    {
        Type_RemoveReference(type);
        return false;
    }

    const char* id = t->tokens[*i].data;
    
    Typedef* old;
    if ((old = Scope_FindTypedef(scope, (char*)id)))
    {
        // FIXME: this should only work if old is an incomplete type AND only with typedefs in current scope
        Type_RemoveReference(old->type);
        old->type = type;
    }
    else
        Scope_AddTypedef(scope, (Typedef){type, id});
    *i += 2;
    return true;
}

static bool CodeGenFunction(TokenArray* t, size_t* i, Scope* globalScope)
{
    size_t oldI = *i;

    char* identifier = NULL;
    VariableTypeFunctionPointer* funcType =
        (VariableTypeFunctionPointer*)ParseVariableType(t->tokens, i, t->curLength, globalScope, &identifier, true);
    if (funcType == NULL || funcType->token != FunctionPointerToken)
    {
        *i = oldI;
        return false;
    }

    VariableType* returnType = funcType->func.returnType;

    if (identifier == NULL) SyntaxErrorAtIndex(*i);
    if (Scope_NameIsUsed(globalScope, identifier)) ErrorAtIndex("Identifier already used", *i);

    Function newFunc = funcType->func;
    newFunc.identifier = identifier;
    Function* outFunc = NULL;
    Function* function;

    free(funcType); // hacky, bypass reference counting

    // Overwriting a forward declaration
    if ((outFunc = Function_Find(identifier)) != NULL)
    {
        if (!outFunc->isForwardDecl || outFunc->parameters.count != newFunc.parameters.count)
            ErrorAtIndex("Identifier already used", *i);

        // Checking types
        for (size_t j = 0; j < outFunc->parameters.count; j++)
        {
            Variable* oldParam = ((Variable*)GenericList_At(&outFunc->parameters, j));
            Variable* newParam = ((Variable*)GenericList_At(&newFunc.parameters, j));
            if (!Type_Check(oldParam->type, newParam->type)) ErrorAtIndex("Identifier already used", *i);

            Type_RemoveReference(oldParam->type);
        }
        Type_RemoveReference(outFunc->returnType);
        GenericList_Dispose(&outFunc->parameters);

        function = outFunc;
        *function = newFunc;
    }
    // Normal declaration
    else
        function = Function_Add(newFunc);

    // Function w/o body
    if (t->tokens[*i].type == Semicolon)
    {
        function->isForwardDecl = true;
        function->modifiedRegisters = 0xFFFF;
        Inc(i);
    }
    else
    {
        Function_SetCurrent(function);

        GenericList parameters = function->parameters;
        Scope functionScope = Scope_Create(globalScope);
        GenericList_Dispose(&functionScope.variables);
        functionScope.variables = GenericList_CreateCopy(parameters);

        for (size_t i = 0; i < parameters.count; i++)
        {
            Variable* param = GenericList_At(&parameters, i);
            param->type->refCount++;
        }

        GenericList statements = GenericList_Create(sizeof(AST_Statement*));
        { // Parse
            PopCur(i, CBrOpen);
            Optimizer_EnterNewScope();

            AST_Statement* outStmt;
            while (t->tokens[*i].type != CBrClose)
            {
                ParseStatement(t, i, &functionScope, &outStmt);
                GenericList_Append(&statements, &outStmt);
            }
            Optimizer_ExitScope(&functionScope.preferredRegisters[0]);
            Registers_SetPreferred(&functionScope.preferredRegisters[0]);
        }

        // if (strcmp(function->identifier, "Value_GenerateMemCpy") == 0)
        //     FunctionASTGraphviz(function, statements);

        { // Code Generation
            OutWrite("_%s:\n", identifier);
            for (size_t j = 0; j < statements.count; j++)
            {
                CodeGen_Statement(*((AST_Statement**)GenericList_At(&statements, j)), &functionScope);
            }
        }

        GenericList_Dispose(&statements);
        Function_SetCurrent(NULL);

        if (returnType->token == VoidKeyword)
        {
            Stack_ToAddress(Stack_GetSize() + 1);
            OutWrite("mov ip, [sp]\n");
        }

        if (t->tokens[*i].type != CBrClose) SyntaxErrorAtIndex(*i);
        // The increment here is purposefully unsafe, as the CBrClose might
        // have been the last token
        (*i)++;

        // All function variables are now out of scope
        Registers_FreeAll();

        Stack_SetSize(0);
        Stack_SetOffset(0);

        Scope_Dispose(&functionScope);
    }
    return true;
}

static bool generatedHeader = false;
static void GenerateHeader()
{
    OutWrite("add [sp++], ip, 2\n");
    OutWrite("jmp _main\n__term_loop:\njmp __term_loop\n");
}

void Compile(TokenArray t)
{
    Scope globalScope = Scope_Create(NULL);

    if (!generatedHeader)
    {
        GenerateHeader();
        generatedHeader = true;
    }

    Function_InitFunctions();

    size_t i = 0;
    size_t oldI = 0;

    while (i < t.curLength)
    {
        CompileGlobalVariable(&t, &i, &globalScope);
        if (i >= t.curLength) break;
        CompileTypedef(&t, &i, &globalScope);
        if (i >= t.curLength) break;
        CodeGenFunction(&t, &i, &globalScope);

        if (i == oldI) SyntaxErrorAtIndex(i);
        oldI = i;
    }

    Scope_Dispose(&globalScope);
    Function_DeleteFunctions();
}