// ============================================================================
// AudioRecorder.h - Record Dual Mic audio to Ring buffer
// ============================================================================
#ifndef AUDIO_RECORDER_H
#define AUDIO_RECORDER_H
#include <Arduino.h>

const int micPin1 = A0;
const int micPin2 = A1;
const int SAMPLE_RATE = 16000;
const int BUFFER_SIZE = 16000; // 1 second buffer
const int BUFFER_SIZE_MIC1 = 48000; // 3 second buffer

// extern int16_t ringBuffer1[BUFFER_SIZE_MIC1];
// extern int16_t ringBuffer2[BUFFER_SIZE];
// // Pitch-shifted output buffers
// extern int16_t pitchBuffer1[BUFFER_SIZE];
// extern int16_t pitchBuffer2[BUFFER_SIZE];

extern int16_t* ringBuffer1;
extern int16_t* ringBuffer2;
extern int16_t* pitchBuffer1;
extern int16_t* pitchBuffer2;

extern volatile bool buffersAllocated;
extern size_t currentBufferSize;

// Memory management functions
bool allocateWakeWordBuffers(); 
bool allocateWitBuffers(); 
void freeBuffers();


void MIC_setup();
bool MIC_loop(); 
void acknowledgeData(); 

#endif