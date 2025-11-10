#include <Arduino.h>

const int micPin1 = A0;
const int micPin2 = A1;
const int SAMPLE_RATE = 16000;
const int BUFFER_SIZE = 16000; // 1 second buffer
const float PITCH_FACTOR = 1.5; // 0.5 = octave down, 1.0 = normal, 2.0 = octave up



