// ============================================================================
// main.cpp - Test with preloaded audio
// ============================================================================
#include <Arduino.h>
#include "VoiceDetector.h"
#include "audio_data.h"
#include "AudioRecorder.h"

VoiceDetector* detector;

void setup() {
    Serial.begin(1021600);//1021600
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
     if (MIC_loop()) {

        Serial.println("\n✓ Audio data ready!");

        unsigned long start_time = millis();
        // Run detection on preloaded audio
        float score = detector->detectWakeWord(pitchBuffer1, BUFFER_SIZE);
        
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
        acknowledgeData();
     }
}