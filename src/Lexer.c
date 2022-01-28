#include "Lexer.h"
#include "Error.h"
#include "Lexer_generated.h"
#include "Preprocessor.h"
#include "Token.h"
#include "Util.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* readFileAsString(char* path, size_t* size)
{
    FILE* f = fopen(path, "rb");
    if (f == NULL)
    {
        size = 0;
        return NULL;
    }

    fseek(f, (long)0, SEEK_END);
    size_t fsize = (size_t)ftell(f);
    fseek(f, (long)0, SEEK_SET);

    char* code = xmalloc(fsize + 1);
    fread(code, 1, fsize, f);
    fclose(f);
    code[fsize] = 0; // Null terminate

    *size = fsize;

    return code;
}

/*
static void PrintTokenArray(TokenArray* array)
{
    for (int i = 0; i < array->curLength; i++)
    {
        printf("Index: %i, Type: %i;", i, array->tokens[i].type);

        if (array->tokens[i].data != NULL)
        {
            printf(" Literal: ");
            switch (array->tokens[i].type)
            {
            case Identifier:
                printf("\"%s\"", (char*)array->tokens[i].data);
                break;

            case IntLiteral:
                printf("%li", *((long*)array->tokens[i].data));
                break;
            default:
                break;
            }
        }
        putc('\n', stdout);
    }
}*/

static bool StringEquals(char* a, const char* b)
{
    size_t i = 0;

    while (b[i] != 0)
    {
        if (a[i] != b[i])
            return false;
        ++i;
    }

    return true;
}

bool IsNonIDChar(char c)
{
    return !((isalpha(c) || c == '_') || ((isdigit(c))));
}

static bool IsValidIDChar(char c, bool firstChar)
{
    return (isalpha(c) || c == '_') || (!firstChar && (isdigit(c)));
}

static size_t ParseStringStrict(size_t streamLength, char* stream, const char* token)
{
    size_t len = strlen(token);
    if (streamLength >= len && StringEquals(stream, token) &&
        (streamLength == len || !IsValidIDChar(stream[len], false)))
        return len;
    return SIZE_MAX;
}

static size_t ParseString(size_t streamLength, char* stream, const char* token)
{
    size_t len = strlen(token);
    if (streamLength >= len && StringEquals(stream, token))
        return len;
    return SIZE_MAX;
}

static char buffer[256];

static bool ParseStringLiteral(TokenArray* t, size_t* i, size_t length, int lineNumber, char* code,
                               char* sourceFileName)
{
    size_t bufferIndex = 0;

    if (code[*i] != '\"')
        return false;

    size_t j;
    for (j = (*i) + 1; j < length; j++)
    {
        if (bufferIndex > 256 - 1)
            ErrorAtLine("String too long!", lineNumber);
        if (code[j] == '\\')
        {
            if (j == length - 1)
                ErrorAtLine("Unterminated escape char!", lineNumber);
            j++;

            // Just basic escape sequences
            if (code[j] == '\\')
                buffer[bufferIndex++] = '\\';
            else if (code[j] == 'n')
                buffer[bufferIndex++] = '\n';
            else if (code[j] == 'r')
                buffer[bufferIndex++] = '\r';
            else if (code[j] == '0')
                buffer[bufferIndex++] = 0;
            else if (code[j] == '\'')
                buffer[bufferIndex++] = '\'';
            else if (code[j] == '\"')
                buffer[bufferIndex++] = '\"';
            continue;
        }
        if (code[j] == '\"')
            break;
        if (code[j] == '\n')
            ErrorAtLine("Unterminated string literal!", lineNumber);
        buffer[bufferIndex++] = code[j];
    }

    char* stringLiteral = xmalloc(bufferIndex + 1);
    memcpy(stringLiteral, &buffer[0], bufferIndex);
    stringLiteral[bufferIndex] = 0; // Null terminate
    Token_AppendArray(Token_GetString(StringLiteral, stringLiteral), t, lineNumber, sourceFileName);
    *i = j + 1;
    return true;
}

static bool ParseIdentifier(TokenArray* t, size_t* i, size_t length, int lineNumber, char* code, char* sourceFileName)
{
    size_t bufferIndex = 0;

    size_t j;
    for (j = *i; j < length; j++)
    {
        if (bufferIndex > 256 - 1)
            ErrorAtLineInFile("Identifier too long!", lineNumber, sourceFileName);

        if (IsValidIDChar(code[j], j == *i))
            buffer[bufferIndex++] = code[j];
        else
            break;
    }
    if (bufferIndex > 0)
    {
        char* stringLiteral = xmalloc(bufferIndex + 1);
        if (stringLiteral == NULL)
            Error("Out of memory!");
        memcpy(stringLiteral, &buffer[0], bufferIndex);
        stringLiteral[bufferIndex] = 0; // Null terminate
        Token_AppendArray(Token_GetString(Identifier, stringLiteral), t, lineNumber, sourceFileName);
        *i = j;
        return true;
    }
    return false;
}

