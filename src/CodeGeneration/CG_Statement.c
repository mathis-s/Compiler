#include "CG_Statement.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../AST.h"
#include "../Compiler.h"
#include "../Error.h"
#include "../Flags.h"
#include "../Function.h"
#include "../GenericList.h"
#include "../Optimizer.h"
#include "../Outfile.h"
#include "../Register.h"
#include "../Scope.h"
#include "../Stack.h"
#include "../Token.h"
#include "../Type.h"
#include "../Util.h"
#include "../Value.h"
#include "../Variables.h"
#include "CG_Binop.h"
#include "CG_Expression.h"

typedef struct
{
    int currentLoopContinueStackSize;
    int currentLoopBreakSpOffset;
    int currentLoopBreakStackSize;
    char currentContinueLabel[32];
    char currentBreakLabel[32];
} LoopState;

LoopState loopState;

/*typedef struct
{
    const char* label;
    int stackSize;
    int spOffset;
} GotoLabel;
GenericList gotoLabels;

void CG_Statement_DeleteLabels ()
{
    // The strings themselves are still references to the
    // original tokens, and as such will be cleared when the
    // token array gets deleted
    GenericList_Dispose(&gotoLabels);
}

void CG_Statement_InitLabels ()
{
    gotoLabels = GenericList_Create(sizeof(GotoLabel));
}*/

static void CodeGen_ReturnStatement(AST_Statement_Return* stmt, Scope* scope)
{
    int spOffset = Stack_GetOffset();
    int stackSize = Stack_GetSize();

    VariableType* returnType = Function_GetCurrent()->returnType;
    if (IsDataType(returnType))
    {
        if (stmt->expr == NULL)
            ErrorAtLocation("Invalid return statement", stmt->loc);

        int returnValueSize = SizeInWords(returnType);

        Value outValue =
            (Value){(int32_t)(Stack_GetSize() + returnValueSize + 1), AddressType_MemoryRelative, returnValueSize};

        VariableType* outType = NULL;
        bool outReadOnly;
        CodeGen_Expression(stmt->expr, scope, &outValue, &outType, &outReadOnly);

        if (!Type_Check(returnType, outType))
            ErrorAtLocation("Invalid return type", stmt->loc);

        Type_RemoveReference(outType);

        Value returnValue =
            (Value){(int32_t)(Stack_GetSize() + returnValueSize + 1), AddressType_MemoryRelative, returnValueSize};
        if (!Value_Equals(&outValue, &returnValue))
            Value_GenerateMemCpy(returnValue, outValue);

        if (!outReadOnly)
            Value_FreeValue(&outValue);
    }

    Stack_ToAddress(Stack_GetSize() + 1);
    OutWrite("mov ip, [sp]\nnop\n");

    // Reset the stack to was it was before the return
    // otherwise some unreachable instructions that align the stack
    // afterwards are generated.
    Stack_SetOffset(spOffset);
    Stack_SetSize(stackSize);
}

static void CodeGen_IfStatement(AST_Statement_If* stmt, Scope* scope)
{
    int ifId = GetLabelID();
    Value outValue = FlagValue;
    bool outReadOnly;
    CodeGen_Expression(stmt->cond, scope, &outValue, NULL, &outReadOnly);

    if (outValue.addressType == AddressType_Flag)
    {
        OutWrite("jmp%s else%u\n", Flags_FlagToString(Flags_Invert((Flag)outValue.address)), ifId);
        OutWrite("nop\n");
    }
    else
    {
        Value_ToFlag(&outValue);
        OutWrite("jmp_z else%u\n", ifId);
        OutWrite("nop\n");
    }

    int spOffsetPostCond = Stack_GetOffset();
    int stackSizePostCond = Stack_GetSize();

    if (!outReadOnly)
        Value_FreeValue(&outValue);

    CodeGen_Statement(stmt->ifTrue, scope);

    // if (Stack_GetSize() != stackSizePostCond)
    //     ErrorAtLocation("Invalid if-statement body", stmt->loc);

    Stack_ToAddress(-spOffsetPostCond);

    if (stmt->ifFalse != NULL)
    {
        // SetCurStackPointerOffset(spOffsetPostCond);
        // SetStackSize(stackSizePostCond);

        OutWrite("jmp n_else%u\n", ifId);
        OutWrite("nop\n");
        OutWrite("else%u:\n", ifId);

        CodeGen_Statement(stmt->ifFalse, scope);

        // if (Stack_GetSize() != stackSizePostCond)
        //     ErrorAtLocation("Invalid else body", stmt->loc);

        Stack_ToAddress(-spOffsetPostCond);
        OutWrite("n_else%u:\n", ifId);
    }
    else
        OutWrite("else%u:\n", ifId);

    Stack_SetOffset(spOffsetPostCond);
    Stack_SetSize(stackSizePostCond);
}

