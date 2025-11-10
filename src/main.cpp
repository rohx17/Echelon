// ============================================================================
// main.cpp - Test with preloaded audio
// ============================================================================
#include <Arduino.h>
#include "VoiceDetector.h"
#include "audio_data.h"

VoiceDetector* detector;


// Add this to your setup() function after initializing the detector
// This will help debug the MFCC extraction differences

void debugMFCCExtraction() {
    Serial.println("\n=== MFCC Debug Test ===");
    
    // Get the audio processor from detector (you may need to make it public)
    // Or create a temporary one for testing
    AudioProcessor* testProcessor = new AudioProcessor();
    
    // Print mel filterbank info
    testProcessor->printMelFilterbank();
    
    // Debug comparison with Python
    testProcessor->debugCompareWithPython(audio_data, AUDIO_LENGTH);
    
    // Extract MFCC and print detailed info
    float test_mfcc[N_FRAMES][N_MFCC];
    testProcessor->extractMFCC(audio_data, AUDIO_LENGTH, test_mfcc);
    
    Serial.println("\n=== MFCC Results ===");
    
    // Print frames 0, 10, 20 for comparison with Python
    int frames_to_check[] = {0, 10, 20};
    for (int f = 0; f < 3; f++) {
        int frame = frames_to_check[f];
        Serial.print("Frame ");
        Serial.print(frame);
        Serial.print(": ");
        for (int i = 0; i < N_MFCC; i++) {
            Serial.print(test_mfcc[frame][i], 6);
            Serial.print(" ");
        }
        Serial.println();
    }
    
    // Check if first coefficient is in expected range
    Serial.println("\nFirst coefficient (C0) analysis:");
    float c0_min = 1000.0f, c0_max = -1000.0f, c0_avg = 0.0f;
    for (int frame = 0; frame < N_FRAMES; frame++) {
        float c0 = test_mfcc[frame][0];
        if (c0 < c0_min) c0_min = c0;
        if (c0 > c0_max) c0_max = c0;
        c0_avg += c0;
    }
    c0_avg /= N_FRAMES;
    
    Serial.print("C0 range: [");
    Serial.print(c0_min);
    Serial.print(", ");
    Serial.print(c0_max);
    Serial.print("], avg: ");
    Serial.println(c0_avg);
    
    Serial.println("\nExpected Python C0 range: [-50, -25] approximately");
    Serial.println("If your C0 is not in this range, the DCT is likely incorrect");
    
    delete testProcessor;
}

void setup() {
    Serial.begin(115200);
    while(!Serial);
    
    Serial.println("\n=== Voice Detection Test ===");
    Serial.println("Initializing...");
    
    // Initialize detector
    detector = new VoiceDetector();
    
    Serial.println("Model loaded!");
    Serial.print("Audio data: ");
    Serial.print(AUDIO_LENGTH);
    Serial.print(" samples @ ");
    Serial.print(AUDIO_SAMPLE_RATE);
    Serial.println(" Hz");
    
    
// In your setup() function, call:
    debugMFCCExtraction();
    delay(1000);
}

void loop() {
    Serial.println("\n--- Running Detection ---");
    
    unsigned long start_time = millis();
    
    // Run detection on preloaded audio
    float score = detector->detectWakeWord(audio_data, AUDIO_LENGTH);
    
    unsigned long inference_time = millis() - start_time;
    
    // Print results
    Serial.print("Detection Score: ");
    Serial.print(score * 100, 1);
    Serial.print("%");
    
    if (score > 0.5) {
        Serial.println(" 😊 WAKE WORD DETECTED!");
    } else {
        Serial.println(" ❌ Not detected");
    }
    
    Serial.print("Inference time: ");
    Serial.print(inference_time);
    Serial.println(" ms");
    
    // Optional: Print first few MFCC frames for debugging
    detector->printMFCC(0);
    detector->printMFCC(10);
    detector->printMFCC(20);
    
    delay(5000);  // Wait 5 seconds before next test
}