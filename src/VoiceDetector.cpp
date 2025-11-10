// ============================================================================
// VoiceDetector.cpp - Process int16_t directly
// ============================================================================
#include "VoiceDetector.h"

VoiceDetector::VoiceDetector() {
    nn = new NeuralNetwork();
    audioProcessor = new AudioProcessor();
}

VoiceDetector::~VoiceDetector() {
    delete nn;
    delete audioProcessor;
}

float VoiceDetector::detectWakeWord(const int16_t* audio, int length) {
    // Extract MFCC features directly from int16_t audio
    audioProcessor->extractMFCC(audio, length, mfcc_features);
    
    // Get input buffer from neural network
    float* input_buffer = nn->getInputBuffer();
    
    // Copy MFCC features to input buffer
    int idx = 0;
    for (int frame = 0; frame < N_FRAMES; frame++) {
        for (int mfcc = 0; mfcc < N_MFCC; mfcc++) {
            input_buffer[idx++] = mfcc_features[frame][mfcc];
        }
    }
    
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