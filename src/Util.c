#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

void* xmalloc(size_t s)
{
    void* r = malloc(s);
    if (r == NULL)
    {
        printf("Out of memory\n");
        exit(1);
    }
    return r;
}

void* xrealloc(void* ptr, size_t s)
{
    void* r = realloc(ptr, s);
    if (r == NULL)
    {
        printf("Out of memory\n");
        exit(1);
    }
    return r;
}
