// ============================================================================
// VoiceDetector.cpp - Updated for (1, 79, 10, 1) input shape
// ============================================================================
#include "VoiceDetector.h"
#include <Arduino.h>

VoiceDetector::VoiceDetector() {
    nn = new NeuralNetwork();
    audioProcessor = new AudioProcessor();
}

VoiceDetector::~VoiceDetector() {
    delete nn;
    delete audioProcessor;
}

float VoiceDetector::detectWakeWord(const float* audio, int length) {
    // Extract MFCC features (79 frames x 10 coefficients)
    audioProcessor->extractMFCC(audio, length, mfcc_features);
    
    // Get input buffer from neural network
    // Expected shape: (1, 79, 10, 1) = batch, height, width, channels
    // Data layout: 1 * 79 * 10 * 1 = 790 floats
    float* input_buffer = nn->getInputBuffer();
    
    // Copy MFCC features to input buffer
    // Memory layout for (1, 79, 10, 1):
    //   - 1 batch
    //   - 79 rows (time frames)
    //   - 10 columns (MFCC coefficients)
    //   - 1 channel
    // The channel dimension is implicit (always 1), so we flatten as before
    int idx = 0;
    for (int frame = 0; frame < N_FRAMES; frame++) {
        for (int mfcc = 0; mfcc < N_MFCC; mfcc++) {
            input_buffer[idx++] = mfcc_features[frame][mfcc];
        }
    }
    
    // Total: 79 * 10 * 1 = 790 values copied
    
    // Run inference
    float score = nn->predict();
    
    return score;
}

void VoiceDetector::printMFCC(int frame) {
    if (frame >= N_FRAMES) return;
    
    Serial.print("Frame ");
    Serial.print(frame);
    Serial.print(": ");
    for (int i = 0; i < N_MFCC; i++) {
        Serial.print(mfcc_features[frame][i], 2);
        Serial.print(" ");
    }
    Serial.println();
}