static bool ParseIntLiteral(TokenArray* t, size_t* i, int lineNumber, char* code, char* sourceFileName)
{
    errno = 0;
    char* end;

    int32_t literal = strtol((code + *i), &end, 0);
    if ((code + *i) == end)
        return false;
    if (errno == ERANGE)
        return false;
    *i += end - (code + (*i)); // Skip over parsed literal with i

    int32_t* intPtr = xmalloc(sizeof(int32_t));

    // Parsing a fractional literal.
    // These also get turned into int literals as
    // all fractional math is fixed point.
    if (code[*i] == '.')
    {

        (*i)++;
        int32_t literalFrac = strtol((code + *i), &end, 10);
        if ((code + *i) == end)
            return false;
        if (errno == ERANGE)
            return false;
        int len = end - (code + *i);
        *i += len; // Skip over parsed literal with i
        int div = 1;
        while ((len--) > 0)
            div *= 10;

#ifndef CUSTOM_COMP
        *intPtr = (int)round((((double)literal + ((double)literalFrac / (div))) * 256.0));
#endif
    }
    else
        *intPtr = literal;

    Token_AppendArray(Token_GetInt(IntLiteral, intPtr), t, lineNumber, sourceFileName);
    return true;
}

static bool ParseCharLiteral(TokenArray* t, size_t* i, size_t length, int lineNumber, char* code, char* sourceFileName)
{
    if (code[*i] == '\'')
    {
        char literal = 0;
        if (++(*i) >= length)
            return false;
        if (code[*i] == '\\')
        {
            if (++(*i) >= length)
                return false;
            switch (code[*i])
            {
                case 'n':
                    literal = '\n';
                    break;
                case 't':
                    literal = '\t';
                    break;
                case '\'':
                    literal = '\'';
                    break;
                case '0':
                    literal = '\0';
                    break;
                case 'r':
                    literal = '\r';
                    break;
                default:
                    literal = code[*i];
                    break;
            }
        }
        else
            literal = code[*i];
        if (++(*i) >= length || code[*i] != '\'')
            return false;
        int32_t* copy = xmalloc(sizeof(uint32_t));
        *copy = (int32_t)literal;
        Token_AppendArray(Token_GetInt(IntLiteral, copy), t, lineNumber, sourceFileName);
        if (++(*i) >= length)
            return false;
        return true;
    }
    return false;
}

static void SkipWhitespace(size_t* i, size_t length, int* lineNumber, char* code, bool allowNewline)
{
    size_t len = 0;

    while (isspace(code[*i + len]))
    {
        if (code[*i + len] == '\n')
        {
            if (allowNewline)
                (*lineNumber)++;
            else
                ErrorAtLine("Syntax Error!", *lineNumber);
        }
        len++;
        if (*i + len >= length)
            ErrorAtLine("Syntax Error!", *lineNumber);
    }

    (*i) += len;
}

static bool ParseInlineAssembly(TokenArray* t, size_t* i, size_t length, int* lineNumber, char* code,
                                char* sourceFileName)
{
    int len;
    if ((len = ParseStringStrict(length - (*i), code + (*i), "asm")) != -1)
    {
        /*while (isspace(code[*i + len]))
        {
            if (code[*i + len] == '\n') (*lineNumber)++;
            len++;
            if (*i + len >= length) ErrorAtLine("Invalid inline assembly!", *lineNumber);
        }*/
        (*i) += len;
        SkipWhitespace(i, length, lineNumber, code, true);

        if (code[*i] != '{')
            ErrorAtLineInFile("Invalid inline assembly!", *lineNumber, sourceFileName);

        int maxLen = 32;
        char* assembly = xmalloc(maxLen);

        (*i)++;
        int j = 0;
        int k = 0;
        while (isspace(code[*i + k]))
            if (code[*i + (k++)] == '\n')
                (*lineNumber)++;
        // SkipWhitespace(i, length, lineNumber, code, true);

        while (code[*i + k] != '}')
        {
            if (*i + k >= length)
                ErrorAtLineInFile("Invalid inline assembly!", *lineNumber, sourceFileName);
            assembly[j] = code[*i + k];

            if (code[*i + k] == '\n')
            {
                (*lineNumber)++;
                int whitespaceLen = 0;

                while (isspace(code[*i + k + whitespaceLen + 1]))
                    if (code[*i + k + (whitespaceLen++) + 1] == '\n')
                        (*lineNumber)++;

                k += whitespaceLen;
            }

            j++;
            k++;
            if (j >= maxLen)
                assembly = xrealloc(assembly, maxLen = maxLen + 32);
        }

        assembly[j++] = 0;
        (*i) += k + 1;
        Token_AppendArray(Token_GetString(AsmKeyword, assembly), t, *lineNumber, sourceFileName);
        return true;
    }

    return false;
}

