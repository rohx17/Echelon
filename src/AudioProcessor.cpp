// ============================================================================
// AudioProcessor.cpp - Process int16_t audio
// ============================================================================
#include "AudioProcessor.h"

AudioProcessor::AudioProcessor() {
    // Initialize Hanning window
    for (int i = 0; i < N_FFT; i++) {
        hanning_window[i] = 0.5 * (1.0 - cos(2.0 * PI * i / (N_FFT - 1)));
    }
}

// Simplified FFT for N=256 (power of 2)
void AudioProcessor::computeFFT(float* real, float* imag, int n) {
    // Bit-reversal permutation
    int j = 0;
    for (int i = 0; i < n - 1; i++) {
        if (i < j) {
            float temp = real[i];
            real[i] = real[j];
            real[j] = temp;
            temp = imag[i];
            imag[i] = imag[j];
            imag[j] = temp;
        }
        int k = n / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }
    
    // Cooley-Tukey FFT
    for (int len = 2; len <= n; len *= 2) {
        float angle = -2.0 * PI / len;
        for (int i = 0; i < n; i += len) {
            for (int k = 0; k < len / 2; k++) {
                float cos_val = cos(angle * k);
                float sin_val = sin(angle * k);
                
                int idx1 = i + k;
                int idx2 = i + k + len / 2;
                
                float tr = cos_val * real[idx2] - sin_val * imag[idx2];
                float ti = cos_val * imag[idx2] + sin_val * real[idx2];
                
                real[idx2] = real[idx1] - tr;
                imag[idx2] = imag[idx1] - ti;
                real[idx1] = real[idx1] + tr;
                imag[idx1] = imag[idx1] + ti;
            }
        }
    }
}

// Extract MFCC from int16_t audio (matching ESP32 mic format)
void AudioProcessor::extractMFCC(const int16_t* audio, int length, float mfcc_features[][N_MFCC]) {
    // Process each frame
    int frame_idx = 0;
    
    for (int start = 0; start + N_FFT <= length && frame_idx < N_FRAMES; start += HOP_LENGTH) {
        // Apply window and convert int16_t to float
        // Note: We keep the int16_t scale here (no division by 32768)
        for (int i = 0; i < N_FFT; i++) {
            fft_real[i] = (float)audio[start + i] * hanning_window[i];
            fft_imag[i] = 0.0f;
        }
        
        // Compute FFT
        computeFFT(fft_real, fft_imag, N_FFT);
        
        // Compute power spectrum (only positive frequencies)
        for (int i = 0; i < FFT_BINS; i++) {
            power_spectrum[i] = fft_real[i] * fft_real[i] + fft_imag[i] * fft_imag[i];
        }
        
        // Apply simplified mel filterbank
        for (int mel = 0; mel < MEL_BINS; mel++) {
            mel_spectrum[mel] = 0.0f;
            
            int start_bin = (mel * FFT_BINS) / MEL_BINS;
            int end_bin = ((mel + 1) * FFT_BINS) / MEL_BINS;
            
            for (int bin = start_bin; bin < end_bin; bin++) {
                mel_spectrum[mel] += power_spectrum[bin];
            }
            mel_spectrum[mel] /= (end_bin - start_bin);
        }
        
        // Log mel spectrum
        for (int i = 0; i < MEL_BINS; i++) {
            log_mel[i] = log(mel_spectrum[i] + 1e-6f);
        }
        
        // DCT to get MFCCs
        for (int mfcc = 0; mfcc < N_MFCC; mfcc++) {
            mfcc_features[frame_idx][mfcc] = 0.0f;
            for (int mel = 0; mel < MEL_BINS; mel++) {
                mfcc_features[frame_idx][mfcc] += 
                    log_mel[mel] * cos(PI * mfcc * (mel + 0.5) / MEL_BINS);
            }
            mfcc_features[frame_idx][mfcc] *= sqrt(2.0 / MEL_BINS);
        }
        
        frame_idx++;
    }
}