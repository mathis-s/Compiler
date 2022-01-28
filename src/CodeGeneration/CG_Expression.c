#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "CG_Expression.h"

#include "../AST.h"
#include "../Data.h"
#include "../Error.h"
#include "../Flags.h"
#include "../Function.h"
#include "../GenericList.h"
#include "../Optimizer.h"
#include "../Outfile.h"
#include "../Register.h"
#include "../Scope.h"
#include "../Stack.h"
#include "../Struct.h"
#include "../Token.h"
#include "../Type.h"
#include "../Util.h"
#include "../Value.h"
#include "../Variables.h"
#include "CG_Binop.h"
#include "CG_NativeOP.h"
#include "CG_UnOp.h"

int labelIDCounter = 0;
int GetLabelID()
{
    return labelIDCounter++;
}

static void CodeGen_ListLiteral(AST_Expression_ListLiteral* expr, Scope* scope, Value* oValue, VariableType** oType,
                                bool* oReadOnly)
{

    size_t totalSize = 0;
    for (size_t i = 0; i < expr->numExpr; i++)
    {
        Value outValue = NullValue;
        VariableType* outType;
        bool outReadOnly;

        CodeGen_Expression(expr->expressions[i], scope, &outValue, &outType, &outReadOnly);

        // Compile-time
        if (Function_GetCurrent() == NULL)
        {
            assert(outValue.addressType == AddressType_Literal || outValue.addressType == AddressType_Memory);
            if (outValue.addressType == AddressType_Literal)
            {
                if (outValue.size == 1)
                    AllocateGlobalWord((int)outValue.address);
                else if (outValue.size == 2)
                    AllocateGlobalDoubleWord(outValue.address);
            }
        }
        else
        {
            if (outValue.addressType != AddressType_MemoryRelative || (int32_t)outValue.size != outValue.address ||
                outReadOnly)
            {
                Stack_Align();
                Value_Push(&outValue);
                ShiftAddressSpace(scope, outValue.size);

                if (!outReadOnly)
                    Value_FreeValue(&outValue);
            }
            // If the expression already pushed the result to the stack, there's nothing to do.
        }

        Type_RemoveReference(outType);
        totalSize += outValue.size;
    }

    // Not compile time
    if (Function_GetCurrent() != NULL)
    {
        Stack_Align();
        Value retval = Value_MemoryRelative(totalSize, totalSize);
        if (oValue != NULL)
        {
            *oReadOnly = false;
            *oValue = retval;
        }
        else
            Value_FreeValue(&retval);
    }
    else
    {
        *oValue = Value_Memory(GetGlobalDataIndex() - totalSize, totalSize);
    }

    if (oType != NULL)
        *oType = Type_AddReference(&AnyVariableType);

    free(expr->expressions);
}

void CodeGen_TypeCast(AST_Expression_TypeCast* expr, Scope* scope, Value* oValue, VariableType** oType, bool* oReadOnly)
{
    if (oValue == NULL)
        return;

    // TODO add conversion functions from and to fixed.
    Value outValue = NullValue;
    VariableType* outType = NULL;
    bool outReadOnly;
    CodeGen_Expression(expr->exprA, scope, &outValue, &outType, &outReadOnly);

    int newSize = SizeInWords(expr->newType);
    if (outType->token != None && (IsPrimitiveType(expr->newType) ^ IsPrimitiveType(outType)))
        ErrorAtLocation("Invalid type cast", expr->loc);

    if (outValue.size == newSize || outValue.addressType == AddressType_Literal)
    {
        outValue.size = newSize;
        *oValue = outValue;
        *oReadOnly = outReadOnly;
    }
    else if (IsPrimitiveType(outType) && IsPrimitiveType(expr->newType))
    {
        if (Function_GetCurrent() == NULL)
            ErrorAtLocation("Expected compile-time constant", expr->loc);

        if (outReadOnly)
        {
            // if (IsPrimitiveType(expr->newType))
            {
                *oValue = Value_Register(newSize);
                Value_GenerateMemCpy(*oValue, outValue);

                if (newSize == 2 && outValue.size == 1)
                    OutWrite("mov r%i, 0\n", Value_GetR1(oValue));
            }
            /*else
            {
                int delta = outValue.size - newSize;
                switch (outValue.addressType)
                {
                    case AddressType_Memory:
                        *oValue = Value_Memory(outValue.address + delta, newSize);
                        break;
                    case AddressType_MemoryRelative:
                        *oValue = Value_Memory(outValue.address - delta, newSize);
                        break;
                }
            }*/
            *oReadOnly = false;
        }
        else
        {
            if (outValue.addressType == AddressType_Register && outValue.size == 1 && newSize == 2)
            {
                int newRegister = Registers_GetFree();
                OutWrite("mov r%i, 0\n", newRegister);
                *oValue = Value_FromRegisters((int)outValue.address, newRegister);
                *oReadOnly = false;
            }
            else if (outValue.addressType == AddressType_Register && outValue.size == 2 && newSize == 1)
            {
                *oValue = Value_FromRegister(Value_GetR0(&outValue));
                Register_Free(Value_GetR1(&outValue));
                *oReadOnly = false;
            }
            else
            {
                *oValue = Value_Register(newSize);
                Value_GenerateMemCpy(*oValue, outValue);
                Value_FreeValue(&outValue);
                *oReadOnly = false;
            }
        }
    }
    else
        ErrorAtLocation("Invalid type cast", expr->loc);

    Type_RemoveReference(outType);
    if (oType != NULL)
        *oType = expr->newType;
    else
        Type_RemoveReference(expr->newType);
}

