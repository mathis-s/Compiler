#pragma once

/*
These are all of the stdlib functions used in the compiler;
just here for compiling itself.
*/
void* malloc(size_t s);
int32_t strtol(const char* c, char** endPointer, int base);
struct FILE;
FILE* fopen(const char* path, const char* modes);
int fseek(FILE* f, int32 off, int whence);
int32_t ftell(FILE* f);
int32_t fread(void* ptr, size_t size, size_t n, FILE* f);
int fclose(FILE* f);

int strlen(const char* s);
void free(void* ptr);
void* memcpy(void* dst, void* src, size_t len);
void* realloc(void* ptr, size_t size);

const int SEEK_END = 2;
const int SEEK_SET = 0;
uint isalpha(char c);
uint isdigit(char c);
uint isspace(char c);

const size_t SIZE_MAX = 0xFFFF;

int errno = 0;

const int ERANGE = 34;
void assert(bool a);

int strcmp(const char* a, const char* b);
char* strcpy(char* dst, char* src);

void printf(const char* str, ...);
void exit(int err);
int vfprintf(FILE* f, const char* fmt, void* va_args);
int fwrite(void* data, size_t size, size_t n, FILE* file);
int sprintf(char* s, const char* fmt, ...);

void qsort(void* base, size_t num, size_t membSize, bool (*compar)(const void*, const void*));
void memset(void* dst, uint word, size_t len);