#include <Arduino.h>
#include "utils.h"

// Add this at the top of main.cpp after includes
void checkMemory(const char* location) {
  Serial.printf("[MEMORY] %s - Free Heap: %d bytes, Min Free: %d bytes\n", 
                location, 
                ESP.getFreeHeap(), 
                ESP.getMinFreeHeap());
}

void* allocateAudioBuffer(size_t samples, const char* name) {
  size_t bytes = samples * sizeof(int16_t);
  void* buffer = malloc(bytes);
  if (buffer) {
    memset(buffer, 0, bytes);
    Serial.printf("[MEMORY] Allocated %s: %d bytes (%d samples)\n", name, bytes, samples);
    checkMemory("After allocation");
  } else {
    Serial.printf("[MEMORY] ERROR: Failed to allocate %s (%d bytes)\n", name, bytes);
  }
  return buffer;
}

void freeAudioBuffer(void* buffer, const char* name) {
  if (buffer) {
    free(buffer);
    Serial.printf("[MEMORY] Freed %s\n", name);
    checkMemory("After free");
  }
}
