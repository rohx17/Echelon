// ============================================================================
// VoiceDetector.h - int16_t only
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
    
    // Process int16_t audio (matches ESP32 mic format)
    float detectWakeWord(const int16_t* audio, int length);
    
    // Helper to print MFCC features for debugging
    void printMFCC(int frame);
};

#endif