#include "P_Type.h"
#include "../Function.h"
#include "../Scope.h"
#include "../Util.h"
#include <malloc.h>
#include <signal.h>
#include <stddef.h>

void* P_Type_PopNext(Token* tokens, const size_t maxLen, size_t* const i, TokenType type)
{
    if (++(*i) > maxLen)
        SyntaxErrorAtToken(&tokens[*i - 1]);
    if (tokens[*i].type != type)
        SyntaxErrorAtToken(&tokens[*i]);

    return tokens[*i].data;
}

void* P_Type_PopNextInc(Token* tokens, const size_t maxLen, size_t* const i, TokenType type)
{
    if (++(*i) > maxLen)
        SyntaxErrorAtToken(&tokens[*i - 1]);
    if (tokens[*i].type != type)
        SyntaxErrorAtToken(&tokens[*i]);

    void* retval = tokens[*i].data;
    if (++(*i) > maxLen)
        SyntaxErrorAtToken(&tokens[*i - 1]);
    return retval;
}

void* P_Type_PopCur(Token* tokens, const size_t maxLen, size_t* const i, TokenType type)
{
    if (tokens[*i].type != type)
        SyntaxErrorAtToken(&tokens[*i]);
    void* retval = tokens[*i].data;
    if (++(*i) > maxLen)
        SyntaxErrorAtToken(&tokens[*i - 1]);
    return retval;
}

void P_Type_Inc(Token* tokens, const size_t maxLen, size_t* const i)
{
    if (++(*i) > maxLen)
        SyntaxErrorAtToken(&tokens[*i - 1]);
}

