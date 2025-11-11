// ============================================================================
// main.cpp - Test with preloaded audio
// ============================================================================
#include <Arduino.h>
#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "VoiceDetector.h"
#include "AudioRecorder.h"
#include "WitAiProcess.h"
#include "utils.h"

VoiceDetector* detector;

enum MainStates{
    WIFI_CONNECT,       //LCD display - Connecting ... / Connected
    WAKE_WORD_STATE,    //LCD display  - Waiting... / Detected
    VERIFY_LASER,       //LCD display  - Verifying... / Pass/Fail
    WIT_STATE,          //LCD display  - POST WIT / 
    PROCESS_INTENT,     //LCD display  - Display outcome
};

MainStates m_states;

void Run_WifiConnectionCheck();
void Run_WakeWord();
void Run_VerifyLaser();
void Run_Wit();
void Run_Process();

void connectWiFi();


void setup() {
    Serial.begin(1021600);
    while(!Serial);
    
    Serial.println("\n=== Voice Detection Test ===");
    checkMemory("Setup start");
    
    // Initialize detector
    detector = new VoiceDetector();    
    Serial.println("Model loaded!");
    
    MIC_setup();
    checkMemory("After MIC setup");

    m_states = WIFI_CONNECT;
    delay(1000);
}

void loop() {
    
    Run_WifiConnectionCheck();

    switch (m_states)
    {
        case WIFI_CONNECT:
            connectWiFi();
            break;
        case WAKE_WORD_STATE:
            Run_WakeWord();
            break;
        case VERIFY_LASER:
            /* code */
            break;
        case WIT_STATE:
            Run_Wit();
            /* code */
            break;
        case PROCESS_INTENT:
            /* code */
            break;
        
        default:
            break;
    }
     
}

void Run_Wit() {
    // Allocate only ringBuffer1 for Wit.ai (3 seconds)
    if (!buffersAllocated) {
        if (!allocateWitBuffers()) {
            Serial.println("ERROR: Failed to allocate Wit.ai buffers!");
            return;
        }
    }
    
    if (WIT_loop()) {
        WIT_acknowledgeData();
        freeBuffers();  // Free Wit.ai buffer
        m_states = PROCESS_INTENT;
        Serial.println("\nReady to process");
        checkMemory("After Wit.ai processing");
    }
}


// Add a new state to free all memory
void Run_Cleanup() {
    freeBuffers();
    checkMemory("After cleanup");
    // Transition to appropriate state
    m_states = WAKE_WORD_STATE;
}


void Run_WakeWord() {
    // Allocate 4 buffers for wake word (1 second each)
    if (!buffersAllocated) {
        if (!allocateWakeWordBuffers()) {
            Serial.println("ERROR: Failed to allocate wake word buffers!");
            return;
        }
    }
    
    if (MIC_loop()) {
        Serial.println("\n✓ Audio data ready!");
        checkMemory("Before wake word detection");

        unsigned long start_time = millis();
        float score = detector->detectWakeWord(pitchBuffer1, BUFFER_SIZE);
        
        unsigned long inference_time = millis() - start_time;
        
        Serial.print("Detection Score: ");
        Serial.print(score * 100, 1);
        Serial.print("%");
        
        if (score > 0) {
            Serial.println(" 😊 WAKE WORD DETECTED!");
            freeBuffers();  // Free wake word buffers
            m_states = WIT_STATE;
        } else {
            Serial.println(" ❌ Not detected");
            // Keep buffers for next wake word attempt
        }
        
        acknowledgeData();
    }
}

void Run_WifiConnectionCheck(){
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected! Reconnecting...");
        connectWiFi();
    }

}

void connectWiFi() {
  Serial.print("Connecting to WiFi"); //LCD_Connecting
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!"); //LCD_Connected
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    m_states = WAKE_WORD_STATE;
  } else {
    Serial.println("\nWiFi connection FAILED!"); //LCD_ConnectionFailed
  }
}