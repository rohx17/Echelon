#include <Arduino.h>
#include "AudioRecorder.h"
#include "utils.h"


const float PITCH_FACTOR = 2; // 0.5 = octave down, 1.0 = normal, 2.0 = octave up

// Make variables accessible
// int16_t ringBuffer1[BUFFER_SIZE_MIC1];
// int16_t ringBuffer2[BUFFER_SIZE];
// int16_t pitchBuffer1[BUFFER_SIZE];
// int16_t pitchBuffer2[BUFFER_SIZE];

int16_t* ringBuffer1 = nullptr;
int16_t* ringBuffer2 = nullptr;
int16_t* pitchBuffer1 = nullptr;
int16_t* pitchBuffer2 = nullptr;

size_t currentBufferSize = 0;

volatile bool continuousRecording = true;
volatile bool buffersAllocated = false;
volatile bool shouldRecord = false;
volatile int writeIndex = 0;
volatile bool bufferReady = false;
volatile bool dataReadyToConsume = false;  


void stopRecording();
void applyPitchShift();
void applySimpleDownsample();
void sendBufferData();


void freeBuffers() {
  freeAudioBuffer(ringBuffer1, "ringBuffer1");
  freeAudioBuffer(ringBuffer2, "ringBuffer2");
  freeAudioBuffer(pitchBuffer1, "pitchBuffer1");
  freeAudioBuffer(pitchBuffer2, "pitchBuffer2");
  
  ringBuffer1 = nullptr;
  ringBuffer2 = nullptr;
  pitchBuffer1 = nullptr;
  pitchBuffer2 = nullptr;
  buffersAllocated = false;
  currentBufferSize = 0;
}


// Allocate 4 buffers for wake word (1 second each)
bool allocateWakeWordBuffers() {
  checkMemory("Before wake word buffer allocation");
  
  freeBuffers();
  
  ringBuffer1 = (int16_t*)allocateAudioBuffer(BUFFER_SIZE, "ringBuffer1");
  ringBuffer2 = (int16_t*)allocateAudioBuffer(BUFFER_SIZE, "ringBuffer2");
  pitchBuffer1 = (int16_t*)allocateAudioBuffer(BUFFER_SIZE, "pitchBuffer1");
  pitchBuffer2 = (int16_t*)allocateAudioBuffer(BUFFER_SIZE, "pitchBuffer2");
  
  buffersAllocated = ringBuffer1 && ringBuffer2 && pitchBuffer1 && pitchBuffer2;
  currentBufferSize = BUFFER_SIZE;
  
  if (buffersAllocated) {
    Serial.println("[MEMORY] Successfully allocated 4 wake word buffers (1 second each)");
  } else {
    Serial.println("[MEMORY] ERROR: Wake word buffer allocation failed!");
    freeBuffers();
  }
  
  return buffersAllocated;
}

// Allocate only ringBuffer1 for Wit.ai (3 seconds)
bool allocateWitBuffers() {
  checkMemory("Before Wit.ai buffer allocation");
  
  freeBuffers();
  
  // Only allocate ringBuffer1 for Wit.ai
  ringBuffer1 = (int16_t*)allocateAudioBuffer(BUFFER_SIZE_MIC1, "ringBuffer1");
  
  // Set other buffers to nullptr (not needed for Wit.ai)
  ringBuffer2 = nullptr;
  pitchBuffer1 = nullptr;
  pitchBuffer2 = nullptr;
  
  buffersAllocated = ringBuffer1 != nullptr;
  currentBufferSize = BUFFER_SIZE_MIC1;
  
  if (buffersAllocated) {
    Serial.println("[MEMORY] Successfully allocated Wit.ai buffer (3 seconds)");
  } else {
    Serial.println("[MEMORY] ERROR: Wit.ai buffer allocation failed!");
    freeBuffers();
  }
  
  return buffersAllocated;
}






void MIC_setup() {
  
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  
  pinMode(micPin1, INPUT);
  pinMode(micPin2, INPUT);
  
  delay(2000);
  Serial.println("DUAL MIC RING BUFFER READY");
  Serial.println("Send 'R' to record 1 second, 'S' to stop");
  Serial.print("Pitch factor: ");
  Serial.println(PITCH_FACTOR);
}

