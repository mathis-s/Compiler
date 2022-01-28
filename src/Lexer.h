#pragma once
#include "Token.h"

char* readFileAsString(char* path, size_t* size);
void LexIntoArray(char* code, size_t length, TokenArray* t, char* sourceFileName);
TokenArray* Lex(char* sourceFilePath);
