#include "CG_UnOp.h"
#include "../AST.h"
#include "../Outfile.h"
#include "../Register.h"
#include "../Stack.h"
#include "../Token.h"
#include "../Type.h"
#include "../Value.h"
#include "../Variables.h"
#include "CG_Binop.h"
#include "CG_Expression.h"
#include "CG_NativeOP.h"
#include <stdint.h>
#include <stdio.h>

void UnopIncrementPostG(Value* dstValue, Value* srcValue, bool* oReadOnly, VariableType* type)
{
    Value srcB;
    if (type->pointerLevel > 0)
    {
        type->pointerLevel--;
        srcB = Value_Literal((int32_t)SizeInWords(type));
        type->pointerLevel++;
    }
    else
        srcB = Value_Literal((int32_t)1);

    if (dstValue != NULL)
    {
        if (dstValue->addressType == AddressType_None || dstValue->addressType == AddressType_Flag)
        {
            *dstValue = Value_Register(srcValue->size);
            *oReadOnly = false;
        }
        else
            *oReadOnly = true;

        Value_GenerateMemCpy(*dstValue, *srcValue);
    }

    if (srcValue->size == 1)
        GenerateNativeOP(NativeOP_Add, srcValue, *srcValue, srcB, true, true);
    else
    {
        BinopAdd32(srcValue, srcValue, &srcB, true, true);
    }
}

void UnopDecrementPostG(Value* dstValue, Value* srcValue, bool* oReadOnly, VariableType* type)
{
    Value srcB;
    if (type->pointerLevel > 0)
    {
        type->pointerLevel--;
        srcB = Value_Literal((int32_t)SizeInWords(type));
        type->pointerLevel++;
    }
    else
        srcB = Value_Literal((int32_t)1);

    if (dstValue != NULL)
    {
        if (dstValue->addressType == AddressType_None || dstValue->addressType == AddressType_Flag)
        {
            *dstValue = Value_Register(srcValue->size);
            *oReadOnly = false;
        }
        else
            *oReadOnly = true;

        Value_GenerateMemCpy(*dstValue, *srcValue);
    }

    if (srcValue->size == 1)
        GenerateNativeOP(NativeOP_Sub, srcValue, *srcValue, srcB, true, true);
    else
    {
        BinopSub32(srcValue, srcValue, &srcB, true);
    }
}

void UnopIncrementPreG(Value* dstValue, Value* srcValue, bool* oReadOnly, VariableType* type)
{
    Value srcB;
    if (type->pointerLevel > 0)
    {
        type->pointerLevel--;
        srcB = Value_Literal((int32_t)SizeInWords(type));
        type->pointerLevel++;
    }
    else
        srcB = Value_Literal((int32_t)1);

    if (srcValue->size == 1)
        GenerateNativeOP(NativeOP_Add, srcValue, *srcValue, srcB, true, true);
    else
    {
        Value srcB = Value_Literal((uint32_t)1);
        BinopAdd32(srcValue, srcValue, &srcB, true, true);
    }

    if (dstValue != NULL)
    {
        *dstValue = *srcValue;
        *oReadOnly = true;
    }
}

void UnopDecrementPreG(Value* dstValue, Value* srcValue, bool* oReadOnly, VariableType* type)
{

    Value srcB;
    if (type->pointerLevel > 0)
    {
        type->pointerLevel--;
        srcB = Value_Literal((int32_t)SizeInWords(type));
        type->pointerLevel++;
    }
    else
        srcB = Value_Literal((int32_t)1);

    if (srcValue->size == 1)
        GenerateNativeOP(NativeOP_Sub, srcValue, *srcValue, srcB, true, true);
    else
    {
        BinopSub32(srcValue, srcValue, &srcB, true);
    }

    if (dstValue != NULL)
    {
        *dstValue = *srcValue;
        *oReadOnly = true;
    }
}

