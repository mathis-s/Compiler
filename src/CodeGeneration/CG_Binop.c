#include "CG_Binop.h"
#include "../AST.h"
#include "../Flags.h"
#include "../Outfile.h"
#include "../Register.h"
#include "../Stack.h"
#include "../Token.h"
#include "../Type.h"
#include "../Util.h"
#include "../Value.h"
#include "CG_Expression.h"
#include "CG_NativeOP.h"

#include <stdint.h>
#include <stdio.h>

void CodeGen_StructMemberAccess(AST_Expression_BinOp* expr, Scope* scope, Value* oValue, VariableType** oType,
                                bool* oReadOnly)
{

    Value structValue = NullValue;
    VariableType* structType = NULL;
    bool structReadOnly;
    CodeGen_Expression(expr->exprA, scope, &structValue, &structType, &structReadOnly);

    if (expr->exprB->type != AST_ExpressionType_VariableAccess)
        ErrorAtLocation("Invalid struct member", expr->exprB->loc);

    AST_Expression_VariableAccess* memberExpr = (AST_Expression_VariableAccess*)expr->exprB;
    if (memberExpr->type != AST_ExpressionType_VariableAccess)
        ErrorAtLocation("Invalid struct access", expr->loc);

    Struct* outStruct = structType->structure;
    assert(outStruct != NULL);

    Variable* outStructMemberVar = NULL;
    if ((outStructMemberVar = GenericList_Find(&outStruct->members, CompareVariableToID, memberExpr->id)) == NULL)
        ErrorAtLocation("Invalid struct member", expr->exprB->loc);

    free(expr->exprB);

    if (oValue == NULL)
    {
        bool err;
        if ((structType->pointerLevel == 0 && expr->op == BinOp_StructAccessDot) ||
            (structType->pointerLevel == 1 && expr->op == BinOp_StructAccessArrow))
        {
            err = false;
        }
        else
            err = true;

        Type_RemoveReference(structType);

        if (err)
            ErrorAtLocation("Invalid struct access", expr->loc);

        return;
    }

    int size = SizeInWords(outStructMemberVar->type);

    assert(size != 0);

    if (oType != NULL)
        *oType = Type_AddReference(outStructMemberVar->type);

    // Struct access via dot
    if (structType->pointerLevel == 0 && expr->op == BinOp_StructAccessDot)
    {
        Value srcValue;
        if (structValue.addressType == AddressType_MemoryRelative)
        {
            srcValue = Value_MemoryRelative((int)(structValue.address - outStructMemberVar->value.address),
                                            outStructMemberVar->value.size);

            *oReadOnly = structReadOnly;
        }

        else if (structValue.addressType == AddressType_Memory)
        {
            srcValue = Value_Memory((uint16_t)(structValue.address + outStructMemberVar->value.address),
                                    outStructMemberVar->value.size);

            *oReadOnly = structReadOnly;
        }

        else if (structValue.addressType == AddressType_MemoryRegister)
        {
            if (outStructMemberVar->value.address == 0)
            {
                srcValue = structValue;
                srcValue.size = outStructMemberVar->value.size;
                *oReadOnly = structReadOnly;
            }
            else if (!structReadOnly)
            {
                // We want to change the address so it points to the member var we're interested in.
                // We don't want to change the value itself. So we interpret the value as a register,
                // not a pointer.

                structValue.addressType = AddressType_Register;

                srcValue = structValue;
                OutWrite("add r%i, %i\n", Value_GetR0(&srcValue), Value_Literal(outStructMemberVar->value.address));

                *oReadOnly = false;

                // Back to a pointer!
                srcValue.addressType = AddressType_MemoryRegister;
                srcValue.size = outStructMemberVar->value.size;
            }
            else
            {
                srcValue = Value_Register(1);
                structValue.addressType = AddressType_Register;

                GenerateNativeOP(NativeOP_Add, &srcValue, structValue, Value_Literal(outStructMemberVar->value.address),
                                 true, true);
                structValue.addressType = AddressType_MemoryRegister;
                *oReadOnly = false;
                srcValue.addressType = AddressType_MemoryRegister;
                srcValue.size = outStructMemberVar->value.size;
            }
        }
        else
        {
            ErrorAtLocation("Invalid struct access!", expr->loc);
            exit(0);
        }

        *oValue = srcValue;
    }

    // Struct pointer access via ->
    else if (structType->pointerLevel == 1 && expr->op == BinOp_StructAccessArrow)
    {
        Value addr = NullValue;

        if (outStructMemberVar->value.address == 0 && structValue.addressType == AddressType_Register)
        {
            addr = structValue;
            addr.addressType = AddressType_MemoryRegister;
            addr.size = outStructMemberVar->value.size;
            *oReadOnly = structReadOnly;
            *oValue = addr;
        }
        else
        {
            // (Make sure oValue is a register, as we will turn it into a memory register)
            if (oValue->addressType != AddressType_None && oValue->addressType != AddressType_Register)
                *oValue = Value_Register(1);

            // Assertion should also hold
            // assert(oValue->addressType == AddressType_None);

            GenerateNativeOP(NativeOP_Add, oValue, structValue, Value_Literal(outStructMemberVar->value.address),
                             structReadOnly, true);
            oValue->addressType = AddressType_MemoryRegister;
            oValue->size = outStructMemberVar->value.size;
            *oReadOnly = false;

            /*addr = Value_FromRegister(GetFreeRegister());
            Value_GenerateMemCpy(addr, structValue);
            if (outStructMemberVar->value.address != 0)
                OutWrite("add r%i, %i\n", Value_GetR0(&addr), outStructMemberVar->value.address);
            addr.addressType = AddressType_MemoryRegister;
            addr.size = outStructMemberVar->value.size;
            *oReadOnly = false;

            if (!structReadOnly)
                Value_FreeValue(&structValue);*/
        }
    }
    else
        ErrorAtLocation("Invalid struct access!", expr->loc);

    Type_RemoveReference(structType);
}

