#include "Variables.h"
#include "Function.h"
#include "GenericList.h"
#include "Outfile.h"
#include "Scope.h"
#include "Struct.h"
#include "Token.h"
#include "Type.h"
#include "Value.h"

void ShiftAddressSpace(Scope* scope, int offset)
{
    for (size_t i = 0; i < scope->variables.count; i++)
    {
        Variable* handle = (Variable*)GenericList_At(&scope->variables, i);

        if (handle->value.addressType == AddressType_MemoryRelative)
            handle->value.address += (int32_t)offset;
    }

    if (scope->parent != NULL)
        ShiftAddressSpace(scope->parent, offset);
}

void ShiftAddressSpaceLocal(GenericList* variables, int offset)
{
    for (size_t i = 0; i < variables->count; i++)
    {
        Variable* handle = (Variable*)GenericList_At(variables, i);

        if (handle->value.addressType == AddressType_MemoryRelative)
            handle->value.address += (int32_t)offset;
    }
}

Value GetValueOnStack(int size, Scope* scope)
{
    ShiftAddressSpace(scope, size);
    Stack_Offset(-size);
    Stack_OffsetSize(size);
    return Value_MemoryRelative(size, size);
}
