#include "GenericList.h"
#include "Util.h"
#include <assert.h>
#include <stdint.h>

GenericList GenericList_Create(size_t memberSize)
{
    GenericList l;

    l.data = xmalloc(DEFAULT_COUNT * memberSize);
    l.count = 0;
    l.maxCount = DEFAULT_COUNT;
    l.memberSize = memberSize;
    assert(memberSize != 0);

    return l;
}

GenericList GenericList_CreateCopy(GenericList original)
{
    GenericList copy;

    copy.data = xmalloc(original.memberSize * original.maxCount);
    copy.count = original.count;
    copy.maxCount = original.maxCount;
    copy.memberSize = original.memberSize;

    memcpy(copy.data, original.data, original.memberSize * original.count);

    return copy;
}

void* GenericList_Append(GenericList* this, void* member)
{
    if (this->count == this->maxCount)
    {
        this->data = xrealloc(this->data, (this->maxCount *= 2) * this->memberSize);
    }

    void* addr = this->data + this->count * this->memberSize;
    memcpy(addr, member, this->memberSize);
    this->count++;
    return addr;
}

void* GenericList_At(const GenericList* this, size_t index)
{
    return this->data + index * this->memberSize;
}

// Find an object in this List based on a generic comparator. Left side in comparator(void*, void*)
// is object pointer in list, right side is always compareTo.
void* GenericList_Find(GenericList* this, bool (*comparator)(const void*, const void*), const void* compareTo)
{
    for (size_t i = 0; i < this->count; i++)
    {
        if (comparator(this->data + this->memberSize * i, compareTo))
            return this->data + this->memberSize * i;
    }
    return NULL;
}

void GenericList_Delete(GenericList* this, void* member)
{
    void* lastAddr = this->data + this->memberSize * this->count;
    assert(member >= this->data && member < lastAddr);
    void* nextMember = member + this->memberSize;

    while (nextMember < lastAddr)
    {
#ifndef CUSTOM_COMP
        *((uint8_t*)member) = *((uint8_t*)nextMember);
#endif
#ifdef CUSTOM_COMP
        *((uint16_t*)member) = *((uint16_t*)nextMember);
#endif

        member++;
        nextMember++;
    }
    this->count--;
}

bool GenericList_Contains(GenericList* this, void* member)
{
    void* lastAddr = this->data + this->memberSize * this->count;
    return (member >= this->data && member < lastAddr);
}

void GenericList_ShrinkToSize(GenericList* this)
{
    if (this->count != 0)
    {
        this->data = xrealloc(this->data, this->count * this->memberSize);
        this->maxCount = this->count;
    }
    else
    {
        this->data = xrealloc(this->data, this->memberSize);
        this->maxCount = 1;
    }
}

void GenericList_Dispose(GenericList* this)
{
    free(this->data);
    this->count = 0;
    this->maxCount = 0;
    this->memberSize = 0;
    this->data = NULL;
}