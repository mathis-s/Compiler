#pragma once

#include "GenericList.h"

typedef struct
{
    char* identifier;
    GenericList members;
    int sizeInWords;

} Struct;