void CodeGen_FunctionCall(const AST_Expression_FunctionCall* expr, Scope* scope, Value* oValue, VariableType** oType,
                          bool* oReadOnly)
{
    Variable* funcPointer = NULL;
    Function* outFunc = NULL;

    if ((funcPointer = Scope_FindVariable(scope, expr->id)) != NULL && funcPointer->type->token == FunctionPointerToken)
    {
        outFunc = funcPointer->type->structure;
    }
    else if ((outFunc = Function_Find(expr->id)) == NULL)
        ErrorAtLocation("Undefined identifier!", expr->loc);

    size_t parameterIndex = 0;
    int allocatedSizeInWords = 0;

    int numAllocForPushing = Registers_GetNumUsedMasked(outFunc->modifiedRegisters);
    if (oValue != NULL && oValue->addressType == AddressType_Register)
    {
        if (outFunc->modifiedRegisters & Value_GetR0(oValue))
            numAllocForPushing--;
        if (oValue->size == 2 && outFunc->modifiedRegisters & Value_GetR1(oValue))
            numAllocForPushing--;
    }
    // Up
    Value spaceForSavingRegisters = NullValue;
    if (numAllocForPushing != 0)
        spaceForSavingRegisters = GetValueOnStack(numAllocForPushing, scope);
    // OffsetStackPointer(-numUsed);
    // ShiftAddressSpace(scope, numUsed);
    // OffsetStackSize(numUsed);

    int sizeOfRetval = SizeInWords(outFunc->returnType);
    int sizeOfParameters = 0;
    for (size_t i = 0; i < outFunc->parameters.count; i++)
        sizeOfParameters += SizeInWords(((Variable*)GenericList_At(&outFunc->parameters, i))->type);

    // Usually, the return value uses the same memory space
    // as the parameters, but if is is bigger than the
    // parameters, we need to allocate more space.
    if (sizeOfRetval > sizeOfParameters)
    {
        Stack_OffsetSize(sizeOfRetval - sizeOfParameters);
        Stack_Offset(-(sizeOfRetval - sizeOfParameters));
        ShiftAddressSpace(scope, sizeOfRetval - sizeOfParameters);
        allocatedSizeInWords += sizeOfRetval - sizeOfParameters;
        spaceForSavingRegisters.address += (int32_t)(sizeOfRetval - sizeOfParameters);
    }

    for (size_t i = 0; i < expr->numParameters; i++)
    {
        AST_Expression* parExpr = expr->parameters[i]; // exprs are already parsed backwards

        Variable* originalParameter = NULL;
        size_t index = expr->numParameters - i - 1;
        if (index < outFunc->parameters.count)
            originalParameter = (Variable*)GenericList_At(&outFunc->parameters, index);

        Value requestedOutValue;
        Value parValue;
        if (originalParameter == NULL || !IsPrimitiveType(originalParameter->type))
        {
            parValue = NullValue;
            requestedOutValue = NullValue;
        }
        else
        {
            requestedOutValue = Value_MemoryRelative(0, originalParameter->value.size);
            parValue = requestedOutValue;
        }

        VariableType* parType = NULL;
        int old = Stack_GetSize();
        bool readOnly = false;
        CodeGen_Expression(parExpr, scope, &parValue, &parType, &readOnly);
        // numUsed -= Stack_GetSize() - old;

        allocatedSizeInWords += parValue.size;
        spaceForSavingRegisters.address += (int32_t)parValue.size;

        if (Stack_GetSize() - old == parValue.size)
        {
            ; // In this case, the expression is returned on the stack,
            // so there's no need for us to push it.
        }
        else if (old != Stack_GetSize())
            ErrorAtLocation("Invalid function call", expr->loc);
        else
        {
            if (requestedOutValue.addressType != AddressType_None && Value_Equals(&parValue, &requestedOutValue))
            {
                Stack_OffsetSize(parValue.size);
                Stack_Offset(-parValue.size);
            }
            else
                Value_Push(&parValue);
            ShiftAddressSpace(scope, parValue.size);
        }

        // access parameter types backwards here
        if (index < outFunc->parameters.count)
        {
            if (originalParameter->value.size != parValue.size || (!Type_Check(originalParameter->type, parType)))
                ErrorAtLocation("Invalid parameter type!", expr->loc);
        }
        else if (!outFunc->variadicArguments)
            ErrorAtLocation("Invalid number of parameters", expr->loc);

        if (!readOnly)
            Value_FreeValue(&parValue);

        parameterIndex++;
        Type_RemoveReference(parType);
        // if ()
        //     printf("%s %u %s\n", expr->loc.sourceFile, expr->loc.lineNumber, outFunc->identifier);
    }

    free(expr->parameters);

    if (parameterIndex < outFunc->parameters.count)
        ErrorAtLocation("Invalid number of parameters", expr->loc);

    // If the return value is stored a register, there's
    // no need to save that register, as it will be overwritten.
    // We set it to unused here...
    if (oValue != NULL && oValue->addressType == AddressType_Register)
    {
        Register_Free(Value_GetR0(oValue));
        if (oValue->size == 2)
            Register_Free(Value_GetR1(oValue));
    }

    int numPushed = Registers_GetNumUsedMasked(outFunc->modifiedRegisters);
    uint16_t pushedRegisters = 0;
    assert(numPushed <= numAllocForPushing);
    spaceForSavingRegisters.address -= (int32_t)(numAllocForPushing - numPushed);
    if (numPushed != 0)
    {
        Stack_ToAddress((int)spaceForSavingRegisters.address);
        pushedRegisters = Registers_PushAllUsedMasked(&numPushed, outFunc->modifiedRegisters);

        // ... and then set it to used again.
        if (oValue != NULL && oValue->addressType == AddressType_Register)
        {
            Register_GetSpecific(Value_GetR0(oValue));
            if (oValue->size == 2)
                Register_GetSpecific(Value_GetR1(oValue));
        }

        Stack_Offset(numPushed);
        Stack_Align();
        // OffsetStackPointer(numUsed);
    }
    else
        Stack_Align();
    // else
    // curStackPointerOffset -= numUsed;

    if (oType != NULL)
        *oType = Type_AddReference(outFunc->returnType);

    if (funcPointer != NULL)
    {
        Value temp = NullValue;
        switch (funcPointer->value.addressType)
        {
            case AddressType_Register:
                OutWrite("add [sp++], ip, 1\n");
                OutWrite("jmp r%i\n", Value_GetR0(&funcPointer->value));
                break;
            case AddressType_Literal:
                OutWrite("add [sp++], ip, 1\n");
                OutWrite("jmp %i\n", funcPointer->value.address);
                break;
            case AddressType_Memory:
                OutWrite("add [sp++], ip, 1\n");
                OutWrite("jmp [%i]\n", funcPointer->value.address);
                break;
            default:
                temp = Value_Register(1);
                Value_GenerateMemCpy(temp, funcPointer->value);
                Stack_Align();
                OutWrite("add [sp++], ip, 1\n");
                OutWrite("jmp r%i\n", Value_GetR0(&temp));
                Value_FreeValue(&temp);
                break;
        }
    }
    else
    {
        OutWrite("add [sp++], ip, 1\n");
        OutWrite("jmp _%s\n", expr->id);
    }

    OutWrite("nop\n");

    // curStackPointerOffset -= numUsed;

    // OutWrite("Stack pointer offset: %i, Allocated size in words: %i\n", curStackPointerOffset,
    // allocatedSizeInWords);
    if (sizeOfRetval > 0 && IsPrimitiveType(outFunc->returnType) && oValue != NULL)
    {
        if (oValue->addressType == AddressType_None || oValue->addressType == AddressType_Flag ||
            oValue->addressType == AddressType_MemoryRegister || oValue->addressType == AddressType_MemoryRelative)
        {
            *oValue = Value_Register(sizeOfRetval);
            *oReadOnly = false;
        }
        else
        {
            if (oValue->size != sizeOfRetval)
                ErrorAtLocation("Invalid return type", expr->loc);

            *oReadOnly = true;
        }

        Value_GenerateMemCpy(*oValue, (Value){(int32_t)sizeOfRetval, AddressType_MemoryRelative, sizeOfRetval});
    }

    // We waste some stack space by not cleaning up the stack
    // when a function returns something large (eg a struct) on it.
    // Advantage is that we don't have to copy the return value.
    if (sizeOfRetval > 0 && !IsPrimitiveType(outFunc->returnType) && oValue != NULL)
    {
        if (numPushed > 0)
        {
            Registers_Pop(pushedRegisters, Stack_GetOffset() + allocatedSizeInWords);
        }

        *oValue = Value_MemoryRelative(allocatedSizeInWords + numAllocForPushing, sizeOfRetval);
        *oReadOnly = false;
        Value_GenerateMemCpy(*oValue, Value_MemoryRelative(sizeOfRetval, sizeOfRetval));

        Stack_OffsetSize(-(allocatedSizeInWords - sizeOfRetval));
        Stack_Offset(allocatedSizeInWords - sizeOfRetval);

        Stack_OffsetSize(-numAllocForPushing);
        Stack_Offset(numAllocForPushing);
    }
    else
    {
        Stack_OffsetSize(-allocatedSizeInWords);
        Stack_Offset(allocatedSizeInWords);

        if (numPushed != 0)
            Registers_Pop(pushedRegisters, Stack_GetOffset());

        Stack_OffsetSize(-numAllocForPushing);
        Stack_Offset(numAllocForPushing);

        ShiftAddressSpace(scope, -(numAllocForPushing + allocatedSizeInWords));
    }

    Function_GetCurrent()->modifiedRegisters |= outFunc->modifiedRegisters;
}

