// ============================================================================
// AudioProcessor.h - MFCC Feature Extraction
// ============================================================================
#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <Arduino.h>
#include <cmath>


// Audio parameters (must match training)
#define SAMPLE_RATE 16000
#define N_FFT 256
#define HOP_LENGTH 200
#define N_MELS 20
#define N_MFCC 10
#define N_FRAMES 79  // (16000 - 256) / 200 + 1

class AudioProcessor {
private:
    // Mel filterbank (precomputed to save memory)
    static const int MEL_BINS = N_MELS;
    static const int FFT_BINS = N_FFT / 2 + 1;
    
    // Working buffers
    float fft_input[N_FFT];
    float fft_real[N_FFT];
    float fft_imag[N_FFT];
    float power_spectrum[FFT_BINS];
    float mel_spectrum[MEL_BINS];
    float log_mel[MEL_BINS];
    
    // Hanning window
    float hanning_window[N_FFT];
    
    // Mel filterbank weights (simplified version)
    void computeMelFilterbank();
    
    // Simple FFT (for small N_FFT=256)
    void computeFFT(float* real, float* imag, int n);
    
public:
    AudioProcessor();
    
    // Extract MFCC features from audio
    // Output: mfcc_features[N_FRAMES][N_MFCC]
    void extractMFCC(const float* audio, int length, float mfcc_features[][N_MFCC]);
};

#endif
