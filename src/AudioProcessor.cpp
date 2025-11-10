// ============================================================================
// AudioProcessor.cpp - Corrected Implementation to Match TensorFlow
// ============================================================================
#include "AudioProcessor.h"
#include <algorithm>
#include <Arduino.h>  // For Serial

AudioProcessor::AudioProcessor() {
    // Initialize Hanning window - TensorFlow uses periodic version
    for (int i = 0; i < AUDIO_N_FFT; i++) {
        // Periodic Hann window (matches tf.signal.hann_window with periodic=True)
        hanning_window[i] = 0.5f - 0.5f * cos(2.0f * M_PI * i / AUDIO_N_FFT);
    }
    
    // Initialize mel filterbank
    initializeMelFilterbank();
    
    // Pre-compute DCT matrix for efficiency and accuracy
    initializeDCTMatrix();
}

// Initialize DCT matrix to match TensorFlow's implementation
void AudioProcessor::initializeDCTMatrix() {
    // TensorFlow uses orthonormal DCT-II
    // The formula for DCT-II coefficient (k, n) is:
    // For k=0: sqrt(1/N) * cos(pi * 0 * (n + 0.5) / N) = sqrt(1/N)
    // For k>0: sqrt(2/N) * cos(pi * k * (n + 0.5) / N)
    
    for (int k = 0; k < N_MFCC; k++) {
        for (int n = 0; n < MEL_BINS; n++) {
            if (k == 0) {
                // DC coefficient - note this is different!
                dct_matrix[k][n] = sqrt(1.0f / MEL_BINS);
            } else {
                // AC coefficients  
                float angle = M_PI * k * (n + 0.5f) / MEL_BINS;
                dct_matrix[k][n] = sqrt(2.0f / MEL_BINS) * cos(angle);
            }
        }
    }
}

// Convert frequency to mel scale
float AudioProcessor::hzToMel(float hz) {
    return 2595.0f * log10(1.0f + hz / 700.0f);
}

// Convert mel scale to frequency
float AudioProcessor::melToHz(float mel) {
    return 700.0f * (pow(10.0f, mel / 2595.0f) - 1.0f);
}

void AudioProcessor::initializeMelFilterbank() {
    // Clear filterbank
    for (int i = 0; i < MEL_BINS; i++) {
        for (int j = 0; j < AUDIO_FFT_BINS; j++) {
            mel_filterbank[i][j] = 0.0f;
        }
    }
    
    // Parameters matching TensorFlow's linear_to_mel_weight_matrix
    float min_freq = 80.0f;   // Lower edge frequency
    float max_freq = 7600.0f; // Upper edge frequency
    float sample_rate = 16000.0f;
    
    // Convert frequency range to mel scale
    float min_mel = hzToMel(min_freq);
    float max_mel = hzToMel(max_freq);
    
    // Create MEL_BINS + 2 equally spaced points in mel scale
    float mel_points[MEL_BINS + 2];
    for (int i = 0; i < MEL_BINS + 2; i++) {
        mel_points[i] = min_mel + i * (max_mel - min_mel) / (MEL_BINS + 1);
    }
    
    // Convert back to Hz
    float hz_points[MEL_BINS + 2];
    for (int i = 0; i < MEL_BINS + 2; i++) {
        hz_points[i] = melToHz(mel_points[i]);
    }
    
    // Convert to FFT bin indices (using round instead of truncation)
    float bin_points[MEL_BINS + 2];
    for (int i = 0; i < MEL_BINS + 2; i++) {
        // More precise bin calculation
        bin_points[i] = (hz_points[i] * AUDIO_N_FFT) / sample_rate;
    }
    
    // Create triangular filters with proper normalization
    for (int m = 0; m < MEL_BINS; m++) {
        float left = bin_points[m];
        float center = bin_points[m + 1];
        float right = bin_points[m + 2];
        
        // Calculate the actual integer bin ranges
        int left_int = (int)floor(left);
        int center_int = (int)ceil(center);
        int right_int = (int)ceil(right);
        
        // Rising edge of triangle
        for (int k = left_int; k < center_int && k < AUDIO_FFT_BINS; k++) {
            if (k >= 0) {
                float freq_k = (k * sample_rate) / AUDIO_N_FFT;
                float hz_left = hz_points[m];
                float hz_center = hz_points[m + 1];
                if (hz_center != hz_left) {
                    mel_filterbank[m][k] = (freq_k - hz_left) / (hz_center - hz_left);
                }
            }
        }
        
        // Falling edge of triangle
        for (int k = center_int; k <= right_int && k < AUDIO_FFT_BINS; k++) {
            if (k >= 0) {
                float freq_k = (k * sample_rate) / AUDIO_N_FFT;
                float hz_center = hz_points[m + 1];
                float hz_right = hz_points[m + 2];
                if (hz_right != hz_center) {
                    mel_filterbank[m][k] = (hz_right - freq_k) / (hz_right - hz_center);
                }
            }
        }
        
        // Normalize each filter by its sum (energy normalization)
        float sum = 0.0f;
        for (int k = 0; k < AUDIO_FFT_BINS; k++) {
            sum += mel_filterbank[m][k];
        }
        if (sum > 0.0f) {
            for (int k = 0; k < AUDIO_FFT_BINS; k++) {
                mel_filterbank[m][k] *= 2.0f / (hz_points[m + 2] - hz_points[m]);
            }
        }
    }
}