void CodeGen_VariableAccess(AST_Expression_VariableAccess* expr, Scope* scope, Value* oValue, VariableType** oType,
                            bool* oReadOnly)
{
    Variable* outVar = NULL;
    if ((outVar = Scope_FindVariable(scope, expr->id)) != NULL)
    {
        if (oType != NULL)
            *oType = Type_AddReference(outVar->type);

        if (oValue != NULL)
        {
            // TODO fix this hack to get address at compile time
            if (Function_GetCurrent() == NULL && oValue->addressType == AddressType_Memory)
                *oValue = Value_Literal(oValue->address);
            else
                *oValue = outVar->value;
        }

        if (outVar->lastAccess == expr)
        {
            if (oReadOnly != NULL)
                *oReadOnly = 0;
            else
            {
                Value_FreeValue(&outVar->value);
            }
            Scope_DeleteVariable(scope, outVar);
        }
        else if (oReadOnly != NULL)
            *oReadOnly = 1;

        return;
    }

    // Functions return their address for use with function pointers.
    if (Function_GetCurrent() != NULL)
    {
        Function* func = NULL;
        if ((func = Function_Find(expr->id)) != NULL)
        {
            // TODO actual type here
            if (oType != NULL)
                *oType = Type_AddReference(&AnyVariableType);
            *oValue = Value_Register(1);
            OutWrite("mov r%i, _%s\n", Value_GetR0(oValue), expr->id);
            *oReadOnly = 0;
            return;
        };
    }
    ErrorAtLocation("Undefined reference", expr->loc);
}

