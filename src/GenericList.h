#pragma once

#include "memory.h"
#include "stddef.h"
#include <stdbool.h>
#include <stdlib.h>

static const size_t DEFAULT_COUNT = 1;

typedef struct
{
    void* data;
    size_t memberSize;

    size_t count;
    size_t maxCount;

} GenericList;

GenericList GenericList_Create(size_t memberSize);
GenericList GenericList_CreateCopy(GenericList original);

// Copies memberSize words from member into list. Returns address of data in list.
void* GenericList_Append(GenericList* this, void* member);

void* GenericList_At(GenericList* this, size_t index);

// Find an object in this List based on a generic comparator. Left side in comparator(void*, void*)
// is object pointer in list, right side is always compareTo.
void* GenericList_Find(GenericList* this, bool (*comparator)(const void*, const void*), const void* compareTo);

void GenericList_Dispose(GenericList* this);

void GenericList_ShrinkToSize(GenericList* this);

void GenericList_Delete(GenericList* this, void* member);

bool GenericList_Contains(GenericList* this, void* member);