#include "P_Type.h"
#include "../Scope.h"
#include "../Function.h"
#include "../Util.h"

VariableType* ParseVariableType(Token* tokens, size_t* i, size_t maxLen, Scope* scope, char** identifier,
                                bool allowVoid)
{
    Token token;
    Qualifiers qualifiers = Qualifier_None;
    void* structure = NULL;
    int pointerLevel = 0;

    while (tokens[*i].type == ConstKeyword || tokens[*i].type == RegisterKeyword || tokens[*i].type == StaticKeyword)
    {
        switch (tokens[*i].type)
        {
            case ConstKeyword:
                qualifiers |= Qualifier_Const;
                break;
            case RegisterKeyword:
                qualifiers |= Qualifier_Register;
                break;
            case StaticKeyword:
                qualifiers |= Qualifier_Static;
                break;
            default:;
        }
        Inc(i);
    }

    token = tokens[(*i)];
    Inc(i);

    // TODO: Maybe filter out all invalid tokens here with a switch?
    // if (varType.token == StructKeyword || varType.token == EnumKeyword)
    //   SyntaxErrorAtIndex(*i);
    if (!IsDataToken(token, scope) && !(allowVoid && token.type == VoidKeyword))
        return NULL;

    if (token.type == Identifier)
    {
        if ((structure = Scope_FindStruct(scope, token.data)) != NULL)
            token.type = StructKeyword;
        else if (Scope_FindEnum(scope, token.data) != NULL)
            token.type = IntKeyword;
        else
            return NULL;
    }

    // Count stars after variable type to see if and what level of pointer it is.
    while (tokens[(*i)].type == Star)
    {
        pointerLevel++;
        Inc(i);
    }

    if (pointerLevel > 0)
        while (tokens[*i].type == ConstKeyword || tokens[*i].type == RestrictKeyword)
        {
            if (tokens[*i].type == ConstKeyword)
                qualifiers |= Qualifier_ConstPointer;
            else
                qualifiers |= Qualifier_Restrict;
            Inc(i);
        }

    // Function Pointer Type
    if ((*i + 1) < maxLen && tokens[*i].type == RBrOpen && tokens[*i + 1].type == Star)
    {
        (*i)++;
        Inc(i);

        if (identifier != NULL)
        {
            if (tokens[*i].type != Identifier)
                SyntaxErrorAtToken(&tokens[*i]);
            *identifier = tokens[(*i)++].data;
        }

        if (tokens[(*i)].type != RBrClose || ((*i + 1 < maxLen) && tokens[(*i) + 1].type != RBrOpen))
            SyntaxErrorAtToken(&tokens[*i]);

        Inc(i);
        Inc(i);

        GenericList parameters = GenericList_Create(sizeof(Variable));

        Function* fpointer = xmalloc(sizeof(Function));
        fpointer->variadicArguments = false;

        int index = 2;
        // TODO count brackets everywhere
        while (tokens[*i].type != RBrClose)
        {
            if (tokens[*i].type == DotDotDot)
            {
                fpointer->variadicArguments = true;
                PopNext(i, RBrClose);
                break;
            }
            VariableType* paramType = ParseVariableType(tokens, i, maxLen, scope, NULL, false);
            int size = SizeInWords(paramType);
            if (size == -1)
                ErrorAtIndex("Invalid type", *i);
            Variable v = (Variable){paramType, "", Value_MemoryRelative(index, size), (void*)NULL};

            GenericList_Append(&parameters, &v);

            if (tokens[*i].type == Comma)
            {
                Inc(i);
                if (tokens[*i].type == RBrClose)
                    SyntaxErrorAtToken(&tokens[*i]);
            }
            index += size;
        }
        Inc(i);

        fpointer->identifier = "";
        fpointer->parameters = parameters;
        VariableType* retType = xmalloc(sizeof(VariableType));
        retType->pointerLevel = pointerLevel;
        retType->qualifiers = qualifiers;
        retType->refCount = 1;
        retType->structure = structure;
        retType->token = token.type;
        fpointer->returnType = retType;

        // We have to assume the function called using a function pointer
        // modifies all registers.
        fpointer->modifiedRegisters = 0xFFFF;

        VariableType* varType = xmalloc(sizeof(VariableType));
        varType->token = FunctionPointerToken;
        varType->pointerLevel = 0;
        varType->structure = fpointer;
        varType->refCount = 1;
        varType->qualifiers = Qualifier_None;

        return varType;
    }

    if (token.type == VoidKeyword && pointerLevel == 0 && !(allowVoid))
        return NULL;

    if (identifier != NULL)
    {
        if (tokens[*i].type != Identifier)
            ErrorAtIndex("Expected identifier!", *i);
        *identifier = tokens[(*i)++].data;
    }

    VariableType* varType = xmalloc(sizeof(VariableType));
    varType->token = token.type;
    varType->pointerLevel = pointerLevel;
    varType->structure = structure;
    varType->refCount = 1;
    varType->qualifiers = qualifiers;

    // Array
    if (tokens[*i].type == ABrOpen)
    {
        Inc(i);

        int elemCount = -1;
        if (tokens[(*i)].type == IntLiteral)
        {
            elemCount = (int)(*(int32_t*)tokens[*i].data);
            Inc(i);
        }
        PopCur(i, ABrClose);

        varType->qualifiers = Qualifier_None;

        VariableTypeArray* array = xmalloc(sizeof(VariableTypeArray));
        array->token = ArrayToken;
        array->memberCount = elemCount;
        array->memberType = varType;
        array->refCount = 1;
        array->pointerLevel = 0;
        array->qualifiers = qualifiers;
        array->structure = NULL;

        return (VariableType*)array;
    }

    return varType;
}