void UnopNOT(Value* dstValue, Value* srcValue, bool* oReadOnly)
{
    if (dstValue == NULL)
        return;

    if (dstValue->addressType == AddressType_None || dstValue->addressType == AddressType_Flag)
    {
        *dstValue = Value_Register(srcValue->size);
        *oReadOnly = false;
    }
    else
        *oReadOnly = true;

    if (srcValue->size == 1)
    {
        GenerateNativeOP(NativeOP_Not, dstValue, *dstValue, *srcValue, true, true);
    }
    else
    {
        bool srcHighReadOnly;
        Value srcLow = Value_GetLowerWord(srcValue);
        Value srcHigh = Value_GetUpperWord(srcValue, &srcHighReadOnly);

        bool dstHighReadOnly;
        Value dstLow = Value_GetLowerWord(dstValue);
        Value dstHigh = Value_GetUpperWord(dstValue, &dstHighReadOnly);

        GenerateNativeOP(NativeOP_Not, &dstLow, dstLow, srcLow, true, true);
        GenerateNativeOP(NativeOP_Not, &dstHigh, dstHigh, srcHigh, true, true);

        if (!srcHighReadOnly)
            Value_FreeValue(&srcHigh);

        if (!dstHighReadOnly)
            Value_FreeValue(&dstHigh);
    }
}

void UnopNegate(Value* dstValue, Value* srcValue, bool* oReadOnly)
{
    if (dstValue == NULL)
        return;

    *oReadOnly = true;

    if (dstValue->addressType == AddressType_None || dstValue->addressType == AddressType_Flag)
        *oReadOnly = false;

    if (srcValue->size == 1)
        GenerateNativeOP(NativeOP_Sub, dstValue, Value_FromRegister(-1), *srcValue, true, true);
    else
    {
        Value srcA = Value_FromRegister(-1);
        BinopSub32(dstValue, &srcA, srcValue, true);
    }
}

void UnopLogicalNOT(Value* dstValue, Value* srcValue, bool* oReadOnly)
{
    if (dstValue == NULL)
        return;
    Value rZ = Value_FromRegister(-1);
    BinopEquality(BinOp_Equal, dstValue, srcValue, &rZ, true, true);
    *oReadOnly = false;
}

void CodeGen_Dereference(AST_Expression_UnOp* expr, Scope* scope, Value* oValue, VariableType** oType, bool* oReadOnly)
{
    if (oValue == NULL)
    {
        FreeExpressionTree(expr->exprA, scope);
        return;
    }

    Value src = NullValue;
    VariableType* srcType;
    bool srcReadOnly;
    CodeGen_Expression(expr->exprA, scope, &src, &srcType, &srcReadOnly);

    if (srcType->pointerLevel == 0)
        ErrorAtLocation("Invalid dereference", expr->loc);

    srcType = Type_Copy(srcType);
    srcType->pointerLevel--;

    if (oType != NULL)
        *oType = Type_AddReference(srcType);

    // TODO optimize types other than register.
    Value addr = NullValue;
    if (!srcReadOnly && src.addressType == AddressType_Register)
    {
        addr = src;
        *oReadOnly = false;
    }
    else
    {
        if (src.addressType != AddressType_Register)
        {
            addr = Value_Register(1);
            Value_GenerateMemCpy(addr, src);
            *oReadOnly = false;

            if (!srcReadOnly)
                Value_FreeValue(&src);
        }
        else
        {
            addr = src;
            *oReadOnly = true;
        }
    }

    addr.addressType = AddressType_MemoryRegister;
    addr.size = SizeInWords(srcType);
    Type_RemoveReference(srcType);
    *oValue = addr;
}