// FFT implementation with proper scaling
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
        float angle = -2.0f * M_PI / len;
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
    
    // No additional scaling here - TensorFlow doesn't normalize the FFT
}

void AudioProcessor::extractMFCC(const float* audio, int length, float mfcc_features[][N_MFCC]) {
    // First, normalize the audio like Python does
    float* normalized_audio = new float[length];
    
    // Calculate mean
    float mean = 0.0f;
    for (int i = 0; i < length; i++) {
        mean += audio[i];
    }
    mean /= length;
    
    // Subtract mean
    for (int i = 0; i < length; i++) {
        normalized_audio[i] = audio[i] - mean;
    }
    
    // Find max absolute value
    float max_abs = 1e-10f;  // Avoid division by zero
    for (int i = 0; i < length; i++) {
        float abs_val = fabs(normalized_audio[i]);
        if (abs_val > max_abs) {
            max_abs = abs_val;
        }
    }
    
    // Normalize by max absolute value
    for (int i = 0; i < length; i++) {
        normalized_audio[i] /= max_abs;
    }
    
    // Process each frame
    int frame_idx = 0;
    
    for (int start = 0; start + AUDIO_N_FFT <= length && frame_idx < N_FRAMES; 
         start += AUDIO_HOP_LENGTH) {
        
        // Apply window and copy to FFT buffer
        for (int i = 0; i < AUDIO_N_FFT; i++) {
            fft_real[i] = normalized_audio[start + i] * hanning_window[i];
            fft_imag[i] = 0.0f;
        }
        
        // Compute FFT
        computeFFT(fft_real, fft_imag, AUDIO_N_FFT);
        
        // Compute power spectrum (magnitude squared)
        // Only use positive frequencies (0 to N_FFT/2)
        for (int i = 0; i < AUDIO_FFT_BINS; i++) {
            // Power = real^2 + imag^2
            power_spectrum[i] = fft_real[i] * fft_real[i] + fft_imag[i] * fft_imag[i];
        }
        
        // Apply mel filterbank - matrix multiplication
        for (int mel = 0; mel < MEL_BINS; mel++) {
            mel_spectrum[mel] = 0.0f;
            for (int bin = 0; bin < AUDIO_FFT_BINS; bin++) {
                mel_spectrum[mel] += mel_filterbank[mel][bin] * power_spectrum[bin];
            }
            // Ensure non-negative before log
            if (mel_spectrum[mel] < 1e-10f) {
                mel_spectrum[mel] = 1e-10f;
            }
        }
        
        // Log mel spectrum
        for (int i = 0; i < MEL_BINS; i++) {
            log_mel[i] = log(mel_spectrum[i] + 1e-6f);
        }
        
        // Apply DCT using pre-computed matrix
        for (int k = 0; k < N_MFCC; k++) {
            mfcc_features[frame_idx][k] = 0.0f;
            for (int n = 0; n < MEL_BINS; n++) {
                mfcc_features[frame_idx][k] += dct_matrix[k][n] * log_mel[n];
            }
        }
        
        frame_idx++;
    }
    
    delete[] normalized_audio;
}

