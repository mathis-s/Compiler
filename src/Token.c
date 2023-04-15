#include "Token.h"
#include "Error.h"
#include "Util.h"
#include <assert.h>
#include <string.h>

// We create a LUT that keeps track of the last token on any given lineNumber
// in any given sourceFile. This way we can look these up for error messages
// without having to store them for every individual token.
static void StoreLocation(TokenArray* t, size_t tokenIndex, uint16_t lineNumber, char* sourceFile)
{
    if (t->locationsCount == 0 || (t->locations[(t->locationsCount - 1)]).location.lineNumber != lineNumber ||
        strcmp(t->locations[t->locationsCount - 1].location.sourceFile, sourceFile) != 0)
    {
        t->locationsCount++;
        if (t->locationsCount > t->maxLocationsCount)
        {
            t->maxLocationsCount += 128;
            t->locations = xrealloc(t->locations, (t->maxLocationsCount) * sizeof(TokenSourceGroup));
        }

        char* newString = NULL;
        // No need to create another copy of the file name string in memory if it is still the same
        if (t->locationsCount >= 2 && strcmp(t->locations[t->locationsCount - 2].location.sourceFile, sourceFile) == 0)
        {
            newString = t->locations[t->locationsCount - 2].location.sourceFile;
        }
        else
        {
            newString = xmalloc(strlen(sourceFile) + 1);
            strcpy(newString, sourceFile);
        }
        t->locations[t->locationsCount - 1].location.sourceFile = newString;

        t->locations[t->locationsCount - 1].location.lineNumber = lineNumber;
        t->locations[t->locationsCount - 1].highestTokenIndex = tokenIndex;
    }
    else
    {
        t->locations[t->locationsCount - 1].highestTokenIndex = tokenIndex;
    }
}
static TokenArray* currentTokenArray = NULL;

SourceLocation Token_GetLocationP(const Token* token)
{
    size_t offset = (void*)token - (void*)currentTokenArray->tokens;
    offset /= sizeof(Token);
    assert(offset < currentTokenArray->curLength);

    return Token_GetLocation(offset);
}

SourceLocation Token_GetLocation(size_t tokenIndex)
{
    for (size_t i = 0; i < currentTokenArray->locationsCount; i++)
    {
        if (currentTokenArray->locations[i].highestTokenIndex >= tokenIndex)
            return currentTokenArray->locations[i].location;
    }
    assert(0);
}

Token Token_Get(TokenType type)
{
    Token t = {type, NULL};
    return t;
}
Token Token_GetString(TokenType type, char* string)
{
    Token t = {type, string};
    return t;
}
Token Token_GetInt(TokenType type, int32_t* integer)
{
    Token t = {type, integer};
    return t;
}
TokenArray* Token_CreateArray(size_t size)
{
    TokenArray* arr = xmalloc(sizeof(TokenArray));

    arr->tokens = xmalloc(sizeof(Token) * size);
    arr->maxLength = size;
    arr->curLength = 0;

    arr->locationsCount = 0;
    arr->maxLocationsCount = 32;
    arr->locations = xmalloc(sizeof(TokenSourceGroup) * 32);

    currentTokenArray = arr;

    return arr;
}

void Token_DeleteArray(TokenArray* array)
{
    // Freeing the strings is a little more involved, as they are
    // reused for many locations
    for (size_t i = 0; i < array->locationsCount; i++)
    {
        void* cur = array->locations[i].location.sourceFile;
        void* next;
        if (i + 1 >= array->locationsCount)
            next = NULL;
        else
            next = array->locations[i + 1].location.sourceFile;

        if (cur != next)
            free(cur);
    }
    for (size_t i = 0; i < array->curLength; i++)
    {
        void* data = array->tokens[i].data;
        if (data)
            free(data);
    }
    free(array->tokens);
    free(array->locations);
    free(array);
}
void Token_AppendArray(Token t, TokenArray* array, uint16_t lineNumber, char* sourceFile)
{
    if (array->curLength + 1 > array->maxLength)
    {
        array->tokens = xrealloc(array->tokens, (array->maxLength *= 2) * sizeof(Token));
    }

    StoreLocation(array, array->curLength, lineNumber, sourceFile);
    array->tokens[array->curLength++] = t;
}

// Some helper methods for easier parsing
void* PopNext(size_t* i, TokenType type)
{
    if (++(*i) >= currentTokenArray->curLength)
        SyntaxErrorAtIndex(*i - 1);
    if (currentTokenArray->tokens[*i].type != type)
        SyntaxErrorAtIndex(*i);
        
    return currentTokenArray->tokens[*i].data;
}

void* PopNextInc(size_t* i, TokenType type)
{
    if (++(*i) >= currentTokenArray->curLength)
        SyntaxErrorAtIndex(*i - 1);
    if (currentTokenArray->tokens[*i].type != type)
        SyntaxErrorAtIndex(*i);
    if (++(*i) >= currentTokenArray->curLength)
        SyntaxErrorAtIndex(*i - 1);

    return currentTokenArray->tokens[(*i) - 1].data;
}

void* PopCur(size_t* i, TokenType type)
{
    if (currentTokenArray->tokens[*i].type != type)
        SyntaxErrorAtIndex(*i);
    if (++(*i) >= currentTokenArray->curLength)
        SyntaxErrorAtIndex(*i - 1);

    return currentTokenArray->tokens[(*i) - 1].data;
}

void Inc(size_t* i)
{
    if (++(*i) >= currentTokenArray->curLength)
        SyntaxErrorAtIndex(*i);
}