bool MIC_loop() {

  if (!buffersAllocated || !ringBuffer1) {
    Serial.println("ERROR: Buffers not allocated in MIC_loop!");
    return false;
  }
  // Check if we're using wake word configuration (4 buffers)
  bool isWakeWordMode = (ringBuffer2 != nullptr && pitchBuffer1 != nullptr && pitchBuffer2 != nullptr);
  

  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'S' || command == 's') {
      stopRecording();
      continuousRecording = false;  // Stop continuous recording
    } else if (command == 'R' || command == 'r') {
      continuousRecording = true;   // Restart continuous recording
      startRecording();
    }
  }

  // Auto-start recording if in continuous mode and not currently recording
  if (continuousRecording && !shouldRecord && !bufferReady) {
    startRecording();
  }
  
  // If recording, fill ring buffer
  if (shouldRecord) {
    unsigned long startTime = micros();
    
    for (int i = 0; i < 100; i++) { // Read 100 samples at a time
      if (writeIndex >= BUFFER_SIZE) {
        bufferReady = true;
        shouldRecord = false;
        writeIndex = 0;
        break;
      }
      
      // Read both mics
      ringBuffer1[writeIndex] = (int16_t)((analogRead(micPin1) - 2048) * 16);
      ringBuffer2[writeIndex] = (int16_t)((analogRead(micPin2) - 2048) * 16);
      writeIndex++;
      
      // Timing for 16kHz
      while (micros() - startTime < (i + 1) * 62.5) {}
    }
    
    // If buffer is full, process and send data
    if (bufferReady) {
      applyPitchShift();
      // sendBufferData();
      bufferReady = false;
      dataReadyToConsume = true; 
    }
    
  }
  return dataReadyToConsume;
}


void acknowledgeData() {
  // Call this after you've used the pitch buffer data
  dataReadyToConsume = false;
}

void startRecording() {
  if (!buffersAllocated || !ringBuffer1) {
    Serial.println("ERROR: Buffers not allocated in startRecording!");
    return;
  }
  
  writeIndex = 0;
  bufferReady = false;
  shouldRecord = true;
  Serial.println("RECORDING STARTED - Filling 1 second buffer...");
}

void stopRecording() {
  shouldRecord = false;
  Serial.println("RECORDING STOPPED");
}

// Simple pitch shifting using linear interpolation
void applyPitchShift() {
  int outputSamples = (int)(BUFFER_SIZE * PITCH_FACTOR);
  if (outputSamples > BUFFER_SIZE) outputSamples = BUFFER_SIZE;
  
  for (int i = 0; i < outputSamples; i++) {
    // Calculate the source position with floating point precision
    float srcPos = i / PITCH_FACTOR;
    int srcIndex = (int)srcPos;
    float frac = srcPos - srcIndex;
    
    if (srcIndex < BUFFER_SIZE - 1) {
      // Linear interpolation for mic 1
      pitchBuffer1[i] = (int16_t)(
        ringBuffer1[srcIndex] * (1.0f - frac) + 
        ringBuffer1[srcIndex + 1] * frac
      );
      
      // Linear interpolation for mic 2
      pitchBuffer2[i] = (int16_t)(
        ringBuffer2[srcIndex] * (1.0f - frac) + 
        ringBuffer2[srcIndex + 1] * frac
      );
    } else if (srcIndex < BUFFER_SIZE) {
      // Last sample
      pitchBuffer1[i] = ringBuffer1[srcIndex];
      pitchBuffer2[i] = ringBuffer2[srcIndex];
    } else {
      // Pad with silence if needed
      pitchBuffer1[i] = 0;
      pitchBuffer2[i] = 0;
    }
  }
  
  // Pad the rest with silence if output is shorter
  for (int i = outputSamples; i < BUFFER_SIZE; i++) {
    pitchBuffer1[i] = 0;
    pitchBuffer2[i] = 0;
  }
}

// Alternative: Simple downsampling method (faster but lower quality)
void applySimpleDownsample() {
  const int DOWNSAMPLE_FACTOR = 2; // 2 = one octave down
  
  // Clear buffers
  memset(pitchBuffer1, 0, BUFFER_SIZE * sizeof(int16_t));
  memset(pitchBuffer2, 0, BUFFER_SIZE * sizeof(int16_t));
  
  // Downsample by averaging
  for (int i = 0; i < BUFFER_SIZE / DOWNSAMPLE_FACTOR; i++) {
    int32_t sum1 = 0, sum2 = 0;
    for (int j = 0; j < DOWNSAMPLE_FACTOR; j++) {
      int srcIdx = i * DOWNSAMPLE_FACTOR + j;
      if (srcIdx < BUFFER_SIZE) {
        sum1 += ringBuffer1[srcIdx];
        sum2 += ringBuffer2[srcIdx];
      }
    }
    
    // Store averaged sample and duplicate to maintain timing
    int16_t avg1 = sum1 / DOWNSAMPLE_FACTOR;
    int16_t avg2 = sum2 / DOWNSAMPLE_FACTOR;
    
    for (int j = 0; j < DOWNSAMPLE_FACTOR; j++) {
      int dstIdx = i * DOWNSAMPLE_FACTOR + j;
      if (dstIdx < BUFFER_SIZE) {
        pitchBuffer1[dstIdx] = avg1;
        pitchBuffer2[dstIdx] = avg2;
      }
    }
  }
}

void sendBufferData() {
  Serial.write(0xFF);
  Serial.write(0xAA);
  // Send pitch-shifted buffers instead of original
  Serial.write((uint8_t*)pitchBuffer1, BUFFER_SIZE * 2);
  Serial.write((uint8_t*)pitchBuffer2, BUFFER_SIZE * 2);
  Serial.println("BUFFER_SENT");
}


