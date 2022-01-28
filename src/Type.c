#include "Type.h"
#include "Function.h"
#include "GenericList.h"
#include "Scope.h"
#include "Struct.h"
#include "Token.h"
#include "Util.h"
#include "Variables.h"
#include <assert.h>

int SizeInWords(const VariableType* type)
{
    if (type->pointerLevel > 0)
        return 1;

    assert(type->token != Identifier);

    if (type->token == StructKeyword)
    {
        assert(type->structure != NULL);
        return ((Struct*)type->structure)->sizeInWords;
    }

    if (type->token == ArrayToken)
    {
        const VariableTypeArray* arr = (VariableTypeArray*)type;
        int size = arr->memberCount * SizeInWords(arr->memberType);
        if (size < 0)
            return -1;
        return size;
    }

    switch (type->token)
    {
        case VoidKeyword:
            return 0;
        case IntKeyword:
        case UintKeyword:
        case FixedKeyword:
        case FunctionPointerToken:
            return 1;

        case Int32Keyword:
        case Uint32Keyword:
            return 2;

        default:
            return -1;
    }
}

bool IsDataToken(Token token, Scope* scope)
{
    switch (token.type)
    {
        case StaticKeyword:
        case RegisterKeyword:
        case ConstKeyword:
        case IntKeyword:
        case Int32Keyword:
        case UintKeyword:
        case Uint32Keyword:
        case FixedKeyword:
        case VoidKeyword:
            return true;

        case Identifier:
            if (scope == NULL)
                return true;
            return Scope_FindStruct(scope, token.data) != NULL || Scope_FindEnum(scope, token.data) != NULL;

        default:
            return false;
    }
}

bool IsDataType(const VariableType* type)
{
    switch (type->token)
    {
        case IntKeyword:
        case Int32Keyword:
        case UintKeyword:
        case Uint32Keyword:
        case FixedKeyword:
            return true;

        case ArrayToken:
            return true;
        case StructKeyword:
            return true;
        case FunctionPointerToken:
            return true;
        case VoidKeyword:
            return type->pointerLevel > 0;

        default:
            return false;
    }
}

bool IsPrimitiveType(const VariableType* type)
{
    if (type->pointerLevel > 0 || type->token == FunctionPointerToken)
        return true;
    if (type->token == ArrayToken)
        return false;
    if (type->token == StructKeyword)
        return false;
    assert(type->token != Identifier);
    return true;
}

bool Type_Check(const VariableType* a, const VariableType* b)
{
    if (a->token == None || b->token == None)
        return true;
    if (SizeInWords(a) != SizeInWords(b))
        return false;

    // Implicit conversion between all pointers
    if (a->pointerLevel > 0 || b->pointerLevel > 0)
        return true;

    // if ((a->structure == NULL) != (b->structure == NULL)) return false;

    // Implicit conversion between int and uint
    if ((a->token == IntKeyword && b->token == UintKeyword) || (a->token == UintKeyword && b->token == IntKeyword) ||
        (a->token == Uint32Keyword && b->token == Int32Keyword) ||
        (a->token == Int32Keyword && b->token == Uint32Keyword))
        return true;

    return a->token == b->token;
}
VariableType* Type_AddReference(VariableType* type)
{
    type->refCount++;
    return type;
}
VariableType* Type_Copy(VariableType* type)
{

    VariableType* retval;
    switch (type->token)
    {
        default:
            retval = xmalloc(sizeof(VariableType));
            *retval = *type;
            break;
        case ArrayToken:
            retval = xmalloc(sizeof(VariableTypeArray));
            *(VariableTypeArray*)retval = *(VariableTypeArray*)type;
    }

    retval->refCount = 1;
    type->refCount--;

    // The copy also references to any underlying types
    if (type->token == ArrayToken)
        Type_AddReference(((VariableTypeArray*)type)->memberType);

    if (type->refCount == 0)
    {
        if (type->token == ArrayToken)
            Type_RemoveReference(((VariableTypeArray*)(type))->memberType);
        else if (type->token == FunctionPointerToken)
        {
            Function* func = ((Function*)(type->structure));
            Type_RemoveReference(func->returnType);
            for (size_t i = 0; i < func->parameters.count; i++)
            {
                Variable* var = GenericList_At(&func->parameters, i);
                Type_RemoveReference(var->type);
            }
            GenericList_Dispose(&func->parameters);
            free(func);
        }
        free(type);
    }
    return retval;
}
bool Type_RemoveReference(VariableType* type)
{
    type->refCount--;
    if (type->refCount == 0)
    {
        if (type->token == ArrayToken)
            Type_RemoveReference(((VariableTypeArray*)(type))->memberType);

        else if (type->token == FunctionPointerToken)
        {
            Function* func = ((Function*)(type->structure));
            Type_RemoveReference(func->returnType);
            for (size_t i = 0; i < func->parameters.count; i++)
            {
                Variable* var = GenericList_At(&func->parameters, i);
                Type_RemoveReference(var->type);
            }
            GenericList_Dispose(&func->parameters);
            free(func);
        }
        free(type);
        return true;
    }
    return false;
}