void CodeGen_Assignment(AST_Expression_BinOp* expr, Scope* scope, Value* oValue, VariableType** oType, bool* oReadOnly)
{
    Value leftVal = NullValue;
    bool leftReadOnly = true;
    VariableType* leftType = NULL;
    bool isNotVariableAccess = (expr->exprA->type != AST_ExpressionType_VariableAccess);
    CodeGen_Expression(expr->exprA, scope, &leftVal, &leftType, &leftReadOnly);

    if (!leftReadOnly && leftVal.addressType != AddressType_MemoryRegister && isNotVariableAccess)
        ErrorAtLocation("Cannot assign rvalue", expr->loc);

    if (oType != NULL)
        *oType = Type_AddReference(leftType);

    // += -= *= ...
    if (expr->op != BinOp_Assignment)
    {
        if (!IsPrimitiveType(leftType))
            ErrorAtLocation("Invalid assignment", expr->loc);

        AST_Expression_Value* exprA = xmalloc(sizeof(AST_Expression_Value));
        exprA->type = AST_ExpressionType_Value;
        exprA->value = leftVal;
        exprA->loc = expr->loc;
        exprA->readOnly = true;
        exprA->vType = Type_AddReference(leftType);

        AST_Expression_BinOp binOp;
        binOp.exprA = (AST_Expression*)exprA;
        binOp.exprB = expr->exprB;
        binOp.op = expr->op - BinOp_AssignmentAdd;
        binOp.loc = expr->loc;
        binOp.type = AST_ExpressionType_BinaryOP;

        Value outVal = leftVal;
        bool outReadOnly;
        VariableType* outType;

        CodeGen_BinaryOperator(&binOp, scope, &outVal, &outType, &outReadOnly);
        Type_RemoveReference(outType);

        if (!Value_Equals(&outVal, &leftVal))
        {
            Value_GenerateMemCpy(leftVal, outVal);
            if (oValue != NULL)
            {
                *oValue = outVal;
                *oReadOnly = outReadOnly;
            }
            else if (!outReadOnly)
                Value_FreeValue(&outVal);

            if (!leftReadOnly)
                Value_FreeValue(&leftVal);
        }
        else if (oValue != NULL)
        {
            *oValue = leftVal;
            *oReadOnly = leftReadOnly;
        }
        else if (!leftReadOnly)
            Value_FreeValue(&leftVal);
    }
    // Normal assignment
    else
    {
        int oldStackSize = Stack_GetSize();
        Value rightVal = NullValue;

        // If normal assignment, we request that the value gets put into the assignee.
        if (expr->op == BinOp_Assignment)
            rightVal = leftVal;

        VariableType* rightType = NULL;
        bool rightReadOnly = true;
        CodeGen_Expression(expr->exprB, scope, &rightVal, &rightType, &rightReadOnly);

        if (leftVal.addressType == AddressType_MemoryRelative)
        {
            leftVal.address += (int32_t)(Stack_GetSize() - oldStackSize);
        }

        if (!Type_Check(leftType, rightType))
            ErrorAtLocation("Invalid assignment types", expr->loc);

        Type_RemoveReference(rightType);
        // For assignment: Our request that the result gets stored into the assignee was fulfilled, so there's
        // nothing to do.
        if (expr->op == BinOp_Assignment && Value_Equals(&leftVal, &rightVal))
        {
            if (oValue != NULL)
            {
                *oValue = leftVal;
                *oReadOnly = leftReadOnly;
            }
            else if (!leftReadOnly)
                Value_FreeValue(&leftVal);
        }
        else
        {
            // Since normal assignment is also valid for non-primitive types
            // we simply copy the value of src to dst, no case distinction
            Value_GenerateMemCpy(leftVal, rightVal);
            if (oValue != NULL)
            {
                *oValue = rightVal;
                *oReadOnly = rightReadOnly;
            }
            else if (!rightReadOnly)
                Value_FreeValue(&rightVal);

            // In case left was a memory register or a deleted variable
            if (!leftReadOnly)
                Value_FreeValue(&leftVal);
        }
    }
    Type_RemoveReference(leftType);
}

void BinopAdd32(Value* dstValue, Value* srcA, Value* srcB, bool srcAro, bool srcBro)
{
    bool srcAHighRO;
    bool srcBHighRO;
    bool dstHighRO;

    Value srcALow = Value_GetLowerWord(srcA);
    Value srcAHigh = Value_GetUpperWord(srcA, &srcAHighRO);
    Value srcBLow = Value_GetLowerWord(srcB);
    Value srcBHigh = Value_GetUpperWord(srcB, &srcBHighRO);

    Value dstLow = Value_GetLowerWord(dstValue);
    Value dstHigh = Value_GetUpperWord(dstValue, &dstHighRO);

    Value* carry;
    if (!srcAro && srcA->addressType == AddressType_Register)
        carry = &srcAHigh;
    else if (!srcBro && srcB->addressType == AddressType_Register)
        carry = &srcBHigh;
    else
    {
        Value temp = Value_Register(1);
        Value_GenerateMemCpy(temp, srcAHigh);
        if (!srcAHighRO)
            Value_FreeValue(&srcAHigh);
        srcAHigh = temp;
        srcAHighRO = false;
        carry = &srcAHigh;
    }

    // Low Word
    GenerateNativeOP(NativeOP_Add, &dstLow, srcALow, srcBLow, true, true);

    // Carry
    OutWrite("add_c r%i, 1\n", Value_GetR0(carry));

    // High Word
    GenerateNativeOP(NativeOP_Add, &dstHigh, srcAHigh, srcBHigh, true, true);

    if (!srcAHighRO)
        Value_FreeValue(&srcAHigh);
    if (!srcBHighRO)
        Value_FreeValue(&srcBHigh);
    if (!dstHighRO)
        Value_FreeValue(&dstHigh);
}