void CodeGen_AddressOf(AST_Expression_UnOp* expr, Scope* scope, Value* oValue, VariableType** oType, bool* oReadOnly)
{
    if (oValue == NULL)
    {
        FreeExpressionTree(expr->exprA, scope);
        return;
    }

    Value src = NullValue;
    VariableType* srcType;
    bool srcReadOnly;
    CodeGen_Expression(expr->exprA, scope, &src, &srcType, &srcReadOnly);

    srcType = Type_Copy(srcType);
    srcType->pointerLevel++;

    if (oType != NULL)
        *oType = Type_AddReference(srcType);
    Type_RemoveReference(srcType);

    if (src.addressType == AddressType_Memory)
    {
        *oValue = src;
        oValue->addressType = AddressType_Literal;
        oValue->size = 1;
        *oReadOnly = false;
    }
    else if (src.addressType == AddressType_MemoryRelative)
    {
        // this could return a literal, as the start stack size is known
        // Would reduce portability though.

        // If the out value is on the stack, GenerateNativeOP
        // might offset the SP to its address. In doing so,
        // our offset calculation would be wrong, so we
        // already align here, before the offset calculation.
        // This could be done better, since we rely on GenerateNativeOPs
        // internal behaviour here.
        if (oValue->addressType == AddressType_MemoryRelative)
            Stack_ToAddress((int)oValue->address);

        int delta = Stack_GetDelta((int)src.address);

        if (delta > 0)
        {
            GenerateNativeOP(NativeOP_Sub, oValue, Value_FromRegister(Register_SP), Value_Literal((int32_t)delta), true,
                             true);
            *oReadOnly = false;
        }
        else if (delta < 0)
        {
            GenerateNativeOP(NativeOP_Add, oValue, Value_FromRegister(Register_SP), Value_Literal((int32_t)(-delta)),
                             true, true);
            *oReadOnly = false;
        }
        else
        {
            GenerateNativeOP(NativeOP_Add, oValue, Value_FromRegister(Register_SP), Value_FromRegister(-1), true, true);
            *oReadOnly = false;
        }
    }
    else if (src.addressType == AddressType_MemoryRegister)
    {
        if (!srcReadOnly)
        {
            src.addressType = AddressType_Register;
            src.size = 1;
            *oValue = src;
            *oReadOnly = false;
        }
        else
        {
            *oValue = Value_Register(1);
            src.addressType = AddressType_Register;
            Value_GenerateMemCpy(*oValue, src);
            oValue->size = 1;
            *oReadOnly = false;
        }
    }
    else
    {
        ErrorAtLocation("Cannot get address of variable", expr->loc);
    }
}

void CodeGen_ExpressionUnaryOperator(AST_Expression_UnOp* expr, Scope* scope, Value* oValue, VariableType** oType,
                                     bool* oReadOnly)
{

    if (expr->op == UnOp_AddressOf)
        CodeGen_AddressOf(expr, scope, oValue, oType, oReadOnly);
    else if (expr->op == UnOp_Dereference)
        CodeGen_Dereference(expr, scope, oValue, oType, oReadOnly);
    else
    {
        Value in = NullValue;
        VariableType* inType;
        bool inReadOnly;
        CodeGen_Expression(expr->exprA, scope, &in, &inType, &inReadOnly);

        if (in.addressType == AddressType_Literal && oValue != NULL)
        {
            oValue->addressType = AddressType_Literal;
            oValue->size = in.size;
            *oReadOnly = true;

            switch (expr->op)
            {
                case UnOp_LogicalNOT:
                    oValue->address = (int32_t)(!((int)in.address));
                    break;
                case UnOp_BitwiseNOT:
                    oValue->address = ~in.address;
                    break;
                case UnOp_Negate:
                    oValue->address = -in.address;
                    break;
                default:
                    ErrorAtLocation("Invalid unop", expr->loc);
            }
        }
        else
        {
            switch (expr->op)
            {
                case UnOp_PostDecrement:
                    UnopDecrementPostG(oValue, &in, oReadOnly, inType);
                    break;
                case UnOp_PreDecrement:
                    UnopDecrementPreG(oValue, &in, oReadOnly, inType);
                    break;
                case UnOp_PostIncrement:
                    UnopIncrementPostG(oValue, &in, oReadOnly, inType);
                    break;
                case UnOp_PreIncrement:
                    UnopIncrementPreG(oValue, &in, oReadOnly, inType);
                    break;
                case UnOp_LogicalNOT:
                    UnopLogicalNOT(oValue, &in, oReadOnly);
                    break;
                case UnOp_Negate:
                    UnopNegate(oValue, &in, oReadOnly);
                    break;
                case UnOp_BitwiseNOT:
                    UnopNOT(oValue, &in, oReadOnly);
                    break;
                default:
                    ErrorAtLocation("Invalid unop", expr->loc);
            }

            if (!inReadOnly)
                Value_FreeValue(&in);
        }

        if (oType != NULL)
        {
            if (inType->token != None)
                *oType = Type_AddReference(inType);
            else
                *oType = Type_AddReference(&AnyVariableType);
        }
        Type_RemoveReference(inType);
    }
}