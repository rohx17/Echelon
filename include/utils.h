// ============================================================================
// Utils.h - utility
// ============================================================================
#ifndef UTILS_H
#define UTILS_H
#include <cstddef>

void checkMemory(const char* location);
void* allocateAudioBuffer(size_t samples, const char* name);
void freeAudioBuffer(void* buffer, const char* name);
void freeBuffers();

#endif