void FreeExpressionTree(AST_Expression* expr, Scope* scope)
{
    switch (expr->type)
    {
        case AST_ExpressionType_UnaryOP:
            FreeExpressionTree(((AST_Expression_UnOp*)expr)->exprA, scope);
            break;
        case AST_ExpressionType_BinaryOP:;
            AST_Expression_BinOp* binop = ((AST_Expression_BinOp*)expr);
            FreeExpressionTree(binop->exprA, scope);
            // Special handling for structs, as the always have a "variable access"
            // as right side that isn't referring to an actual variable, but rather
            // a struct member. We don't want it to be treated as a normal var.
            if (binop->op == BinOp_StructAccessDot || binop->op == BinOp_StructAccessArrow)
                free(binop->exprB);
            else
                FreeExpressionTree(binop->exprB, scope);
            break;
        case AST_ExpressionType_FunctionCall:;
            AST_Expression_FunctionCall* fcall = (AST_Expression_FunctionCall*)expr;
            for (size_t i = 0; i < fcall->numParameters; i++)
                FreeExpressionTree(fcall->parameters[i], scope);
            break;
        case AST_ExpressionType_ListLiteral:;
            AST_Expression_ListLiteral* list = (AST_Expression_ListLiteral*)expr;
            for (size_t i = 0; i < list->numExpr; i++)
                FreeExpressionTree(list->expressions[i], scope);
            break;
        case AST_ExpressionType_TypeCast:
            FreeExpressionTree(((AST_Expression_TypeCast*)expr)->exprA, scope);
            break;
        case AST_ExpressionType_VariableAccess:;
            // The variable itself might get freed by the expression if this is the last time
            // it is used. Therefore when freeing the tree we also have to check if the variable
            // needs to be freed to avoid a register leak.
            Variable* outVar = NULL;
            if ((outVar = Scope_FindVariable(scope, ((AST_Expression_VariableAccess*)expr)->id)) != NULL)
                if (outVar->lastAccess == expr)
                {
                    Value_FreeValue(&outVar->value);
                    Scope_DeleteVariable(scope, outVar);
                }
            break;
        default:;
    }
    free(expr);
}

