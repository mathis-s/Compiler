#pragma once
#include "Token.h"

void ErrorAtLine(const char* error, int lineNumber);
void ErrorAtLineInFile(const char* error, int lineNumber, const char* fileName);
void Error(const char* error);
void SyntaxError();
void ErrorAtLocation(const char* error, SourceLocation location);
void ErrorAtIndex(const char* error, size_t tokenIndex);
void SyntaxErrorAtIndex(size_t tokenIndex);
void ErrorAtToken(const char* error, Token* token);
void SyntaxErrorAtToken(Token* token);