void BinopSub32(Value* dstValue, Value* srcA, Value* srcB, bool srcAro)
{

    bool srcAHighRO;
    bool srcBHighRO;
    bool dstHighRO;

    Value srcALow = Value_GetLowerWord(srcA);
    Value srcAHigh = Value_GetUpperWord(srcA, &srcAHighRO);
    Value srcBLow = Value_GetLowerWord(srcB);
    Value srcBHigh = Value_GetUpperWord(srcB, &srcBHighRO);

    Value dstLow = Value_GetLowerWord(dstValue);
    Value dstHigh = Value_GetUpperWord(dstValue, &dstHighRO);

    Value* carry;
    if (!srcAro && srcA->addressType == AddressType_Register)
        carry = &srcAHigh;
    // TODO could use srcB as carry, just add 1 instead
    // else if (!srcBro && srcA->addressType == AddressType_Register)
    //    carry = &srcBHigh;
    else
    {
        Value temp = Value_Register(1);
        Value_GenerateMemCpy(temp, srcAHigh);
        srcAHigh = temp;
        srcAHighRO = false;
        carry = &srcAHigh;
    }

    // Low Word
    GenerateNativeOP(NativeOP_Sub, &dstLow, srcALow, srcBLow, true, true);

    // Borrow
    OutWrite("sub_nc r%i, 1\n", Value_GetR0(carry));

    // High Word
    GenerateNativeOP(NativeOP_Sub, &dstHigh, srcAHigh, srcBHigh, true, true);

    if (!srcAHighRO)
        Value_FreeValue(&srcAHigh);
    if (!srcBHighRO)
        Value_FreeValue(&srcBHigh);
    if (!dstHighRO)
        Value_FreeValue(&dstHigh);
}

void BinopBitwise32(BinOp op, Value* dstValue, Value* srcA, Value* srcB)
{
    bool srcAHighRO;
    bool srcBHighRO;
    bool dstHighRO;

    Value srcALow = Value_GetLowerWord(srcA);
    Value srcAHigh = Value_GetUpperWord(srcA, &srcAHighRO);
    Value srcBLow = Value_GetLowerWord(srcB);
    Value srcBHigh = Value_GetUpperWord(srcB, &srcBHighRO);

    Value dstLow = Value_GetLowerWord(dstValue);
    Value dstHigh = Value_GetUpperWord(dstValue, &dstHighRO);

    // Low Word
    GenerateNativeOP((NativeOP)op, &dstLow, srcALow, srcBLow, true, true);

    // High Word
    GenerateNativeOP((NativeOP)op, &dstHigh, srcAHigh, srcBHigh, true, true);

    if (!srcAHighRO)
        Value_FreeValue(&srcAHigh);
    if (!srcBHighRO)
        Value_FreeValue(&srcBHigh);
    if (!dstHighRO)
        Value_FreeValue(&dstHigh);
}

void BinopEquality(BinOp op, Value* dstValue, Value* srcA, Value* srcB, bool srcAro, bool srcBro)
{
    if (srcA->size == 2)
    {
        bool srcAHighRO;
        bool srcBHighRO;
        bool dstRO = true;

        Value srcALow = Value_GetLowerWord(srcA);
        Value srcAHigh = Value_GetUpperWord(srcA, &srcAHighRO);
        Value srcBLow = Value_GetLowerWord(srcB);
        Value srcBHigh = Value_GetUpperWord(srcB, &srcBHighRO);

        Value dstLow;
        Value dstHigh;

        if (!srcAro && srcA->addressType == AddressType_Register)
        {
            dstLow = srcALow;
            dstHigh = srcAHigh;
        }
        else if (!srcBro && srcB->addressType == AddressType_Register)
        {
            dstLow = srcBLow;
            dstHigh = srcBHigh;
        }
        else
        {
            // TODO use fewer registers here.
            dstRO = false;
            dstHigh = Value_Register(1);
        }

        // High Word
        GenerateNativeOP(NativeOP_Xor, &dstHigh, srcAHigh, srcBHigh, true, true);

        if (!srcAHighRO)
            Value_FreeValue(&srcAHigh);
        if (!srcBHighRO)
            Value_FreeValue(&srcBHigh);

        if (dstRO == false)
        {
            dstLow = Value_Register(1);
        }

        // Low Word
        GenerateNativeOP(NativeOP_Xor, &dstLow, srcALow, srcBLow, true, true);

        // Combine
        GenerateNativeOP(NativeOP_Or, &dstLow, dstLow, dstHigh, true, true);

        // TODO could optimize inequality by return dstLow here...

        if (!dstRO)
        {
            Value_FreeValue(&dstLow);
            Value_FreeValue(&dstHigh);
        }
    }
    else
    {
        Value dst = Value_FromRegister(-1);
        // If srcA is temp anyways, we might as well override it and only use a
        // 2-operand instruction
        if (!srcAro && srcA->addressType == AddressType_Register)
            dst = *srcA;
        GenerateNativeOP(NativeOP_Xor, &dst, *srcA, *srcB, true, true);
    }

    Flag returnFlag = Flag_None;
    if (op == BinOp_Equal)
        returnFlag = Flag_Z;
    else
        returnFlag = Flag_NZ;

    if (dstValue->addressType == AddressType_Flag)
        dstValue->address = (int32_t)returnFlag;
    else
    {
        // Ignore dst requests if they are not registers
        if (dstValue->addressType != AddressType_Register)
        {
            *dstValue = Value_Register(1);
        }

        assert(dstValue->size == 1);

        OutWrite("mov r%i, 0\n", Value_GetR0(dstValue));
        OutWrite("mov%s r%i, 1\n", Flags_FlagToString(returnFlag), Value_GetR0(dstValue));
    }
}