static void CodeGen_WhileLoop(AST_Statement_While* stmt, Scope* scope)
{
    Stack_Align();
    int whileId = GetLabelID();

    // We need to keep track of the loops continue/break label name
    // as well as the current stack size for continue/break.

    LoopState oldLoopState = loopState;

    loopState.currentLoopContinueStackSize = Stack_GetSize();
    sprintf(&loopState.currentContinueLabel[0], "while_loop%u", whileId);
    sprintf(&loopState.currentBreakLabel[0], "while_end%u", whileId);

    OutWrite("while_loop%u:\n", whileId);
    Value outValue = FlagValue;
    bool outReadOnly;
    CodeGen_Expression(stmt->cond, scope, &outValue, NULL, &outReadOnly);

    if (!(outValue.addressType == AddressType_Literal && outValue.address != 0))
    {
        if (outValue.addressType == AddressType_Flag)
        {
            OutWrite("jmp%s while_end%u\n", Flags_FlagToString(Flags_Invert((Flag)outValue.address)), whileId);
            OutWrite("nop\n");
        }
        else
        {
            Value_ToFlag(&outValue);
            OutWrite("jmp_z while_end%u\n", whileId);
            OutWrite("nop\n");
        }
    }

    loopState.currentLoopBreakSpOffset = Stack_GetOffset();
    loopState.currentLoopBreakStackSize = Stack_GetSize();

    if (!outReadOnly)
        Value_FreeValue(&outValue);

    CodeGen_Statement(stmt->body, scope);

    int delta = Stack_GetSize() - loopState.currentLoopContinueStackSize;
    Stack_Offset(delta);
    ShiftAddressSpace(scope, -delta);
    Stack_SetSize(loopState.currentLoopContinueStackSize);
    Stack_Align();

    OutWrite("jmp while_loop%u\n", whileId);
    OutWrite("nop\n");
    OutWrite("while_end%u:\n", whileId);

    Stack_SetOffset(loopState.currentLoopBreakSpOffset);
    Stack_SetSize(loopState.currentLoopBreakStackSize);

    Scope_DeleteVariablesAfterLoop(scope, stmt);

    loopState = oldLoopState;
}

static void CodeGen_DoWhileLoop(AST_Statement_Do* stmt, Scope* scope)
{
    Stack_Align();
    int doId = GetLabelID();
    LoopState oldLoopState = loopState;

    sprintf(&loopState.currentContinueLabel[0], "do_loop%u", doId);
    sprintf(&loopState.currentBreakLabel[0], "do_end%u", doId);

    loopState.currentLoopContinueStackSize = Stack_GetSize();
    OutWrite("do_loop%u:\n", doId);
    loopState.currentLoopBreakSpOffset = 0;
    loopState.currentLoopBreakStackSize = loopState.currentLoopContinueStackSize;

    CodeGen_Statement(stmt->body, scope);

    Value outValue = FlagValue;
    bool outReadOnly;

    int delta = Stack_GetSize() - loopState.currentLoopContinueStackSize;
    Stack_Offset(delta);
    ShiftAddressSpace(scope, -delta);
    Stack_SetSize(loopState.currentLoopContinueStackSize);
    Stack_Align();
    CodeGen_Expression(stmt->cond, scope, &outValue, NULL, &outReadOnly);

    if (Stack_GetOffset() != 0)
        ErrorAtLocation("Invalid do-while-loop condition", stmt->loc);

    if (!(outValue.addressType == AddressType_Literal && outValue.address != 0))
    {
        if (outValue.addressType == AddressType_Flag)
        {
            OutWrite("jmp%s do_loop%u\n", Flags_FlagToString((Flag)outValue.address), doId);
        }
        else
        {
            Value_ToFlag(&outValue);
            OutWrite("jmp_nz do_loop%u\n", doId);
        }
    }

    if (!outReadOnly)
        Value_FreeValue(&outValue);

    OutWrite("nop\n");
    OutWrite("do_end%u:\n", doId);

    Scope_DeleteVariablesAfterLoop(scope, stmt);

    loopState = oldLoopState;
}

