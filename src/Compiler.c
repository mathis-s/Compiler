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
    // Declaration (and Assigment)
    if (IsDataToken(t->tokens[*i], globalScope))
    {
        int oldI = *i;
        char* id = NULL;

        VariableType* type = ParseVariableType(t->tokens, i, t->curLength, globalScope, &id, false);
        if (type == NULL)
        {
            *i = oldI;
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

            if (size != -1 && v.value.size != size)
                ErrorAtIndex("Invalid size", oldI);

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

            if (size == -1)
                ErrorAtIndex("Invalid global variable", *i);
            v.value.address = (int32_t)AllocateGlobalValue(size);
            v.value.size = size;
            v.value.addressType = AddressType_Memory;
        }

        Inc(i);

        if (Scope_NameIsUsed(globalScope, id))
            ErrorAtIndex("Identifier already used!", *i);

        Scope_AddVariable(globalScope, v);
        return true;
    }

    return false;
}

void CompileStructDeclaration(TokenArray* t, size_t* i, Scope* scope)
{
    // Struct declaration
    Struct s;
    bool typedefDecl = false;
    // CPP style struct name {};
    if (t->tokens[*i].type == StructKeyword)
    {
        PopNext(i, Identifier);
        s.identifier = t->tokens[(*i)++].data;
    }
    // Or typedef struct {} name; No anonymous structs!
    else if ((*i + 1 < t->curLength) && t->tokens[*i].type == TypedefKeyword && t->tokens[*i + 1].type == StructKeyword)
    {
        (*i)++;
        Inc(i);

        typedefDecl = true;

        if (t->tokens[*i].type == Identifier)
        {
            s.identifier = t->tokens[(*i)].data;
            Inc(i);
        }
    }
    else
        return;

    s.members = GenericList_Create(sizeof(Variable));
    s.sizeInWords = 0;

    // Forward declaration
    if (t->tokens[*i].type == Semicolon)
    {
        Struct* outStruct = NULL;
        if ((outStruct = Scope_FindStruct(scope, s.identifier)) != NULL)
        {
            if (outStruct->sizeInWords != 0)
                ErrorAtIndex("Identifier already used", *i);
        }
        Inc(i);
        Scope_AddStruct(scope, s);
        return;
    }

    PopCur(i, CBrOpen);

    while (t->tokens[*i].type != CBrClose)
    {
        char* id = NULL;
        VariableType* type = ParseVariableType(t->tokens, i, t->curLength, scope, &id, false);

        if (type == NULL || SizeInWords(type) == 0)
            ErrorAtIndex("Invalid type!", *i);

        Variable v = (Variable){type, id, {(int32_t)s.sizeInWords, AddressType_StructMember, SizeInWords(type)}, NULL};

        GenericList_Append(&s.members, &v);

        if (t->tokens[(*i)++].type != Semicolon)
            SyntaxErrorAtIndex(*i);
        s.sizeInWords += v.value.size;
    }
    Inc(i);

    if (typedefDecl)
    {
        if (t->tokens[(*i)].type != Identifier)
            SyntaxErrorAtIndex(*i);
        s.identifier = t->tokens[(*i)].data;
        Inc(i);
    }

    Struct* outStruct = NULL;
    if ((outStruct = Scope_FindStruct(scope, s.identifier)) != NULL)
    {
        // Complete forward declaration if it is one
        if (outStruct->sizeInWords == 0)
        {
            GenericList_Dispose(&outStruct->members);
            *outStruct = s;
        }
        else
            ErrorAtIndex("Identifier already used", *i);
    }
    else
    {
        if (Scope_NameIsUsed(scope, s.identifier))
            ErrorAtIndex("Identifier already used", *i);

        Scope_AddStruct(scope, s);
    }

    if (t->tokens[*i].type != Semicolon)
        SyntaxErrorAtIndex(*i);
    (*i)++;
}

