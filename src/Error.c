#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "Token.h"

typedef enum
{
    TermColor_Black,
    TermColor_Red,
    TermColor_Green,
    TermColor_Yellow,
    TermColor_Blue,
    TermColor_Magenta,
    TermColor_Cyan,
    TermColor_White
} TermColor;

// Minimal Terminal escape sequence for color/bold
// See here https://chrisyeh96.github.io/2020/03/28/terminal-colors.html
void SetTerminalStyle(TermColor color, bool bold)
{
#ifndef CUSTOM_COMP
    if (bold)
        printf("\x1b[1;%im", 30 + (int)color);
    else
        printf("\x1b[%im", 30 + (int)color);
#endif
}

void ResetTerminalStyle()
{
#ifndef CUSTOM_COMP
    printf("\x1b[0m");
#endif
}

void ErrorAtLine(const char* error, int lineNumber)
{
    SetTerminalStyle(TermColor_White, true);
    printf("%i: ", lineNumber);
    SetTerminalStyle(TermColor_Red, true);
    printf("error:");
    SetTerminalStyle(TermColor_White, true);
    printf(" %s\n", error);
    ResetTerminalStyle();
    exit(1);
}
void ErrorAtLineInFile(const char* error, int lineNumber, const char* fileName)
{
    SetTerminalStyle(TermColor_White, true);
    printf("%s:%i: ", fileName, lineNumber);
    SetTerminalStyle(TermColor_Red, true);
    printf("error:");
    SetTerminalStyle(TermColor_White, true);
    printf(" %s\n", error);
    ResetTerminalStyle();
    exit(1);
}
void Error(const char* error)
{
    SetTerminalStyle(TermColor_Red, true);
    printf("error:");
    SetTerminalStyle(TermColor_White, true);
    printf(" %s\n", error);
    ResetTerminalStyle();
    exit(1);
}
void ErrorAtLocation(const char* error, SourceLocation location)
{
    SetTerminalStyle(TermColor_White, true);
    printf("%s:%i: ", location.sourceFile, location.lineNumber);
    SetTerminalStyle(TermColor_Red, true);
    printf("error:");
    SetTerminalStyle(TermColor_White, true);
    printf(" %s\n", error);
    ResetTerminalStyle();
    exit(1);
}
void ErrorAtIndex(const char* error, size_t tokenIndex)
{
    SourceLocation loc = Token_GetLocation(tokenIndex);
    ErrorAtLocation(error, loc);
}
void ErrorAtToken(const char* error, Token* token)
{
    SourceLocation loc = Token_GetLocationP(token);
    ErrorAtLocation(error, loc);
}
void SyntaxErrorAtIndex(size_t tokenIndex)
{
    SourceLocation loc = Token_GetLocation(tokenIndex);
    ErrorAtLocation("Syntax error\n", loc);
}
void SyntaxErrorAtToken(Token* token)
{
    SourceLocation loc = Token_GetLocationP(token);
    ErrorAtLocation("Syntax error\n", loc);
}
void SyntaxError()
{
    Error("Syntax error\n");
}