static void CodeGen_ForLoop(AST_Statement_For* stmt, Scope* scope)
{
    Scope* statementVars = stmt->statementScope;
    statementVars->parent = scope;

    int oldStackSize = Stack_GetSize();

    int forLoopId = GetLabelID();

    // Init Statement
    CodeGen_Statement(stmt->init, statementVars);
    Stack_Align();

    OutWrite("for_loop%u:\n", forLoopId);

    // We need to keep track of the loops continue/break label name
    // as well as the current stack size for continue/break.
    LoopState oldLoopState = loopState;

    loopState.currentLoopContinueStackSize = Stack_GetSize();
    sprintf(&loopState.currentContinueLabel[0], "for_continue%u", forLoopId);
    sprintf(&loopState.currentBreakLabel[0], "for_break%u", forLoopId);

    Value outValue = FlagValue;
    bool outReadOnly;
    CodeGen_Expression(stmt->cond, statementVars, &outValue, NULL, &outReadOnly);
    // AlignStack();
    if (outValue.addressType == AddressType_Flag)
    {
        OutWrite("jmp%s for_break%u\n", Flags_FlagToString(Flags_Invert((Flag)outValue.address)), forLoopId);
        OutWrite("nop\n");
    }
    else
    {
        Value_ToFlag(&outValue);
        OutWrite("jmp_z for_break%u\n", forLoopId);
        OutWrite("nop\n");
    }

    int spOffsetPostCond = Stack_GetOffset();
    int stackSizePostCond = Stack_GetSize();

    if (!outReadOnly)
        Value_FreeValue(&outValue);

    CodeGen_Statement(stmt->body, statementVars);

    int delta = Stack_GetSize() - loopState.currentLoopContinueStackSize;
    Stack_Offset(delta);
    ShiftAddressSpace(scope, -delta);
    Stack_SetSize(loopState.currentLoopContinueStackSize);
    Stack_Align();
    OutWrite("for_continue%u:\n", forLoopId);

    CodeGen_Expression(stmt->count, statementVars, NULL, NULL, &outReadOnly);

    delta = Stack_GetSize() - loopState.currentLoopContinueStackSize;
    Stack_Offset(delta);
    ShiftAddressSpace(scope, -delta);
    Stack_SetSize(loopState.currentLoopContinueStackSize);
    Stack_Align();

    OutWrite("jmp for_loop%u\n", forLoopId);
    OutWrite("nop\n");
    OutWrite("for_break%u:\n", forLoopId);

    Stack_SetOffset(spOffsetPostCond);
    Stack_SetSize(stackSizePostCond);

    delta = Stack_GetSize() - oldStackSize;
    Stack_Offset(delta);
    ShiftAddressSpace(scope, -delta);
    Stack_SetSize(oldStackSize);
    Scope_Dispose(statementVars);
    free(statementVars);

    Scope_DeleteVariablesAfterLoop(scope, stmt);

    loopState = oldLoopState;
}

static void CodeGen_Scope(AST_Statement_Scope* stmt, Scope* scope)
{
    int16_t usedRegisters = Registers_GetUsed();

    int oldStackSize = Stack_GetSize();
    stmt->scope->parent = scope;
    Registers_SetPreferred(&stmt->scope->preferredRegisters[0]);

    for (size_t i = 0; i < stmt->numStatements; i++)
    {
        CodeGen_Statement(stmt->statements[i], stmt->scope);
    }

    int delta = Stack_GetSize() - oldStackSize;
    Stack_Offset(delta);
    ShiftAddressSpace(scope, -delta);
    Stack_SetSize(oldStackSize);
    Scope_Dispose(stmt->scope);

    // Very useful for finding register leaks
    assert((~usedRegisters & Registers_GetUsed()) == 0);

    Registers_SetPreferred(&scope->preferredRegisters[0]);
    free(stmt->scope);
    free(stmt->statements);
}