static bool ParsePreprocessorDirectives(TokenArray* t, size_t* i, size_t length, int* lineNumber, char* code,
                                        char* sourceFileName)
{
    // Preprocessor directives
    if (code[*i] == '#')
    {
        size_t len;

        if ((len = ParseStringStrict(length - *i, code + *i, "#include")) != SIZE_MAX)
        {
            *i += len;

            SkipWhitespace(i, length, lineNumber, code, false);

            const size_t fileNameBufferLength = 128;
            char fileNameBuffer[128];
            size_t len = 0;

            // Copy the Path of this source file in front of the include-string
            // if using "" to include
            if (code[*i] == '\"')
            {
                const char* iter = sourceFileName;
                const char* substrEnd = iter;
                while (*iter != 0)
                {
                    if (*iter == '/')
                    {
                        substrEnd = iter + 1;
                    }
                    iter++;
                }

                iter = sourceFileName;
                while (iter != substrEnd)
                    fileNameBuffer[len++] = *iter++;
            }

            if (code[*i] == '\"' || code[*i] == '<')
            {
                (*i)++;
                while (code[*i] != '\"' && code[*i] != '>')
                {
                    if (code[*i] == '\n')
                        ErrorAtLineInFile("Unterminated string literal!", *lineNumber, sourceFileName);
                    fileNameBuffer[len++] = code[(*i)++];
                    if (len >= fileNameBufferLength)
                        ErrorAtLineInFile("Include path too long!", *lineNumber, sourceFileName);
                    if ((*i) >= length)
                        ErrorAtLineInFile("Invalid include!", *lineNumber, sourceFileName);
                }
                fileNameBuffer[len++] = 0;
                (*i)++;

                size_t inclLength = 0;
                char* includedFile = readFileAsString(&fileNameBuffer[0], &inclLength);

                LexIntoArray(includedFile, inclLength, t, &fileNameBuffer[0]);
                free(includedFile);
                return true;
            }
        }
        else if ((len = ParseStringStrict(length - *i, code + *i, "#define")) != SIZE_MAX)
        {
            *i += len;
            SkipWhitespace(i, length, lineNumber, code, false);

            const size_t bufferMaxLen = 128;
            char buffer[128]; // = {0};
            size_t j = 0;

            while (!isspace(code[*i]))
            {
                buffer[j++] = code[(*i)++];
                if (*i >= length || j >= bufferMaxLen - 1)
                    ErrorAtLineInFile("Invalid define!", *lineNumber, sourceFileName);
            }
            buffer[j++] = 0;

            if (!Preprocessor_IsValid(&buffer[0]))
                ErrorAtLineInFile("Invalid define!", *lineNumber, sourceFileName);

            Preprocessor_Define(&buffer[0]);

            return true;
        }

        else if ((len = ParseStringStrict(length - *i, code + *i, "#pragma once")) != SIZE_MAX)
        {
            *i += len;

            const char* iter = sourceFileName;
            const char* lastSlashSubstring = iter;
            while (*iter != 0)
            {
                if (*iter == '/')
                {
                    lastSlashSubstring = iter + 1;
                }
                iter++;
            }

            if (Preprocessor_IsDefined(lastSlashSubstring))
                *i = length;
            else
                Preprocessor_Define(lastSlashSubstring);

            return true;
        }

        else if ((len = ParseStringStrict(length - *i, code + *i, "#ifdef")) != SIZE_MAX)
        {
            *i += len;
            SkipWhitespace(i, length, lineNumber, code, false);

            const size_t bufferMaxLen = 128;
            char buffer[128];
            size_t j = 0;

            while (!isspace(code[*i]))
            {
                buffer[j++] = code[(*i)++];
                if (*i >= length || j >= bufferMaxLen - 1)
                    ErrorAtLineInFile("Invalid ifdef!", *lineNumber, sourceFileName);
            }
            buffer[j++] = 0;

            // If not defined, just skip until we find an #endif
            if (!Preprocessor_IsDefined(&buffer[0]))
            {
                int ifDepth = 1;

                while (ifDepth)
                {
                    if (*i >= length)
                        Error("Unterminated #ifdef");
                    if (code[*i] == '#')
                    {
                        if ((len = ParseStringStrict(length - *i, code + *i, "#endif")) != SIZE_MAX)
                        {
                            *i += len;
                            ifDepth--;
                            continue;
                        }
                        if ((len = ParseStringStrict(length - *i, code + *i, "#ifdef")) != SIZE_MAX)
                        {
                            *i += len;
                            ifDepth++;
                            continue;
                        }
                    }
                    if (code[*i] == '\n')
                        (*lineNumber)++;
                    (*i)++;
                }
            }

            return true;
        }

        else if ((len = ParseStringStrict(length - *i, code + *i, "#ifndef")) != SIZE_MAX)
        {
            *i += len;
            SkipWhitespace(i, length, lineNumber, code, false);

            const size_t bufferMaxLen = 128;
            char buffer[128];
            size_t j = 0;

            while (!isspace(code[*i]))
            {
                buffer[j++] = code[(*i)++];
                if (*i >= length || j >= bufferMaxLen - 1)
                    ErrorAtLineInFile("Invalid ifdef!", *lineNumber, sourceFileName);
            }
            buffer[j++] = 0;

            // If defined, just skip until we find an #endif
            if (Preprocessor_IsDefined(&buffer[0]))
            {
                int ifDepth = 1;

                while (ifDepth)
                {
                    if (*i >= length)
                        Error("Unterminated #ifdef");
                    if (code[*i] == '#')
                    {
                        if ((len = ParseStringStrict(length - *i, code + *i, "#endif")) != SIZE_MAX)
                        {
                            *i += len;
                            ifDepth--;
                            continue;
                        }
                        if ((len = ParseStringStrict(length - *i, code + *i, "#ifdef")) != SIZE_MAX)
                        {
                            *i += len;
                            ifDepth++;
                            continue;
                        }
                    }
                    if (code[*i] == '\n')
                        (*lineNumber)++;
                    (*i)++;
                }
            }

            return true;
        }

        // If the endif is relevant, it has already been parsed by its corresponding #ifdef
        // If not, the #ifdef was true, and therefore the #endif is just skipped.
        else if ((len = ParseStringStrict(length - *i, code + *i, "#endif")) != SIZE_MAX)
        {
            (*i) += len;
            return true;
        }
        else
            ErrorAtLineInFile("Could not parse!", *lineNumber, sourceFileName);
    }

    return false;
}

