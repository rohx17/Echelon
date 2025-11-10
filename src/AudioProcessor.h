// ============================================================================
// AudioProcessor.h - Header File with Proper Mel Filterbank
// ============================================================================
#ifndef AUDIO_PROCESSOR_H
#define AUDIO_PROCESSOR_H

#include <math.h>

// Define constants outside the class so they're accessible everywhere
#define AUDIO_N_FFT 256
#define AUDIO_HOP_LENGTH 200
#define AUDIO_FFT_BINS (AUDIO_N_FFT / 2 + 1)
#define MEL_BINS 20
#define N_MFCC 10
#define N_FRAMES 79

// Use M_PI from math.h or define our own if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class AudioProcessor {
public:
    // Audio parameters
    static const int SAMPLE_RATE = 16000;
    
private:
    // Buffers
    float hanning_window[AUDIO_N_FFT];
    float fft_real[AUDIO_N_FFT];
    float fft_imag[AUDIO_N_FFT];
    float power_spectrum[AUDIO_FFT_BINS];
    float mel_spectrum[MEL_BINS];
    float log_mel[MEL_BINS];
    float mel_filterbank[MEL_BINS][AUDIO_FFT_BINS];  // Mel filterbank matrix
    float dct_matrix[N_MFCC][MEL_BINS];  // Pre-computed DCT matrix
    
    // Helper functions
    void computeFFT(float* real, float* imag, int n);
    float hzToMel(float hz);
    float melToHz(float mel);
    void initializeMelFilterbank();
    void initializeDCTMatrix();
    
public:
    AudioProcessor();
    void extractMFCC(const float* audio, int length, float mfcc_features[][N_MFCC]);
    void printMelFilterbank();  // Debug function
    void debugCompareWithPython(const float* audio, int length);  // Debug comparison
};

#endif // AUDIO_PROCESSOR_H