void BinopMul32(Value* dstValue, Value* srcA, Value* srcB, bool srcAro, bool srcBro, const VariableType* type)
{

    if (type->token == FixedKeyword)
    {
        GenerateNativeOP(NativeOP_MulQ, dstValue, *srcA, *srcB, srcAro, srcBro);
    }
    else
    {
        if (Value_Equals(dstValue, srcA) || Value_Equals(dstValue, srcB))
            *dstValue = Value_Register(2);

        bool srcAHighRO;
        bool srcBHighRO;
        bool dstHighRO;

        Value srcALow = Value_GetLowerWord(srcA);
        Value srcAHigh = Value_GetUpperWord(srcA, &srcAHighRO);
        Value srcBLow = Value_GetLowerWord(srcB);
        Value srcBHigh = Value_GetUpperWord(srcB, &srcBHighRO);

        Value dstLow = Value_GetLowerWord(dstValue);
        Value dstHigh = Value_GetUpperWord(dstValue, &dstHighRO);

        /*
        16x16->32

        To do 32x32->64

        l*l = __hl
        l*h = _hl_
        h*l = _hl_
        h*h = hl__ <- dont care as 32-bit result
        */

        Value temp;
        bool tempReadOnly;
        if (dstLow.addressType == AddressType_Register)
        {
            temp = dstLow;
            tempReadOnly = true;
        }
        else
        {
            tempReadOnly = false;
            temp = Value_Register(1);
        }

        // h*h
        GenerateNativeOP(NativeOP_MulH, &dstHigh, srcALow, srcBLow, true, true);

        // l*h
        GenerateNativeOP(NativeOP_Mul, &temp, srcALow, srcBHigh, true, true);
        GenerateNativeOP(NativeOP_Add, &dstHigh, dstHigh, temp, true, true);

        // h*l
        GenerateNativeOP(NativeOP_Mul, &temp, srcAHigh, srcBLow, true, true);
        GenerateNativeOP(NativeOP_Add, &dstHigh, dstHigh, temp, true, true);

        // l*l
        GenerateNativeOP(NativeOP_Mul, &dstLow, srcALow, srcBLow, true, true);

        if (!srcAHighRO)
            Value_FreeValue(&srcAHigh);
        if (!srcBHighRO)
            Value_FreeValue(&srcBHigh);
        if (!dstHighRO)
            Value_FreeValue(&dstHigh);
        if (!tempReadOnly)
            Value_FreeValue(&temp);
    }
}

void BinopDiv32(Value* dstValue, Value* srcA, Value* srcB, bool srcAro, bool srcBro, const VariableType* type)
{
    if (type->token == FixedKeyword)
    {
        GenerateNativeOP(NativeOP_InvQ, dstValue, *srcB, *srcB, srcBro, srcBro);
        GenerateNativeOP(NativeOP_MulQ, dstValue, *dstValue, *srcA, false, srcAro);
    }
    else
    {
        Error("Not implemented");
    }
}

void BinopComparison(BinOp op, Value* dstValue, Value* srcA, Value* srcB, bool srcAro, VariableType* type)
{
    bool srcAHighRO;
    bool srcBHighRO;

    if (op == BinOp_GreaterThan || op == BinOp_LessThanEq)
    {
        Value* temp = srcA;
        srcA = srcB;
        srcB = temp;
    }

    // Zero Register as destination
    if (srcA->size == 2)
    {
        // TODO maybe ignore upper word when comparing to literal

        Value srcALow = Value_GetLowerWord(srcA);
        Value srcAHigh = Value_GetUpperWord(srcA, &srcAHighRO);
        Value srcBLow = Value_GetLowerWord(srcB);
        Value srcBHigh = Value_GetUpperWord(srcB, &srcBHighRO);

        Value dstLow;
        Value dstHigh;

        if (!srcAro)
        {
            dstLow = srcALow;
            dstHigh = srcAHigh;
        }
        else
        {
            dstLow = Value_FromRegister(-1);
            dstHigh = Value_FromRegister(-1);
        }

        Value* carry;
        if (!srcAro && srcA->addressType == AddressType_Register)
            carry = &srcAHigh;
        // else if (!srcBro && srcA->addressType == AddressType_Register)
        //     carry = &srcBHigh;
        else
        {
            Value temp = Value_Register(1);
            Value_GenerateMemCpy(temp, srcAHigh);
            srcAHigh = temp;
            srcAHighRO = false;
            carry = &srcAHigh;
        }

        // Low Word
        GenerateNativeOP(NativeOP_Sub, &dstLow, srcALow, srcBLow, true, true);

        // Borrow
        OutWrite("sub_nc r%i, 1\n", Value_GetR0(carry));

        // High Word
        GenerateNativeOP(NativeOP_Sub, &dstHigh, srcAHigh, srcBHigh, true, true);

        if (!srcAHighRO)
            Value_FreeValue(&srcAHigh);
        if (!srcBHighRO)
            Value_FreeValue(&srcBHigh);
    }
    else
    {
        Value dst = Value_FromRegister(-1);
        // If srcA is temp anyways, we might as well override it and only use a
        // 2-operand instruction
        if (!srcAro && srcA->addressType == AddressType_Register)
            dst = *srcA;
        GenerateNativeOP(NativeOP_Sub, &dst, *srcA, *srcB, true, true);
    }

    Flag returnFlag = Flag_None;
    switch (op)
    {
        case BinOp_GreaterThan:
        case BinOp_LessThan:
            if ((type)->token == UintKeyword || (type)->token == Uint32Keyword)
                returnFlag = Flag_NC;
            else
                returnFlag = Flag_S;
            break;
        case BinOp_LessThanEq:
        case BinOp_GreaterThanEq:
            if ((type)->token == UintKeyword || (type)->token == Uint32Keyword)
                returnFlag = Flag_C;
            else
                returnFlag = Flag_NS;
            break;
        default:
            assert(0);
            break;
    }

    if (dstValue->addressType == AddressType_Flag)
        dstValue->address = (int32_t)returnFlag;
    else
    {
        // Ignore dst requests if they are not registers
        if (dstValue->addressType != AddressType_Register || dstValue->size != 1)
        {
            if (dstValue->addressType != AddressType_None)
                Value_FreeValue(dstValue);

            *dstValue = Value_Register(1);
        }

        OutWrite("mov r%i, 0\n", Value_GetR0(dstValue));
        OutWrite("mov%s r%i, 1\n", Flags_FlagToString(returnFlag), Value_GetR0(dstValue));
    }
}

