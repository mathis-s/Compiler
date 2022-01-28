#include "Optimizer.h"
#include "AST.h"
#include "GenericList.h"
#include "Type.h"
#include "Util.h"
#include "Value.h"
#include "Variables.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
    const char* id;
    int16_t lastScore;
    int16_t currentScore;
    int definedAtLoopLevel;
    void* lastAccess;
    bool isPointer;
    AST_Statement_Declaration* declaration;

} Optimizer_Variable;

#ifndef CUSTOM_COMP
typedef struct Optimizer_Scope Optimizer_Scope;
typedef struct Optimizer_Loop Optimizer_Loop;
#endif
#ifdef CUSTOM_COMP
struct Optimizer_Scope;
struct Optimizer_Loop;
#endif
typedef struct Optimizer_Scope
{
    Optimizer_Scope* parent;
    GenericList variables;
    uint16_t preferredRegisters[8];
} Optimizer_Scope;

typedef struct Optimizer_Loop
{
    Optimizer_Loop* parent;
    GenericList accessedVars;
} Optimizer_Loop;

Optimizer_Scope* curScope = NULL;
int loopLevel = 0;
Optimizer_Loop* curLoop = NULL;

void Optimizer_LogDeclaration(AST_Statement_Declaration* declaration)
{
    Optimizer_Variable v = {
        declaration->variableName, 0, 0, loopLevel, NULL, declaration->variableType->pointerLevel > 1, declaration};
    GenericList_Append(&curScope->variables, &v);
}

static bool CompareVarToStr(const void* var, const void* id)
{
    return strcmp(((Optimizer_Variable*)var)->id, id) == 0;
}

void Optimizer_LogAccess(AST_Expression_VariableAccess* access)
{
    if (curScope == NULL)
        return;

    Optimizer_Scope* scope = curScope;
    do
    {
        Optimizer_Variable* var = GenericList_Find(&scope->variables, CompareVarToStr, access->id);
        if (var != NULL)
        {
            if (curLoop != NULL && var->definedAtLoopLevel < loopLevel)
                GenericList_Append(&curLoop->accessedVars, &var);

            if (var->currentScore != (int16_t)0x8000)
            {
                int score = 1;
                if (var->isPointer)
                    score = 2;
                var->currentScore += score << (3 * (loopLevel - var->definedAtLoopLevel));
            }

            // Actually in this order, last score is the score after the last access
            var->lastScore = var->currentScore;
            var->lastAccess = access;
            return;
        }
    } while ((scope = scope->parent) != NULL);
}

void Optimizer_LogFunctionCall(uint16_t modifiedRegisters)
{
    if (curScope == NULL)
        return;

    Optimizer_Scope* scope = curScope;
    do
    {
        for (int i = 0; i < 8; i++)
            if (modifiedRegisters & (1 << i))
                scope->preferredRegisters[i]--;

        for (size_t i = 0; i < scope->variables.count; i++)
        {
            Optimizer_Variable* var = GenericList_At(&scope->variables, i);
            if (var->currentScore != (int16_t)0x8000)
                var->currentScore -= 2 << (3 * (loopLevel - var->definedAtLoopLevel));
        }
    } while ((scope = scope->parent) != NULL);
}

void Optimizer_EnterNewScope()
{
    Optimizer_Scope* new = xmalloc(sizeof(Optimizer_Scope));
    memset(&new->preferredRegisters[0], 0xFF, 8 * sizeof(uint16_t));
    new->parent = curScope;
    new->variables = GenericList_Create(sizeof(Optimizer_Variable));
    curScope = new;
}

void Optimizer_ExitScope(uint16_t* const oPrefRegisters)
{
    for (size_t i = 0; i < curScope->variables.count; i++)
    {
        Optimizer_Variable* var = GenericList_At(&curScope->variables, i);
        AST_Statement_Declaration* decl = var->declaration;

        decl->lastAccess = var->lastAccess;

        // Force stack alloc if address-of'ed
        if (var->lastScore == (int16_t)0x8000)
            decl->variableType->qualifiers |= Qualifier_Stack;
        else if (var->lastScore >= (int16_t)0)
            decl->variableType->qualifiers |= Qualifier_OptimizerRegister;
        else
            decl->variableType->qualifiers |= Qualifier_OptimizerStack;
    }

    memcpy(oPrefRegisters, &curScope->preferredRegisters[0], 8 * sizeof(uint16_t));

    GenericList_Dispose(&curScope->variables);
    Optimizer_Scope* old = curScope;
    curScope = old->parent;
    free(old);
}

void Optimizer_EnterLoop()
{
    Optimizer_Loop* new = xmalloc(sizeof(Optimizer_Loop));
    new->accessedVars = GenericList_Create(sizeof(Optimizer_Variable*));
    new->parent = curLoop;
    curLoop = new;
    loopLevel++;
}

void Optimizer_LogAddrOf(const char* idOfDerefdVar)
{
    if (curScope == NULL)
        return;

    Optimizer_Scope* scope = curScope;
    do
    {
        Optimizer_Variable* var = GenericList_Find(&scope->variables, CompareVarToStr, idOfDerefdVar);
        if (var != NULL)
        {
            var->currentScore = (int16_t)0x8000;
            var->lastScore = (int16_t)0x8000;
            return;
        }
    } while ((scope = scope->parent) != NULL);
}

void Optimizer_ExitLoop(void* loop)
{
    for (size_t i = 0; i < curLoop->accessedVars.count; i++)
    {
        Optimizer_Variable** v = GenericList_At(&curLoop->accessedVars, i);
        Optimizer_Variable* var = *v;
        var->lastAccess = loop;
        var->lastScore = var->currentScore;
    }

    GenericList_Dispose(&curLoop->accessedVars);
    Optimizer_Loop* old = curLoop;
    curLoop = old->parent;
    free(old);
    loopLevel--;
}

// An inline asm counts as an access to all defined variables, as they might be used in it.
void Optimizer_LogInlineASM(void* asmNode)
{
    Optimizer_Scope* scope = curScope;
    do
    {
        for (size_t i = 0; i < scope->variables.count; i++)
        {
            Optimizer_Variable* var = GenericList_At(&scope->variables, i);
            var->lastScore = var->currentScore;
            var->lastAccess = asmNode;
        }
    } while ((scope = scope->parent) != NULL);
}
