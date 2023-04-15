#include "Function.h"
#include "GenericList.h"
#include "Token.h"
#include "Type.h"
#include "Variables.h"

static GenericList functions;
static Function* curFunction = NULL;

Function* Function_GetCurrent()
{
    return curFunction;
}
void Function_SetCurrent(Function* f)
{
    curFunction = f;
}

void Function_InitFunctions()
{
    functions = GenericList_Create(sizeof(Function));
}

void Function_DeleteFunctions()
{
    for (size_t i = 0; i < functions.count; i++)
    {
        Function* handle = GenericList_At(&functions, i);
        for (size_t j = 0; j < handle->parameters.count; j++)
        {

            Variable* param = GenericList_At(&handle->parameters, j);

            assert(param->type->refCount == 1);
            assert(Type_RemoveReference(param->type));
        }
        GenericList_Dispose(&handle->parameters);
        Type_RemoveReference(handle->returnType);
    }

    GenericList_Dispose(&functions);
}

void* Function_Add(Function f)
{
    return GenericList_Append(&functions, &f);
}

static bool CompareFunctionToID(const void* variable, const void* identifier)
{
    Function* f = (Function*)variable;
    char* id = (char*)identifier;

    return strcmp(f->identifier, id) == 0;
}

Function* Function_Find(char* identifier)
{
    return GenericList_Find(&functions, CompareFunctionToID, identifier);
}

Function Function_Create(char* identifier, VariableType* returnType)
{
    Function f;
    f.identifier = identifier;
    f.returnType = Type_AddReference(returnType);
    f.parameters = GenericList_Create(sizeof(Variable));

    return f;
}

void Function_Dispose(Function* this)
{
    /*for (size_t i = 0; i < this->parameters.count; i++)
    {
        Variable* v = GenericList_At(&this->parameters, i);
        Type_RemoveReference(v->type);
    }
    GenericList_Dispose(&this->parameters);

    free(this);*/
}