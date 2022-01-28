#pragma once
#include <stddef.h>

typedef struct
{
    char* identifier;
} Enum;

typedef struct
{
    Enum* enums;
    size_t count;
    size_t maxCount;
} EnumArray;