void BinopMod(Value* dstValue, Value* srcA, Value* srcB)
{
    if (srcA->size == 2)
        Error("Not implemented");

    bool tempReadOnly = false;
    Value temp;
    // if (dstValue->addressType != AddressType_Register)
    temp = Value_Register(1);
    /*else
    {
        temp = *dstValue;
        tempReadOnly = true;
    }*/

    GenerateNativeOP(NativeOP_Div, &temp, *srcA, *srcB, true, true);
    GenerateNativeOP(NativeOP_Mul, &temp, temp, *srcB, true, true);
    GenerateNativeOP(NativeOP_Sub, dstValue, *srcA, temp, true, true);

    if (!tempReadOnly)
        Value_FreeValue(&temp);
}

void BinopShiftLeft32(Value* dstValue, Value* srcA, Value* srcB)
{
    bool srcAHighRO;
    bool dstHighRO;

    Value srcALow = Value_GetLowerWord(srcA);

    Value srcBLow = Value_GetLowerWord(srcB);

    Value dstLow = Value_GetLowerWord(dstValue);

    Value outShifted;
    Value n;
    bool tempRegisters = true;
    if (dstValue->addressType == AddressType_Register)
    {
        bool temp;
        outShifted = dstLow;
        n = Value_GetUpperWord(dstValue, &temp);
        assert(temp);
        tempRegisters = false;
    }
    else
    {
        outShifted = Value_Register(1);
        n = Value_Register(1);
    }

    OutWrite("mov r%i, 65535\n", Value_GetR0(&outShifted));
    GenerateNativeOP(NativeOP_Sub, &n, Value_Literal((int32_t)16), srcBLow, true, true);
    GenerateNativeOP(NativeOP_ShiftLeft, &outShifted, outShifted, n, true, true);

    GenerateNativeOP(NativeOP_And, &outShifted, outShifted, srcALow, true, true);
    GenerateNativeOP(NativeOP_ShiftRight, &outShifted, outShifted, n, true, true);

    if (tempRegisters)
        Value_FreeValue(&n);

    Value srcAHigh = Value_GetUpperWord(srcA, &srcAHighRO);
    Value dstHigh = Value_GetUpperWord(dstValue, &dstHighRO);

    GenerateNativeOP(NativeOP_ShiftLeft, &dstHigh, srcAHigh, srcBLow, true, true);

    if (!srcAHighRO)
        Value_FreeValue(&srcAHigh);

    GenerateNativeOP(NativeOP_Or, &dstHigh, dstHigh, outShifted, true, true);

    if (tempRegisters)
        Value_FreeValue(&outShifted);

    if (!dstHighRO)
        Value_FreeValue(&dstHigh);

    GenerateNativeOP(NativeOP_ShiftLeft, &dstLow, srcALow, srcBLow, true, true);
}

void BinopShiftRight32(Value* dstValue, Value* srcA, Value* srcB)
{
    bool srcAHighRO;
    bool dstHighRO;

    Value srcALow = Value_GetLowerWord(srcA);

    Value srcBLow = Value_GetLowerWord(srcB);

    Value dstLow = Value_GetLowerWord(dstValue);

    Value outShifted;
    Value n;
    bool tempRegisters = true;
    if (dstValue->addressType == AddressType_Register)
    {
        bool temp;
        outShifted = Value_GetUpperWord(dstValue, &temp);
        n = dstLow;
        assert(temp);
        tempRegisters = false;
    }
    else
    {
        outShifted = Value_Register(1);
        n = Value_Register(1);
    }

    OutWrite("mov r%i, 65535\n", Value_GetR0(&outShifted));
    GenerateNativeOP(NativeOP_Sub, &n, Value_Literal((int32_t)16), srcBLow, true, true);
    GenerateNativeOP(NativeOP_ShiftRight, &outShifted, outShifted, n, true, true);

    Value srcAHigh = Value_GetUpperWord(srcA, &srcAHighRO);

    GenerateNativeOP(NativeOP_And, &outShifted, outShifted, srcAHigh, true, true);
    GenerateNativeOP(NativeOP_ShiftLeft, &outShifted, outShifted, n, true, true);

    if (tempRegisters)
        Value_FreeValue(&n);

    GenerateNativeOP(NativeOP_ShiftRight, &dstLow, srcALow, srcBLow, true, true);
    GenerateNativeOP(NativeOP_Or, &dstLow, dstLow, outShifted, true, true);

    if (tempRegisters)
        Value_FreeValue(&outShifted);

    Value dstHigh = Value_GetUpperWord(dstValue, &dstHighRO);

    GenerateNativeOP(NativeOP_ShiftRight, &dstHigh, srcAHigh, srcBLow, true, true);

    if (!srcAHighRO)
        Value_FreeValue(&srcAHigh);
    if (!dstHighRO)
        Value_FreeValue(&dstHigh);
}

