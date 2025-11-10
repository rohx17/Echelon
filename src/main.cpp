// ============================================================================
// main.cpp - Test with preloaded audio
// ============================================================================
#include <Arduino.h>
#include "VoiceDetector.h"
#include "audio_data.h"

VoiceDetector* detector;

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
    
    delay(1000);
}

void loop() {
    Serial.println("\n--- Running Detection ---");
    
    unsigned long start_time = millis();
    
    // Run detection on preloaded audio
    float score = detector->detectWakeWord(mic1_audio_data, AUDIO_LENGTH);
    
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