// Debug function to compare with Python
void AudioProcessor::debugCompareWithPython(const float* audio, int length) {
    Serial.println("\n=== Debug Comparison ===");
    
    // Process one frame for detailed comparison
    float* normalized_audio = new float[length];
    
    // Normalize
    float mean = 0.0f;
    for (int i = 0; i < length; i++) mean += audio[i];
    mean /= length;
    
    for (int i = 0; i < length; i++) {
        normalized_audio[i] = audio[i] - mean;
    }
    
    float max_abs = 1e-10f;
    for (int i = 0; i < length; i++) {
        float abs_val = fabs(normalized_audio[i]);
        if (abs_val > max_abs) max_abs = abs_val;
    }
    
    for (int i = 0; i < length; i++) {
        normalized_audio[i] /= max_abs;
    }
    
    // First frame
    Serial.println("First 5 normalized samples:");
    for (int i = 0; i < 5; i++) {
        Serial.print(normalized_audio[i], 6);
        Serial.print(" ");
    }
    Serial.println();
    
    // Window and FFT for first frame
    for (int i = 0; i < AUDIO_N_FFT; i++) {
        fft_real[i] = normalized_audio[i] * hanning_window[i];
        fft_imag[i] = 0.0f;
    }
    
    computeFFT(fft_real, fft_imag, AUDIO_N_FFT);
    
    // Power spectrum
    Serial.println("\nFirst 5 power spectrum bins:");
    for (int i = 0; i < 5; i++) {
        float power = fft_real[i] * fft_real[i] + fft_imag[i] * fft_imag[i];
        Serial.print(power, 6);
        Serial.print(" ");
    }
    Serial.println();
    
    // Mel spectrum
    for (int mel = 0; mel < MEL_BINS; mel++) {
        mel_spectrum[mel] = 0.0f;
        for (int bin = 0; bin < AUDIO_FFT_BINS; bin++) {
            float power = fft_real[bin] * fft_real[bin] + fft_imag[bin] * fft_imag[bin];
            mel_spectrum[mel] += mel_filterbank[mel][bin] * power;
        }
    }
    
    Serial.println("\nFirst 5 mel spectrum bins:");
    for (int i = 0; i < 5; i++) {
        Serial.print(mel_spectrum[i], 6);
        Serial.print(" ");
    }
    Serial.println();
    
    Serial.println("\nFirst 5 log mel bins:");
    for (int i = 0; i < 5; i++) {
        Serial.print(log(mel_spectrum[i] + 1e-6f), 6);
        Serial.print(" ");
    }
    Serial.println();
    
    delete[] normalized_audio;
}

// Print mel filterbank for debugging
void AudioProcessor::printMelFilterbank() {
    Serial.println("Mel Filterbank (first 3 filters, showing non-zero bins):");
    for (int i = 0; i < 3 && i < MEL_BINS; i++) {
        Serial.print("Filter ");
        Serial.print(i);
        Serial.print(": ");
        int count = 0;
        for (int j = 0; j < AUDIO_FFT_BINS; j++) {
            if (mel_filterbank[i][j] > 0.001f && count < 10) {
                Serial.print("bin");
                Serial.print(j);
                Serial.print("=");
                Serial.print(mel_filterbank[i][j], 3);
                Serial.print(" ");
                count++;
            }
        }
        Serial.println();
    }
}