void CodeGen_ArrayAccess(AST_Expression_BinOp* expr, Scope* scope, Value* oValue, VariableType** oType, bool* oReadOnly)
{

    Value arrayValue = NullValue;
    VariableType* arrayType = NULL;
    bool arrayReadOnly = true;

    CodeGen_Expression(expr->exprA, scope, &arrayValue, &arrayType, &arrayReadOnly);

    int oldStackSize = Stack_GetSize();

    Value indexValue = NullValue;
    VariableType* indexType = NULL;
    bool indexReadOnly = true;

    if (arrayValue.addressType == AddressType_MemoryRelative)
        arrayValue.address += (int32_t)(Stack_GetSize() - oldStackSize);

    CodeGen_Expression(expr->exprB, scope, &indexValue, &indexType, &indexReadOnly);

    if (oValue == NULL)
    {
        if (!arrayReadOnly)
            Value_FreeValue(&arrayValue);
        if (!indexReadOnly)
            Value_FreeValue(&indexValue);

        Type_RemoveReference(arrayType);
        Type_RemoveReference(indexType);
        return;
    }
    indexType = Type_Copy(indexType);

    if (indexType->token == None)
        indexType->token = UintKeyword;
    if (indexValue.size != 1 || (indexType->token != UintKeyword && indexType->token != IntKeyword))
        ErrorAtLocation("Invalid array indexing type!", expr->loc);

    if (arrayType->pointerLevel > 0)
    {
        arrayType->refCount++; // Reference is removed later
        VariableType* memberType = Type_Copy(arrayType);
        memberType->pointerLevel--;

        if (oType != NULL)
            *oType = Type_AddReference(memberType);

        if (indexReadOnly || indexValue.addressType != AddressType_Register)
        {
            *oValue = Value_Register(1);
            // Value_GenerateMemCpy(*oValue, indexValue);
        }
        else
            *oValue = indexValue;

        if (SizeInWords(memberType) != 1)
        {
            // OutWrite("mul r%i, %i\n", Value_GetR0(oValue), SizeInWords(arrayType));
            GenerateNativeOP(NativeOP_Mul, oValue, indexValue, Value_Literal((int32_t)SizeInWords(memberType)),
                             indexReadOnly, true);
            GenerateNativeOP(NativeOP_Add, oValue, *oValue, arrayValue, true, arrayReadOnly);
        }
        else
        {
            // TODO (maybe indexReadOnly causes problems when indexValue = *oValue)
            GenerateNativeOP(NativeOP_Add, oValue, indexValue, arrayValue, indexReadOnly, arrayReadOnly);
        }

        oValue->addressType = AddressType_MemoryRegister;
        oValue->size = SizeInWords(memberType);
        *oReadOnly = false;
        Type_RemoveReference(memberType);
    }
    else if (arrayType->token == ArrayToken)
    {
        if (oType != NULL)
            *oType = Type_AddReference(((VariableTypeArray*)arrayType)->memberType);

        if (indexReadOnly || indexValue.addressType != AddressType_Register)
        {
            *oValue = Value_Register(1);
            // ! We do not copy the literal here as that might be slow,
            // as such we need special handling in ALL code paths
            if (indexValue.addressType != AddressType_Literal)
                Value_GenerateMemCpy(*oValue, indexValue);

            if (!indexReadOnly)
                Value_FreeValue(&indexValue);
        }
        else
            *oValue = indexValue;

        int elementSize = SizeInWords(((VariableTypeArray*)arrayType)->memberType);

        if (elementSize != 1)
        {
            if (indexValue.addressType == AddressType_Literal)
                indexValue.address *= (int32_t)elementSize;
            else
                OutWrite("mul r%i, %i\n", Value_GetR0(oValue), elementSize);
        }

        if (arrayValue.addressType == AddressType_Memory)
        {
            if (indexValue.addressType == AddressType_Literal)
            {
                Value_FreeValue(oValue);

                oValue->addressType = AddressType_Memory;
                oValue->size = elementSize;
                oValue->address = arrayValue.address + indexValue.address;

                // Not needed, for the sake of completeness
                // if (!arrayReadOnly)
                //    Value_FreeValue(&arrayValue);
                *oReadOnly = true;
                Type_RemoveReference(arrayType);
                Type_RemoveReference(indexType);
                return;
            }
            else
            {
                OutWrite("add r%i, %i\n", Value_GetR0(oValue), arrayValue.address);
                *oReadOnly = false;
            }
        }
        else if (arrayValue.addressType == AddressType_MemoryRelative)
        {

            int delta = Stack_GetDelta((int)arrayValue.address);

            if (indexValue.addressType == AddressType_Literal)
            {
                Value_FreeValue(oValue);

                oValue->addressType = AddressType_MemoryRelative;
                oValue->address = arrayValue.address - indexValue.address;
                oValue->size = elementSize;
                *oReadOnly = true;
                Type_RemoveReference(arrayType);
                Type_RemoveReference(indexType);
                return;
            }
            else
            {
                OutWrite("add r%i, sp\n", Value_GetR0(oValue));
                if (delta > 0)
                    OutWrite("sub r%i, %i\n", Value_GetR0(oValue), delta);
                else if (delta < 0)
                    OutWrite("add r%i, %i\n", Value_GetR0(oValue), -delta);
            }
        }
        else if (arrayValue.addressType == AddressType_MemoryRegister)
        {
            if (indexValue.addressType == AddressType_Literal)
            {
                arrayValue.addressType = AddressType_Register;
                arrayValue.size = 1;
                GenerateNativeOP(NativeOP_Add, oValue, indexValue, arrayValue, true, arrayReadOnly);
            }
            else
            {
                arrayValue.addressType = AddressType_Register;
                arrayValue.size = 1;
                GenerateNativeOP(NativeOP_Add, oValue, *oValue, arrayValue, true, arrayReadOnly);
            }
        }

        oValue->addressType = AddressType_MemoryRegister;
        oValue->size = elementSize;
        *oReadOnly = false;
    }
    else
        ErrorAtLocation("Invalid array access!", expr->loc);

    Type_RemoveReference(arrayType);
    Type_RemoveReference(indexType);
}

