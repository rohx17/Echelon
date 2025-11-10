// ============================================================================
// VoiceDetector.h - Updated for (79, 10, 1) input
// ============================================================================
#ifndef VOICE_DETECTOR_H
#define VOICE_DETECTOR_H

#include "NeuralNetwork.h"
#include "AudioProcessor.h"

class VoiceDetector {
private:
    NeuralNetwork* nn;
    AudioProcessor* audioProcessor;
    float mfcc_features[N_FRAMES][N_MFCC];
    
public:
    VoiceDetector();
    ~VoiceDetector();
    
    // Process audio and return detection score (0.0 to 1.0)
    float detectWakeWord(const float* audio, int length);
    
    // Helper to print MFCC features for debugging
    void printMFCC(int frame);
};

#endif