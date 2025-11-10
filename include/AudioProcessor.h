// ============================================================================
// AudioProcessor.h - Updated for int16_t input
// ============================================================================
#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <Arduino.h>
#include <math.h>

// MFCC parameters matching Python training
const int N_FFT = 256;
const int HOP_LENGTH = 200;
const int FFT_BINS = 129;  // N_FFT/2 + 1
const int MEL_BINS = 20;
const int N_MFCC = 10;
const int N_FRAMES = 79;

class AudioProcessor {
private:
    float hanning_window[N_FFT];
    float fft_real[N_FFT];
    float fft_imag[N_FFT];
    float power_spectrum[FFT_BINS];
    float mel_spectrum[MEL_BINS];
    float log_mel[MEL_BINS];
    
    void computeFFT(float* real, float* imag, int n);
    
public:
    AudioProcessor();
    
    // Updated to accept int16_t directly (matching ESP32 mic format)
    void extractMFCC(const int16_t* audio, int length, float mfcc_features[][N_MFCC]);
};

#endif