void CodeGen_LogicalAndOr(const AST_Expression_BinOp* expr, Scope* scope, Value* oValue, VariableType** oType,
                          bool* oReadOnly)
{
    bool leftReadOnly;
    Value left = NullValue;
    CodeGen_Expression(expr->exprA, scope, &left, NULL, &leftReadOnly);
    Value boolean = NullValue;
    if (leftReadOnly)
    {
        boolean = Value_Register(1);
        Value_GenerateMemCpy(boolean, left);
    }
    else if (left.addressType == AddressType_Register)
        boolean = left;
    else
    {
        boolean = Value_Register(1);
        Value_GenerateMemCpy(boolean, left);
        Value_FreeValue(&left);
    }
    Value_ToFlag(&left);
    int id = GetLabelID();

    if (expr->op == BinOp_LogicalAnd)
        OutWrite("jmp_z AND_END_%zi\nnop\n", id);
    else
        OutWrite("jmp_nz OR_END_%zi\nnop\n", id);

    bool rightReadOnly;
    Value right = NullValue;

    CodeGen_Expression(expr->exprB, scope, &right, NULL, &rightReadOnly);
    Value_GenerateMemCpy(boolean, right);

    if (!rightReadOnly)
        Value_FreeValue(&right);

    if (expr->op == BinOp_LogicalAnd)
        OutWrite("AND_END_%u:\n", id);
    else
        OutWrite("OR_END_%u:\n", id);

    *oValue = boolean;
    if (oType != NULL)
        *oType = Type_AddReference(&MachineUIntType);
    *oReadOnly = false;
}