static int CompareUInt16(const void* a, const void* b)
{
    uint16_t* ia = (uint16_t*)a;
    uint16_t* ib = (uint16_t*)b;

    if (*ia == *ib)
        return 0;
    if (*ia > *ib)
        return 1;
    return -1;
}

static void CodeGen_SwitchCase(AST_Statement_Switch* stmt, Scope* scope)
{

    int switchId = GetLabelID();

    LoopState oldLoopState = loopState;
    sprintf(&loopState.currentBreakLabel[0], "switch_%u_break", switchId);
    memcpy(&loopState.currentContinueLabel[0], &oldLoopState.currentContinueLabel[0], 32);

    Value outValue = NullValue;
    bool outReadOnly;
    CodeGen_Expression(stmt->selector, scope, &outValue, NULL, &outReadOnly);

    // TODO Fix this (copy when not register)
    if (outValue.size != 1)
        ErrorAtLocation("Switch requires statement of 16-bit integer type", stmt->loc);
    if (outValue.addressType != AddressType_Register || outReadOnly)
    {
        Value newValue = Value_Register(1);
        Value_GenerateMemCpy(newValue, outValue);
        if (!outReadOnly)
            Value_FreeValue(&outValue);
        outReadOnly = false;
        outValue = newValue;
    }

    // AlignStack();
    loopState.currentLoopBreakStackSize = Stack_GetSize();
    loopState.currentLoopBreakSpOffset = Stack_GetOffset();

    AST_Statement_Switch_SwitchCase* listCases = stmt->cases;

    if (stmt->numCases > 0)
    {
        uint16_t* labelsList = xmalloc(sizeof(uint16_t) * stmt->numCases);

        for (size_t i = 0; i < stmt->numCases; i++)
            labelsList[i] = listCases[i].id;

        qsort(labelsList, stmt->numCases, sizeof(uint16_t), CompareUInt16);

        // Generate Jump Table based on sorted list of all cases
        uint16_t lowestId = labelsList[0];

        // Lower bounds check
        OutWrite("sub r%i, %i\n", Value_GetR0(&outValue), lowestId);
        OutWrite("jmp_s switch_%u_default\n", switchId);

        // Upper bounds check
        OutWrite("sub rz, %i, r%i\n", labelsList[stmt->numCases - 1] - lowestId, Value_GetR0(&outValue));
        OutWrite("jmp_s switch_%u_default\n", switchId);

        OutWrite("add r%i, 1\n", Value_GetR0(&outValue));
        OutWrite("add ip, r%i\n", Value_GetR0(&outValue));
        if (!outReadOnly)
            Value_FreeValue(&outValue);
        OutWrite("jmp switch_%u_default\n", switchId);

        int lastIndex = lowestId - 1;
        for (size_t j = 0; j < stmt->numCases; j++)
        {
            int delta = (labelsList[j] - lastIndex) - 1;
            while (delta--)
                OutWrite("jmp switch_%u_default\n", switchId);

            OutWrite("jmp switch_%u_case_%u\n", switchId, labelsList[j]);
            lastIndex = labelsList[j];
        }

        // Otherwise the last jump might not work if it adds 0
        OutWrite("nop\n");

        for (size_t j = 0; j < stmt->numCases; j++)
        {
            Stack_SetOffset(loopState.currentLoopBreakSpOffset);
            Stack_SetSize(loopState.currentLoopBreakStackSize);

            OutWrite("switch_%u_case_%u:\n", switchId, listCases[j].id);

            AST_Statement** stmts = listCases[j].statements;
            size_t len = listCases[j].numStatements;
            for (size_t k = 0; k < len; k++)
            {
                CodeGen_Statement(stmts[k], scope);
            }
            free(stmts);
        }

        Stack_SetOffset(loopState.currentLoopBreakSpOffset);
        Stack_SetSize(loopState.currentLoopBreakStackSize);

        OutWrite("switch_%u_default:\n", switchId);
        if (stmt->defaultCaseStmts != NULL)
        {
            for (size_t j = 0; j < stmt->numStmtsDefCase; j++)
                CodeGen_Statement(stmt->defaultCaseStmts[j], scope);
        }
        free(labelsList);
    }

    free(stmt->cases);
    if (stmt->defaultCaseStmts != NULL)
        free(stmt->defaultCaseStmts);

    // Implicit break at the end of the switch:
    int delta = Stack_GetSize() - loopState.currentLoopBreakStackSize;
    int n = Stack_GetOffset() + delta - loopState.currentLoopBreakSpOffset;
    if (n > 0)
        OutWrite("sub sp, %i\n", n);
    else if (n < 0)
        OutWrite("add sp, %i\n", -n);

    Stack_SetOffset(loopState.currentLoopBreakSpOffset);
    Stack_SetSize(loopState.currentLoopBreakStackSize);
    OutWrite("switch_%u_break:\n", switchId);

    Scope_DeleteVariablesAfterLoop(scope, stmt);
    loopState = oldLoopState;

    return;
}

