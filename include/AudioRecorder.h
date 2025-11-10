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
const float PITCH_FACTOR = 2; // 0.5 = octave down, 1.0 = normal, 2.0 = octave up

extern int16_t ringBuffer1[BUFFER_SIZE];
extern int16_t ringBuffer2[BUFFER_SIZE];
// Pitch-shifted output buffers
extern int16_t pitchBuffer1[BUFFER_SIZE];
extern int16_t pitchBuffer2[BUFFER_SIZE];

extern volatile int writeIndex;
extern volatile bool bufferReady;
extern volatile bool shouldRecord;
extern volatile bool dataReadyToConsume;  // ← NEW FLAG

void MIC_setup();
bool MIC_loop(); 
void acknowledgeData(); 

#endif