void LexIntoArray(char* code, size_t length, TokenArray* t, char* sourceFileName)
{
    // TokenArray t = CreateTokenArray(32);
    int lineNumber = 1;

    for (size_t i = 0; i < length; NULL)
    {

        size_t remLen = length - i;

        if (code[i] == 0)
            break;

        if (code[i] == '\n')
        {
            i++;
            lineNumber++;
            continue;
        }

        if (isspace(code[i]))
        {
            i++;
            continue;
        }
        size_t len;

        /* Multiline
           Comment  */
        if ((len = ParseString(remLen, code + i, "/*")) != SIZE_MAX)
        {
            i += len;

            while (!(code[i] == '/' && code[i - 1] == '*') && i < length)
                if (code[i++] == '\n')
                    lineNumber++;

            if (code[i++] == '\n')
                lineNumber++;
            continue;
        }

        // Single Line Comment
        if ((len = ParseString(remLen, code + i, "//")) != SIZE_MAX)
        {
            i += len;
            while (code[i] != '\n' && i < length)
                i++;
            continue;
        }

        // Actual C Tokens
        if (ParseNext(code, t, sourceFileName, &i, lineNumber))
            continue;
        if (ParseInlineAssembly(t, &i, length, &lineNumber, code, sourceFileName))
            continue;
        if (ParseIdentifier(t, &i, length, lineNumber, code, sourceFileName))
            continue;
        if (ParseIntLiteral(t, &i, lineNumber, code, sourceFileName))
            continue;
        if (ParseStringLiteral(t, &i, length, lineNumber, code, sourceFileName))
            continue;
        if (ParseCharLiteral(t, &i, length, lineNumber, code, sourceFileName))
            continue;

        if (code[i] == '#' && ParsePreprocessorDirectives(t, &i, length, &lineNumber, code, sourceFileName))
            continue;

        ErrorAtLineInFile("Unrecognized Symbol!", lineNumber, sourceFileName);
    }
}
TokenArray* Lex(char* sourceFilePath)
{
    size_t length;
    char* code = readFileAsString(sourceFilePath, &length);
    if (code == NULL)
        Error("Invalid source file");
    TokenArray* t = Token_CreateArray(32);
    LexIntoArray(code, length, t, sourceFilePath);
    free(code);
    return t;
}