static void CodeGen_Declaration(AST_Statement_Declaration* stmt, Scope* scope)
{

    VariableType* type = stmt->variableType;
    int size = SizeInWords(type);
    Variable v;

    v = (Variable){type, stmt->variableName, {(int32_t)0, AddressType_Register, size}, stmt->lastAccess};

    // Normal variable
    if (IsPrimitiveType(type))
    {
        // The CPU has 8 registers, we keep a few for temporary values,
        // the rest can be used for variables.
        bool storeInRegister =
            (Registers_GetNumUsed() + size <= 5) && (v.type->qualifiers & Qualifier_OptimizerRegister);

        // If the register qualifier was explicitly specified, we assume the user knows
        // what they're doing and decrease the number of registers kept free for temp values.
        if (Registers_GetNumPreferred() - 1 >= size && (Qualifier_Register & v.type->qualifiers))
            storeInRegister = true;

        // If the address of the value is ever taken, it is always put on the stack.
        if ((v.type->qualifiers & Qualifier_Stack))
            storeInRegister = false;

        Value val = NullValue;

        if (stmt->value != NULL)
        {
            Value outValue = NullValue;
            VariableType* outType;
            bool outReadOnly;

            CodeGen_Expression(stmt->value, scope, &outValue, &outType, &outReadOnly);
            if (!Type_Check(outType, type))
                ErrorAtLocation("Invalid type", stmt->loc);

            Type_RemoveReference(outType);

            size_t outValueSize = outValue.size;

            if (outReadOnly)
            {
                if (storeInRegister)
                {
                    val = Value_Register(size);
                    Value_GenerateMemCpy(val, outValue);
                }
                else
                {
                    val = GetValueOnStack(size, scope);
                    if (outValue.addressType == AddressType_MemoryRelative)
                        outValue.address += (int32_t)size;

                    Value_GenerateMemCpy(val, outValue);
                }
            }
            else
            {
                if (storeInRegister)
                {
                    if (outValue.addressType == AddressType_Register)
                        val = outValue;
                    else
                    {
                        val = Value_Register(size);
                        Value_GenerateMemCpy(val, outValue);
                        Value_FreeValue(&outValue);
                    }
                }
                else
                {
                    val = GetValueOnStack(size, scope);
                    if (outValue.addressType == AddressType_MemoryRelative)
                        outValue.address += (int32_t)size;
                    Value_GenerateMemCpy(val, outValue);
                    Value_FreeValue(&outValue);
                }
            }

            // TODO do this for stack allocations too
            // (Already done for literals in GenerateMemCpy)
            if (storeInRegister && outValue.addressType != AddressType_Literal)
            {
                // In case this variable is 32 bit, but the result of the expression
                // is only 16 bit
                if (outValueSize == 1 && size == 2)
                    OutWrite("mov r%i, 0\n", Value_GetR1(&val));
            }
        }
        else
        {
            if (!storeInRegister)
                val = GetValueOnStack(size, scope);
            else
                val = Value_Register(size);
        }

        v.value = val;
    }
    // Struct
    else
    {
        Value val = NullValue;
        if (stmt->value != NULL)
        {
            Value outValue = NullValue;
            VariableType* outType;
            bool outReadOnly;
            CodeGen_Expression(stmt->value, scope, &outValue, &outType, &outReadOnly);

            if (size != -1 && outValue.size != size)
                ErrorAtLocation("Invalid assignment", stmt->loc);

            if (!Type_Check(v.type, outType))
                ErrorAtLocation("Invalid type!", stmt->loc);

            Type_RemoveReference(outType);

            if (!outReadOnly && outValue.addressType == AddressType_MemoryRelative)
                val = outValue;
            else
            {
                Stack_Align();
                val = GetValueOnStack(outValue.size, scope);

                // Only the address space of variables is shifted,
                // since outValue isn't a variable we do it manually.
                if (outValue.addressType == AddressType_MemoryRelative)
                    outValue.address += (int32_t)val.size;

                Value_GenerateMemCpy(val, outValue);
                if (!outReadOnly)
                    Value_FreeValue(&outValue);
            }

            // Array size might not be set yet (int arr[] = { 1 });
            if (v.type->token == ArrayToken)
                ((VariableTypeArray*)v.type)->memberCount =
                    outValue.size / SizeInWords(((VariableTypeArray*)v.type)->memberType);
        }
        else
        {
            if (size == -1)
                ErrorAtLocation("Undefined Size", stmt->loc);

            val = GetValueOnStack(size, scope);
        }
        v.value = val;
    }

    if (v.lastAccess == NULL)
    {
        Value_FreeValue(&v.value);
        Type_RemoveReference(type);
    }
    else
        Scope_AddVariable(scope, v);
}

