#include "Outfile.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static FILE* outFile;
static FILE* outFileData;

bool Outfile_TryOpen(char* path)
{
    outFile = fopen(path, "w");
    if (outFile == NULL)
        return false;

    outFileData = fopen("data.bin", "w");
    if (outFileData == NULL)
        return false;

    return true;
}

void Outfile_CloseFiles()
{
    fclose(outFile);
    fclose(outFileData);
}

void OutWrite(const char* format, ...)
{
#ifndef CUSTOM_COMP
    va_list args;
    va_start(args, format);
    vfprintf(outFile, format, args);
    // vprintf(format, args);
    va_end(args);
#endif
#ifdef CUSTOM_COMP
    vfprintf(outFile, format, (void*)(format - 1));
#endif
}

#ifndef CUSTOM_COMP
void OutWriteData(const uint8_t* data, size_t len)
{
    fwrite(data, sizeof(uint8_t), len, outFileData);
}
#endif
#ifdef CUSTOM_COMP
void OutWriteData(const uint16_t* data, size_t len)
{
    fwrite(data, sizeof(uint16_t), len, outFileData);
}
#endif
void OutWriteZeros(size_t size)
{
    // Yeah, this is slow...
    uint16_t zero = 0;
    while (size--)
        fwrite(&zero, sizeof(uint16_t), 1, outFileData);
}
