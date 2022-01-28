#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

bool Outfile_TryOpen(char* path);
void Outfile_CloseFiles();
void OutWrite(const char* format, ...);
#ifndef CUSTOM_COMP
void OutWriteData(const uint8_t* data, size_t size);
#endif
#ifdef CUSTOM_COMP
void OutWriteData(uint16_t* data, size_t size);
#endif
void OutWriteZeros(size_t len);