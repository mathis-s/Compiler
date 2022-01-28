#pragma once
#include "../Type.h"

VariableType* ParseVariableType(Token* tokens, size_t* i, size_t maxLen, Scope* scope, char** identifier,
                                bool allowVoid);