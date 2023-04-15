#include "Scope.h"
#include "Function.h"
#include "GenericList.h"
#include "Token.h"
#include "Type.h"
#include "Util.h"
#include "Value.h"
#include "Variables.h"

#include <memory.h>

// These following comparison methods are passed as arguments to GenericList_Find
bool CompareVariableToID(const void* variable, const void* identifier)
{
    Variable* var = (Variable*)variable;
    char* id = (char*)identifier;

    return strcmp(var->name, id) == 0;
}

static bool CompareVariableToValue(const void* variable, const void* value)
{
    Variable* var = (Variable*)variable;
    Value* val = (Value*)value;

    return (var->value.address == val->address && var->value.addressType == val->addressType &&
            var->value.size == val->size);
}

static bool CompareStructToID(const void* variable, const void* identifier)
{
    Struct* s = *((Struct**)variable);
    char* id = (char*)identifier;

    return strcmp(s->identifier, id) == 0;
}

static bool CompareEnumToID(const void* variable, const void* identifier)
{
    Enum* e = (Enum*)variable;
    char* id = (char*)identifier;

    return strcmp(e->identifier, id) == 0;
}

static bool CompareTypedefToID(const void* variable, const void* identifier)
{
    Typedef* td = (Typedef*)variable;
    char* id = (char*)identifier;

    return strcmp(td->name, id) == 0;
}

void Scope_Dispose(Scope* this)
{
    for (size_t i = 0; i < this->variables.count; i++)
    {
        Variable* handle = (Variable*)GenericList_At(&this->variables, i);
        Value_FreeValue(&handle->value);
        Type_RemoveReference(handle->type);
    }

    for (size_t i = 0; i < this->structs.count; i++)
    {
        Struct* handle = *((Struct**)GenericList_At(&this->structs, i));
        for (size_t j = 0; j < handle->members.count; j++)
            Type_RemoveReference(((Variable*)GenericList_At(&handle->members, j))->type);

        GenericList_Dispose(&handle->members);
        free(handle);
    }

    GenericList_Dispose(&this->variables);
    GenericList_Dispose(&this->structs);
    GenericList_Dispose(&this->enums);
    GenericList_Dispose(&this->typedefs);
}

Scope Scope_Create(Scope* parent)
{
    return (Scope){parent,
                   GenericList_Create(sizeof(Variable)),
                   GenericList_Create(sizeof(Struct*)),
                   GenericList_Create(sizeof(Enum)),
                   GenericList_Create(sizeof(Typedef)),
                   {0, 0, 0, 0, 0, 0, 0, 0}};
}

bool Scope_NameIsUsed(Scope* this, char* id)
{
    return GenericList_Find(&this->variables, CompareVariableToID, id) != NULL ||
           GenericList_Find(&this->structs, CompareStructToID, id) != NULL ||
           GenericList_Find(&this->enums, CompareEnumToID, id) != NULL;
}

void Scope_DeleteVariable(Scope* this, Variable* var)
{
    if (GenericList_Contains(&this->variables, var))
    {
        Type_RemoveReference(var->type);
        GenericList_Delete(&this->variables, var);
    }
    else if (this->parent != NULL)
        Scope_DeleteVariable(this->parent, var);
    else
        assert(0);
}

// The AST Optimizer marks variables that need to be deleted
// after a loop (those that are last accessed in a loop).
// We delete them by calling this after a loop.
void Scope_DeleteVariablesAfterLoop(Scope* this, void* loop)
{   
    do
    {
        for (size_t i = 0; i < this->variables.count; i++)
        {
            Variable* var = GenericList_At(&this->variables, i);
            if (var->lastAccess == loop)
            {
                Value_FreeValue(&var->value);
                Type_RemoveReference(var->type);
                GenericList_Delete(&this->variables, var);
            }
        }
    } while ((this = this->parent) != NULL);
}

Variable* Scope_FindVariable(Scope* this, char* id)
{
    Variable* retval = GenericList_Find(&this->variables, CompareVariableToID, id);

    if (retval == NULL && this->parent != NULL)
        return Scope_FindVariable(this->parent, id);

    return retval;
}

Variable* Scope_FindVariableByValue(Scope* this, Value* val)
{
    Variable* retval = GenericList_Find(&this->variables, CompareVariableToValue, val);

    if (retval == NULL && this->parent != NULL)
        return Scope_FindVariableByValue(this->parent, val);

    return retval;
}

Struct* Scope_FindStruct(Scope* this, char* id)
{
    Struct** retval = GenericList_Find(&this->structs, CompareStructToID, id);

    if (retval == NULL && this->parent != NULL)
        return Scope_FindStruct(this->parent, id);
    if (retval == NULL)
        return NULL;
    return *retval;
}

Enum* Scope_FindEnum(Scope* this, char* id)
{
    Enum* retval = GenericList_Find(&this->enums, CompareEnumToID, id);

    if (retval == NULL && this->parent != NULL)
        return Scope_FindEnum(this->parent, id);

    return retval;
}

Typedef* Scope_FindTypedef(Scope* this, char* id)
{
    Typedef* retval = GenericList_Find(&this->typedefs, CompareTypedefToID, id);

    if (retval == NULL && this->parent != NULL)
        return Scope_FindTypedef(this->parent, id);

    return retval;
}


void Scope_AddVariable(Scope* this, Variable var)
{
    GenericList_Append(&this->variables, &var);
}

void Scope_AddStruct(Scope* this, Struct* s)
{
    GenericList_Append(&this->structs, &s);
}

void Scope_AddEnum(Scope* this, Enum e)
{
    GenericList_Append(&this->enums, &e);
}

void Scope_AddTypedef (Scope* this, Typedef t)
{
    GenericList_Append(&this->typedefs, &t);
}