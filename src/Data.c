#include "Data.h"
#include "Outfile.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

size_t globalDataIndex = 0;
size_t AllocateGlobalValue(size_t size)
{
    OutWriteZeros(size);
    size_t retval = globalDataIndex;
    globalDataIndex += size;
    return retval;
}

size_t AllocateGlobalWord(uint16_t word)
{
    OutWriteData((void*)(&word), sizeof(uint16_t));
    size_t retval = globalDataIndex;
    globalDataIndex += 1;
    return retval;
}

size_t AllocateGlobalDoubleWord(uint32_t dword)
{
    OutWriteData((void*)(&dword), sizeof(uint32_t));
    size_t retval = globalDataIndex;
    globalDataIndex += 2;
    return retval;
}

size_t AllocateAndWriteStringLiteral(const char* str)
{
    size_t retval = globalDataIndex;
    size_t len = 1;
    while (*str != 0)
    {
        AllocateGlobalWord((uint16_t)(*str));
        len++;
        str++;
    }

    AllocateGlobalWord(0);
    return retval;
}

size_t GetGlobalDataIndex()
{
    return globalDataIndex;
}

void Init(size_t dataAddress)
{
    globalDataIndex = dataAddress;
}