void CodeGen_Expression(AST_Expression* expr, Scope* scope, Value* oValue, VariableType** oType, bool* oReadOnly)
{
    if (Function_GetCurrent() == NULL)
        switch (expr->type)
        {
            default:
                ErrorAtLocation("Expected compile time constant", expr->loc);
            case AST_ExpressionType_IntLiteral:
            case AST_ExpressionType_VariableAccess:
            case AST_ExpressionType_StringLiteral:
            case AST_ExpressionType_ListLiteral:
            case AST_ExpressionType_TypeCast:;
        }

    switch (expr->type)
    {
        case AST_ExpressionType_IntLiteral:
            if (oValue != NULL)
            {
                *oValue = Value_Literal(((AST_Expression_IntLiteral*)expr)->literal);
                if (oType != NULL)
                    *oType = Type_AddReference(&AnyVariableType);
                *oReadOnly = false;
            }
            break;

        case AST_ExpressionType_StringLiteral:
            if (oType != NULL)
            {
                VariableType* type = xmalloc(sizeof(VariableType));
                type->token = UintKeyword;
                type->refCount = 1;
                type->structure = NULL;
                type->pointerLevel = 1;
                type->qualifiers |= Qualifier_Const;
                *oType = type;
            }
            if (oValue != NULL)
            {
                uint16_t index = AllocateAndWriteStringLiteral(((AST_Expression_StringLiteral*)expr)->data);
                *oValue = Value_Literal((int32_t)index);
                *oReadOnly = false;
            }
            break;
        case AST_ExpressionType_Value:
        {
            AST_Expression_Value* exprVal = (AST_Expression_Value*)expr;
            if (oValue != NULL)
            {
                *oValue = exprVal->value;
                *oReadOnly = exprVal->readOnly;
            }
            else if (!exprVal->readOnly)
                Value_FreeValue(&exprVal->value);
            if (oType != NULL)
                *oType = exprVal->vType;
            break;
        }
        case AST_ExpressionType_VariableAccess:
            CodeGen_VariableAccess((AST_Expression_VariableAccess*)expr, scope, oValue, oType, oReadOnly);
            break;
        case AST_ExpressionType_BinaryOP:
            CodeGen_BinaryOperator((AST_Expression_BinOp*)expr, scope, oValue, oType, oReadOnly);
            break;
        case AST_ExpressionType_UnaryOP:
            CodeGen_ExpressionUnaryOperator((AST_Expression_UnOp*)expr, scope, oValue, oType, oReadOnly);
            break;
        case AST_ExpressionType_ListLiteral:
            CodeGen_ListLiteral((AST_Expression_ListLiteral*)expr, scope, oValue, oType, oReadOnly);
            break;
        case AST_ExpressionType_FunctionCall:
            CodeGen_FunctionCall((AST_Expression_FunctionCall*)expr, scope, oValue, oType, oReadOnly);
            break;
        case AST_ExpressionType_TypeCast:
            CodeGen_TypeCast((AST_Expression_TypeCast*)expr, scope, oValue, oType, oReadOnly);
            break;
    }
    // printf("Expr %i, %i, %s; AVT %i, %i, %i\n", expr->type, expr->loc.lineNumber, expr->loc.sourceFile,
    // AnyVariableType.token, AnyVariableType.pointerLevel, AnyVariableType.qualifiers);

    free(expr);
}
