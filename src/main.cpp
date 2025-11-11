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
    Serial.begin(1021600);//1021600
    while(!Serial);
    
    Serial.println("\n=== Voice Detection Test ===");
    Serial.println("Initializing...");
    
    // Initialize detector
    detector = new VoiceDetector();    
    Serial.println("Model loaded!");
    
    MIC_setup();

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


void Run_Wit()
{
    if (WIT_loop()) 
    {
        WIT_acknowledgeData();
        m_states = PROCESS_INTENT;
        Serial.println("\nReady to process");
    }
}


void Run_WakeWord()
{
    if (MIC_loop()) 
    {
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
            m_states = WIT_STATE;
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