#pragma once
#include <stddef.h>
#include <stdint.h>

size_t AllocateGlobalValue(size_t size);
void Init(size_t dataAddress);
size_t AllocateGlobalWord(uint16_t word);
size_t AllocateGlobalDoubleWord(uint32_t dword);
size_t AllocateAndWriteStringLiteral(const char* str);
size_t GetGlobalDataIndex();
void Init(size_t dataAddress);