VariableType* ParseVariableType(Token* tokens, size_t* i, size_t maxLen, Scope* scope, char** identifier,
                                bool allowVoid)
{
    // Parse initial Qualifiers
    Qualifiers qualifiers = Qualifier_None;
    while (tokens[*i].type == ConstKeyword || tokens[*i].type == RegisterKeyword || tokens[*i].type == StaticKeyword)
    {
        switch (tokens[*i].type)
        {
            case ConstKeyword: qualifiers |= Qualifier_Const; break;
            case RegisterKeyword: qualifiers |= Qualifier_Register; break;
            case StaticKeyword: qualifiers |= Qualifier_Static; break;
            default:;
        }
        P_Type_Inc(tokens, maxLen, i);
    }

    // Parse Base Type: primitive, identifier, struct/enum/union or function pointer
    VariableType* vtype = NULL;

    Token token = tokens[(*i)];

    switch (token.type)
    {
        case IntKeyword:
        case UintKeyword:
        case Int32Keyword:
        case Uint32Keyword:
        case FixedKeyword:
        case VoidKeyword:
            vtype = xmalloc(sizeof(VariableType));
            vtype->token = token.type;
            vtype->refCount = 1;
            vtype->qualifiers = qualifiers;
            // vtype->pointerLevel = 0;
            P_Type_Inc(tokens, maxLen, i);
            break;

        case Identifier:
        {
            Typedef* td;
            if ((td = Scope_FindTypedef(scope, token.data)))
            {
                // TODO: Copy isn't really necessary when not modifying type
                vtype = Type_Copy(td->type);
                vtype->qualifiers |= qualifiers;
                P_Type_Inc(tokens, maxLen, i);
            }
            else
                SyntaxErrorAtToken(&tokens[*i]);

            break;
        }

        case UnionKeyword:
        case StructKeyword:
        {
            bool isUnion = tokens[*i].type == UnionKeyword;
            P_Type_Inc(tokens, maxLen, i);
            VariableTypeStruct* structType = xmalloc(sizeof(VariableTypeStruct));
            structType->token = StructKeyword;
            structType->qualifiers = Qualifier_None;
            structType->refCount = 1;

            char* id = NULL;
            if (tokens[*i].type == Identifier)
            {
                id = tokens[*i].data;
                P_Type_Inc(tokens, maxLen, i);
            }

            if (tokens[*i].type == CBrOpen)
            {
                bool newAlloc = false;

                Struct* struc;
                if (id != NULL && (struc = Scope_FindStruct(scope, id)))
                {
                    // FIXME: this should only work if old is an incomplete def AND only with structs in current scope
                    if (struc->sizeInWords != 0)
                        ErrorAtToken("struct redefinition", &tokens[*i]);
                }
                else
                {
                    struc = xmalloc(sizeof(Struct));
                    newAlloc = true;
                }

                struc->members = GenericList_Create(sizeof(Variable));
                struc->sizeInWords = 0;
                struc->identifier = id;
                structType->str = struc;

                P_Type_Inc(tokens, maxLen, i);

                // TODO: Accept whatever nonsense you can have in a struct decl in C (multiple semicolons, empty defs,
                // ...)
                // TODO: Add anonymous inner structs and unions
                while (tokens[*i].type != CBrClose)
                {
                    char* id = NULL;
                    VariableType* memType = ParseVariableType(tokens, i, maxLen, scope, &id, false);
                    int size = SizeInWords(memType);

                    Variable v = (Variable){
                        memType, id, {(int32_t)(isUnion ? 0 : struc->sizeInWords), AddressType_StructMember, size}, NULL};
                    GenericList_Append(&struc->members, &v);

                    if (!isUnion)
                        struc->sizeInWords += SizeInWords(memType);
                    else if (size > struc->sizeInWords)
                        struc->sizeInWords = size;

                    P_Type_PopCur(tokens, maxLen, i, Semicolon);
                }

                P_Type_Inc(tokens, maxLen, i);

                // TODO: Add global list for union names
                if (id != NULL && !isUnion && newAlloc)
                {
                    Scope_AddStruct(scope, struc);
                }
            }
            else
            {
                if (isUnion)
                {
                    ErrorAtToken("not implemented", &tokens[*i]);
                }
                else
                {
                    if (id == NULL)
                        SyntaxErrorAtToken(&tokens[*i]);
                    structType->str = Scope_FindStruct(scope, id);
                    if (structType->str == NULL)
                        ErrorAtToken("undefined", &tokens[*i]);
                }
            }

            vtype = (VariableType*)structType;
            break;
        }

        case EnumKeyword:
        {
            P_Type_Inc(tokens, maxLen, i);

            char* id = NULL;
            if (tokens[*i].type == Identifier)
            {
                id = tokens[*i].data;
                P_Type_Inc(tokens, maxLen, i);
            }

            uint32_t currentId = 0;

            if (tokens[*i].type == CBrOpen)
            {
                P_Type_Inc(tokens, maxLen, i);

                while (tokens[*i].type != CBrClose)
                {
                    if (tokens[(*i)].type != Identifier)
                        SyntaxErrorAtToken(&tokens[*i]);
                    char* label = tokens[*i].data;
                    P_Type_Inc(tokens, maxLen, i);

                    if (tokens[(*i)].type == Assignment)
                        currentId = (*((uint32_t*)P_Type_PopNextInc(tokens, maxLen, i, IntLiteral)));

                    Scope_AddVariable(scope, (Variable){Type_AddReference(&MachineUIntType), label,
                                                        Value_Literal((currentId++)), NULL});

                    if (tokens[(*i)].type == Comma)
                        P_Type_Inc(tokens, maxLen, i);
                    else if (tokens[(*i)].type != CBrClose)
                        SyntaxErrorAtToken(&tokens[*i]);
                }
                P_Type_Inc(tokens, maxLen, i);

                if (id != NULL)
                    Scope_AddEnum(scope, (Enum){id});
            }
            else
            {
                if (!Scope_FindEnum(scope, id))
                    ErrorAtToken("undefined", &tokens[*i - 1]);
            }
        }

        default: break;
    }

    if (vtype == NULL)
        vtype = Type_AddReference(&MachineIntType);

    ssize_t next = -1;
    ssize_t last = -1;
    bool identParsed = false;
    while (true)
    {
        switch (tokens[*i].type)
        {
            case Identifier:
                if (identifier)
                    *identifier = tokens[*i].data;
                else
                    break;
                P_Type_Inc(tokens, maxLen, i);
                identParsed = true;
                continue;

            case Star:
            {
                // vtype->pointerLevel++;
                VariableTypePtr* ptrType = xmalloc(sizeof(VariableTypePtr));
                ptrType->refCount = 1;
                ptrType->token = PointerToken;
                ptrType->baseType = vtype;
                ptrType->qualifiers = Qualifier_None;
                vtype = (VariableType*)ptrType;

                P_Type_Inc(tokens, maxLen, i);
                continue;
            }

            case ABrOpen:
            {
                int32_t* size = P_Type_PopNext(tokens, maxLen, i, IntLiteral);

                VariableTypeArray* atype = xmalloc(sizeof(VariableTypeArray));
                atype->memberType = vtype;
                atype->memberCount = (int)*size;
                atype->refCount = 1;
                atype->qualifiers = Qualifier_None;
                atype->token = ArrayToken;

                vtype = (VariableType*)atype;

                P_Type_PopNextInc(tokens, maxLen, i, ABrClose);
                continue;
            }

            case RBrOpen:
            {
                if (next == -1 && !identParsed)
                {
                    P_Type_Inc(tokens, maxLen, i);
                    next = *i;
                    int brLevel = 1;
                    while (brLevel != 0)
                    {
                        TokenType type = tokens[*i].type;
                        if (type == RBrOpen)
                            brLevel++;
                        if (type == RBrClose)
                            brLevel--;
                        P_Type_Inc(tokens, maxLen, i);
                    }
                    continue;
                }
                else
                {
                    P_Type_Inc(tokens, maxLen, i);
                    // Function
                    VariableTypeFunctionPointer* ftype = xmalloc(sizeof(VariableTypeFunctionPointer));
                    ftype->refCount = 1;
                    ftype->token = FunctionPointerToken;
                    ftype->qualifiers = Qualifier_None;

                    ftype->func.identifier = "";
                    ftype->func.isForwardDecl = false;
                    ftype->func.modifiedRegisters = 0xFFFF;
                    ftype->func.returnType = vtype;
                    ftype->func.variadicArguments = false;
                    GenericList parameters = GenericList_Create(sizeof(Variable));

                    int index = 1;
                    while (true)
                    {
                        if (IsDataToken(tokens[*i], scope))
                        {
                            char* id = NULL;

                            VariableType* vt = ParseVariableType(tokens, i, maxLen, scope, &id, false);
                            int size = 0;
                            if (vt == NULL || (size = SizeInWords(vt)) == -1)
                                ErrorAtIndex("Invalid type", *i);

                            index += size;
                            Variable var =
                                (Variable){vt, id, (Value){(int32_t)index, AddressType_MemoryRelative, size}, NULL};
                            GenericList_Append(&parameters, &var);
                        }
                        else if (tokens[*i].type == DotDotDot)
                        {
                            ftype->func.variadicArguments = true;
                            P_Type_PopNext(tokens, maxLen, i, RBrClose);
                            break;
                        }
                        else if (tokens[*i].type == Comma)
                        {
                            P_Type_Inc(tokens, maxLen, i);
                            continue;
                        }
                        else if (tokens[*i].type == RBrClose)
                            break;
                        else
                            ErrorAtIndex("Invalid parameter type", *i);
                    }
                    P_Type_PopCur(tokens, maxLen, i, RBrClose);

                    ftype->func.parameters = parameters;
                    vtype = (VariableType*)ftype;
                    continue;
                }
            }

            case ConstKeyword:
                vtype->qualifiers |= Qualifier_Const;
                P_Type_Inc(tokens, maxLen, i);
                continue;
            case RestrictKeyword:
                vtype->qualifiers |= Qualifier_Restrict;
                P_Type_Inc(tokens, maxLen, i);
                continue;

            default: break;
        }

        if (next != -1)
        {
            if (last == -1)
                last = *i;
            *i = next;
            next = -1;
        }
        else
            break;
    }

    if (last != -1)
        *i = last;

    if (vtype->token == VoidKeyword && !allowVoid)
    {
        Type_RemoveReference(vtype);
        return NULL;
    }

    return vtype;
}