#include "Preprocessor.h"
#include "Error.h"
#include "Lexer.h"
#include "Util.h"
#include <stdlib.h>
#include <string.h>

static char* defines[64];
static size_t currentLenDefines = 0;
static const size_t maxLenDefines = 64;

bool Preprocessor_IsValid(const char* id)
{
    if (!isalpha(*id) && *id != '_')
        return false;
    id++;

    while (*id++)
        if (!isalpha(*id) && !isdigit(*id) && *id != '_')
            return false;

    return true;
}

bool Preprocessor_IsDefined(const char* id)
{
    for (size_t i = 0; i < currentLenDefines; i++)
    {
        if (strcmp(id, defines[i]) == 0)
            return true;
    }
    return false;
}

void Preprocessor_Define(const char* id)
{
    if (Preprocessor_IsDefined(id))
        return;

    if (currentLenDefines == maxLenDefines)
        Error("Too many defines!");

    char* copy = xmalloc(strlen(id) + 1);
    strcpy(copy, id);
    defines[currentLenDefines++] = copy;
}

void Preprocessor_Clear()
{
    for (size_t i = 1; i < currentLenDefines; i++)
    {
        free(defines[i]);
    }
    currentLenDefines = 1;
}

void Preprocessor_Undefine(const char* id)
{
    for (size_t i = 0; i < currentLenDefines; i++)
    {
        if (strcmp(id, defines[i]) == 0)
        {
            free(defines[i]);
            for (size_t j = i + 1; j < currentLenDefines; j++)
            {
                defines[j - 1] = defines[j];
                currentLenDefines--;
                return;
            }
        }
    }
}

void Preprocessor_End()
{
    for (size_t i = 0; i < currentLenDefines; i++)
    {
        if (defines[i] != NULL)
            free(defines[i]);
    }
}