void CodeGen_BinaryOperator(AST_Expression_BinOp* expr, Scope* scope, Value* oValue, VariableType** oType,
                            bool* oReadOnly)
{

    // Some BinOps have special code generators
    if (expr->op >= BinOp_AssignmentAdd && expr->op <= BinOp_Assignment)
    {
        CodeGen_Assignment(expr, scope, oValue, oType, oReadOnly);
        return;
    }
    else if (expr->op == BinOp_StructAccessArrow || expr->op == BinOp_StructAccessDot)
    {
        CodeGen_StructMemberAccess(expr, scope, oValue, oType, oReadOnly);
        return;
    }
    else if (expr->op == BinOp_ArrayAccess)
    {
        CodeGen_ArrayAccess(expr, scope, oValue, oType, oReadOnly);
        return;
    }
    else if (expr->op == BinOp_LogicalAnd || expr->op == BinOp_LogicalOr)
    {
        CodeGen_LogicalAndOr(expr, scope, oValue, oType, oReadOnly);
        return;
    }

    if (oValue == NULL)
        return;

    // Generate code and get values of the two operands
    VariableType* leftType = NULL;
    VariableType* rightType = NULL;
    VariableType* returnType = NULL;

    Value left = NullValue;
    Value right = NullValue;
    bool leftReadOnly = 42;
    bool rightReadOnly = 42;

    int pre = Stack_GetSize();
    CodeGen_Expression(expr->exprA, scope, &left, &leftType, &leftReadOnly);
    int postA = Stack_GetSize();
    CodeGen_Expression(expr->exprB, scope, &right, &rightType, &rightReadOnly);

    // In case the stack has grown since these were calculated
    if (left.addressType == AddressType_MemoryRelative)
        left.address += (int32_t)(Stack_GetSize() - postA);
    if (oValue->addressType == AddressType_MemoryRelative)
        oValue->address += (int32_t)(Stack_GetSize() - pre);

    VariableType* parameterType;

    if (leftType->token != None)
        parameterType = leftType;
    else if (rightType->token != None)
        parameterType = rightType;
    else
        parameterType = &MachineIntType;

    // Comparision operators always return a boolean (aka machine uint) type
    if (expr->op >= BinOp_LessThan && expr->op <= BinOp_NotEqual)
        returnType = &MachineIntType;
    else
        returnType = parameterType;

    if (!IsPrimitiveType(returnType))
        ErrorAtLocation("Invalid binop", expr->loc);

    if (leftType->token != None && rightType->token != None)
    {
        if (!Type_Check(leftType, rightType))
            ErrorAtLocation("Invalid binop", expr->loc);
    }

    if (oType != NULL)
        *oType = Type_AddReference(returnType);

    // If both operands are literals, we can simply calculate the result
    // at compile time and return it without any instructions.

    // TODO fix this for signed/unsigned types
    if (left.addressType == AddressType_Literal && right.addressType == AddressType_Literal)
    {
        int32_t newLiteral;

        switch (expr->op)
        {
            case BinOp_Add:
                newLiteral = left.address + right.address;
                break;
            case BinOp_Sub:
                newLiteral = left.address - right.address;
                break;
            case BinOp_Mul:
                newLiteral = left.address * right.address;
                break;
            case BinOp_Div:
#ifndef CUSTOM_COMP
                newLiteral = left.address / right.address;
#endif
#ifdef CUSTOM_COMP
                newLiteral = (int32_t)(((int)left.address) / ((int)right.address));
#endif
                break;
            case BinOp_Mod:
#ifndef CUSTOM_COMP
                newLiteral = left.address % right.address;
#endif
#ifdef CUSTOM_COMP
                newLiteral = (int32_t)(((int)left.address) % ((int)right.address));
#endif
                break;
            case BinOp_GreaterThan:
                newLiteral = (int32_t)(left.address > right.address);
                break;
            case BinOp_GreaterThanEq:
                newLiteral = (int32_t)(left.address >= right.address);
                break;
            case BinOp_LessThan:
                newLiteral = (int32_t)(left.address < right.address);
                break;
            case BinOp_LessThanEq:
                newLiteral = (int32_t)(left.address <= right.address);
                break;
            case BinOp_LogicalAnd:
            case BinOp_And:
                newLiteral = left.address & right.address;
                break;
            case BinOp_LogicalOr:
            case BinOp_Or:
                newLiteral = left.address | right.address;
                break;
            case BinOp_Xor:
                newLiteral = left.address ^ right.address;
                break;
            case BinOp_ShiftLeft:
                newLiteral = left.address << right.address;
                break;
            case BinOp_ShiftRight:
                newLiteral = left.address >> right.address;
                break;
            default:
                ErrorAtLocation("Invalid binop", expr->loc);
                newLiteral = 0;
        }

        *oValue = Value_Literal(newLiteral);
        *oReadOnly = false;
    }
    else
    {
        // Binops like add, sub, mul, ... on 16-bit types map almost 1:1 to
        // machine instructions This allows us to generalize a lot
        // (fewer explicit case distinctions) and still get optimal code.
        if (SizeInWords(returnType) == 1 && expr->op <= BinOp_ShiftRight &&
            ((expr->op != BinOp_Mul && expr->op != BinOp_Div) || returnType->token != FixedKeyword))
        {
            // If a return value (that is not a flag) has been requested, it owned by whoever requested it
            // therefore readOnly, as we don't want anyone else to free it.
            if (oValue->addressType == AddressType_None || oValue->addressType == AddressType_Flag)
                *oReadOnly = false;
            else
                *oReadOnly = true;
            GenerateNativeOP((NativeOP)expr->op, oValue, left, right, leftReadOnly, rightReadOnly);
        }
        else
        {
            if (oValue->addressType == AddressType_None)
                *oValue = Value_Register(SizeInWords(returnType));

            // Handle Binops that can store their result in flag
            if (expr->op >= BinOp_LessThan && expr->op <= BinOp_GreaterThanEq)
                BinopComparison(expr->op, oValue, &left, &right, leftReadOnly, parameterType);
            else if (expr->op == BinOp_Equal || expr->op == BinOp_NotEqual)
                BinopEquality(expr->op, oValue, &left, &right, leftReadOnly, rightReadOnly);
            else
            {
                if (oValue->addressType == AddressType_Flag)
                    *oValue = Value_Register(SizeInWords(returnType));

                // Handle remaining Binops
                switch (expr->op)
                {
                    // TODO replace switches here with arrays.
                    case BinOp_Add:
                        BinopAdd32(oValue, &left, &right, leftReadOnly, rightReadOnly);
                        break;
                    case BinOp_Sub:
                        BinopSub32(oValue, &left, &right, leftReadOnly);
                        break;
                    case BinOp_Mul:
                        BinopMul32(oValue, &left, &right, leftReadOnly, rightReadOnly, parameterType);
                        break;
                    case BinOp_Div:
                        BinopDiv32(oValue, &left, &right, leftReadOnly, rightReadOnly, parameterType);
                        break;
                    case BinOp_Xor:
                    case BinOp_Or:
                    case BinOp_And:
                        BinopBitwise32(expr->op, oValue, &left, &right);
                        break;
                    case BinOp_Mod:
                        BinopMod(oValue, &left, &right);
                        break;
                    case BinOp_ShiftLeft:
                        BinopShiftLeft32(oValue, &left, &right);
                        break;
                    case BinOp_ShiftRight:
                        BinopShiftRight32(oValue, &left, &right);
                        break;
                    default:
                        ErrorAtLocation("Not implemented", expr->loc);
                }
            }

            // if (!leftReadOnly && (!Value_Equals(oValue, &left) || left.addressType == AddressType_MemoryRegister))
            //     Value_FreeValue(&left);
            // if (!rightReadOnly && (!Value_Equals(oValue, &right) || right.addressType == AddressType_MemoryRegister))
            //     Value_FreeValue(&right);
            if (!leftReadOnly && (!Value_Equals(oValue, &left)))
                Value_FreeValue(&left);
            if (!rightReadOnly && (!Value_Equals(oValue, &right)))
                Value_FreeValue(&right);

            *oReadOnly = false;
        }
    }

    Type_RemoveReference(leftType);
    Type_RemoveReference(rightType);
}