static void CodeGen_Break(AST_Statement* stmt)
{
    if (loopState.currentBreakLabel[0] == 0)
        ErrorAtLocation("Invalid break", stmt->loc);

    int delta = Stack_GetSize() - loopState.currentLoopBreakStackSize;
    int n = Stack_GetOffset() + delta - loopState.currentLoopBreakSpOffset;

    if (n > 0)
        OutWrite("sub sp, %i\n", n);
    else if (n < 0)
        OutWrite("add sp, %i\n", -n);

    OutWrite("jmp %s\nnop\n", loopState.currentBreakLabel);
}

static void CodeGen_Continue(AST_Statement* stmt)
{
    if (loopState.currentContinueLabel[0] == 0)
        ErrorAtLocation("Invalid continue", stmt->loc);

    int delta = Stack_GetSize() - loopState.currentLoopContinueStackSize;
    int n = Stack_GetOffset() + delta;

    if (n > 0)
        OutWrite("sub sp, %i\n", n);
    if (n < 0)
        OutWrite("add sp, %i\n", -n);

    OutWrite("jmp %s\nnop\n", &loopState.currentContinueLabel);
}

void CodeGen_InlineAssembly(AST_Statement_ASM* stmt, Scope* scope)
{
    Stack_Align();
    OutWrite("%s\n", stmt->code);
    Scope_DeleteVariablesAfterLoop(scope, stmt);
}

void CodeGen_Statement(AST_Statement* stmt, Scope* scope)
{
    switch (stmt->type)
    {
        case AST_StatementType_Return:
            CodeGen_ReturnStatement((AST_Statement_Return*)stmt, scope);
            break;
        case AST_StatementType_If:
            CodeGen_IfStatement((AST_Statement_If*)stmt, scope);
            break;
        case AST_StatementType_While:
            CodeGen_WhileLoop((AST_Statement_While*)stmt, scope);
            break;
        case AST_StatementType_Do:
            CodeGen_DoWhileLoop((AST_Statement_Do*)stmt, scope);
            break;
        case AST_StatementType_For:
            CodeGen_ForLoop((AST_Statement_For*)stmt, scope);
            break;
        case AST_StatementType_Scope:
            CodeGen_Scope((AST_Statement_Scope*)stmt, scope);
            break;
        case AST_StatementType_Declaration:
            CodeGen_Declaration((AST_Statement_Declaration*)stmt, scope);
            break;
        case AST_StatementType_Expr:
            CodeGen_Expression(((AST_Statement_Expr*)stmt)->expr, scope, NULL, NULL, NULL);
            break;
        case AST_StatementType_Empty:
            break;
        case AST_StatementType_Break:
            CodeGen_Break(stmt);
            break;
        case AST_StatementType_Continue:
            CodeGen_Continue(stmt);
            break;
        case AST_StatementType_Switch:
            CodeGen_SwitchCase((AST_Statement_Switch*)stmt, scope);
            break;
        case AST_StatementType_ASM:
            CodeGen_InlineAssembly((AST_Statement_ASM*)stmt, scope);
            break;
        default:
            assert(0);
    }

    free(stmt);
}