static void CompileEnumDeclaration(TokenArray* t, size_t* i, Scope* scope)
{
    Enum e;
    bool typedefDecl = false;
    // Enum declaration
    if (t->tokens[*i].type == EnumKeyword)
    {
        PopNext(i, Identifier);
        e.identifier = t->tokens[(*i)++].data;
    }
    else if (t->tokens[*i].type == TypedefKeyword && t->tokens[(*i) + 1].type == EnumKeyword)
    {
        (*i) += 2;
        typedefDecl = true;
    }
    else
        return;

    PopCur(i, CBrOpen);

    int currentId = 0;

    while (t->tokens[*i].type != CBrClose)
    {
        if (t->tokens[(*i)].type != Identifier)
            SyntaxErrorAtIndex(*i);
        char* id = t->tokens[*i].data;

        Inc(i);
        if (t->tokens[(*i)].type == Assignment)
        {
            PopNext(i, IntLiteral);
            currentId = (uint16_t)(*((uint32_t*)t->tokens[*i].data));
            Inc(i);
        }

        Variable v = {Type_AddReference(&MachineUIntType), id, Value_Literal((int32_t)currentId++), NULL};

        Scope_AddVariable(scope, v);

        if (t->tokens[(*i)].type == Comma)
            Inc(i);
        else if (t->tokens[(*i)].type != CBrClose)
            SyntaxErrorAtIndex(*i);
    };
    Inc(i);

    if (typedefDecl)
    {
        if (t->tokens[(*i)].type != Identifier)
            SyntaxErrorAtIndex(*i);
        e.identifier = t->tokens[*i].data;
        Inc(i);
    }

    Scope_AddEnum(scope, e);

    if (t->tokens[*i].type != Semicolon)
        SyntaxErrorAtIndex(*i);
    (*i)++;
}

static bool CodeGenFunction(TokenArray* t, size_t* i, Scope* globalScope)
{
    size_t oldI = *i;

    VariableType* returnType = ParseVariableType(t->tokens, i, t->curLength, globalScope, NULL, true);
    if (returnType == NULL)
    {
        *i = oldI;
        return false;
    }

    if (t->tokens[(*i)].type != Identifier)
        ErrorAtIndex("Expected identifier", *i);
    char* identifier = t->tokens[*i].data;
    if (Scope_NameIsUsed(globalScope, identifier))
        ErrorAtIndex("Identifier already used", *i);

    Inc(i);

    if (t->tokens[*i].type != RBrOpen)
    {
        Type_RemoveReference(returnType);
        *i = oldI;
        return false;
    }
    Inc(i);

    bool variadicArguments = false;
    GenericList parameters = GenericList_Create(sizeof(Variable));

    int index = 1;
    // Parse Parameter Types
    while (true)
    {
        if (IsDataToken(t->tokens[*i], globalScope))
        {
            char* id = NULL;

            VariableType* vt = ParseVariableType(t->tokens, i, t->curLength, globalScope, &id, false);
            int size = 0;
            if (vt == NULL || (size = SizeInWords(vt)) == -1)
                ErrorAtIndex("Invalid type", *i);

            index += size;
            Variable var = (Variable){vt, id, (Value){(int32_t)index, AddressType_MemoryRelative, size}, NULL};
            GenericList_Append(&parameters, &var);
        }
        else if (t->tokens[*i].type == DotDotDot)
        {
            variadicArguments = true;
            PopNext(i, RBrClose);
            break;
        }
        else if (t->tokens[*i].type == Comma)
        {
            Inc(i);
            continue;
        }
        else if (t->tokens[*i].type == RBrClose)
            break;
        else
            ErrorAtIndex("Invalid parameter type", *i);
    }

    PopCur(i, RBrClose);
    Function newFunc = (Function){identifier, parameters, returnType, 0, variadicArguments, false};
    Function* outFunc = NULL;
    Function* function;

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
            if (!Type_Check(oldParam->type, newParam->type))
                ErrorAtIndex("Identifier already used", *i);

            assert(Type_RemoveReference(oldParam->type));
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

        // if (strcmp(function->identifier, "main") == 0)
        //  FunctionASTGraphviz(function, statements);

        { // Code Generation
            OutWrite("_%s:\n", identifier);
            for (size_t j = 0; j < statements.count; j++)
            {
                CodeGen_Statement(*((AST_Statement**)GenericList_At(&statements, j)), &functionScope);
            }
        }

        GenericList_Dispose(&statements);
        Function_SetCurrent(NULL);

        if (returnType->token == VoidKeyword && returnType->pointerLevel == 0)
        {
            Stack_ToAddress(Stack_GetSize() + 1);
            OutWrite("mov ip, [sp]\nnop\n");
        }

        if (t->tokens[*i].type != CBrClose)
            SyntaxErrorAtIndex(*i);
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
    OutWrite("jmp _main\nnop\n__term_loop:\njmp __term_loop\nnop\n");
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
        if (i >= t.curLength)
            break;
        CompileStructDeclaration(&t, &i, &globalScope);
        if (i >= t.curLength)
            break;
        CompileEnumDeclaration(&t, &i, &globalScope);
        if (i >= t.curLength)
            break;
        CodeGenFunction(&t, &i, &globalScope);

        if (i == oldI)
            SyntaxErrorAtIndex(i);
        oldI = i;
    }

    Scope_Dispose(&globalScope);
    Function_DeleteFunctions();
}