#pragma once
#include "Token.h"
#include <stdbool.h>

bool Preprocessor_IsValid(const char* id);
bool Preprocessor_IsDefined(const char* id);
void Preprocessor_Define(const char* id);
void Preprocessor_Clear();
void Preprocessor_Undefine(const char* id);
void Preprocessor_End();