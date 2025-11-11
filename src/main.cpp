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
#include "LaserAttackDetector.h"

VoiceDetector* detector;
LaserAttackDetector* laserDetector;

enum MainStates{
    WIFI_CONNECT,       //LCD display - Connecting ... / Connected
    START_WAKE_WORD_STATE,    //LCD display  - Initializing Recording
    WAKE_WORD_STATE,    //LCD display  - Waiting... / Detected
    WIT_STATE,          //LCD display  - POST WIT / 
    PROCESS_INTENT,     //LCD display  - Display outcome
};

MainStates m_states;

void Run_WifiConnectionCheck();
void Run_WakeWord();
bool Run_VerifyLaser();
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
    
    laserDetector = new LaserAttackDetector();
    Serial.println("Laser attack detector initialized");

    MIC_setup();
    checkMemory("After MIC setup");

    m_states = WIFI_CONNECT;
    delay(1000);
}

void loop() {

    switch (m_states)
    {
        case WIFI_CONNECT:
            connectWiFi();
            m_states = START_WAKE_WORD_STATE;
            break;
        case START_WAKE_WORD_STATE:
            continuousRecording = true;
            if (allocateWakeWordBuffers()) {
                startRecording();
                m_states = WAKE_WORD_STATE;
            }
            break;
        case WAKE_WORD_STATE:
            Run_WakeWord();
            break;
        case WIT_STATE:
            Run_Wit();
            /* code */
            break;
        case PROCESS_INTENT:
            m_states = START_WAKE_WORD_STATE;
            /* code */
            break;
        
        default:
            break;
    }
    Run_WifiConnectionCheck();
     
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

        unsigned long start_time = millis();
        float score = detector->detectWakeWord(pitchBuffer1, BUFFER_SIZE);
        unsigned long inference_time = millis() - start_time;
        
        Serial.print("Detection Score: ");
        Serial.print(score * 100, 1);
        Serial.print("%");
        
        if (score > 0.5) {
            Serial.println(" 😊 WAKE WORD DETECTED!");
            continuousRecording = false;  // Stop continuous recording
            if (!Run_VerifyLaser()) {
                // Attack detected - restart wake word detection
                Serial.println("Restarting wake word detection...");
                continuousRecording = true;
                acknowledgeData();
                startRecording();
                return;  
            }
            freeBuffers();  // Free wake word buffers
            startRecording_wit();
            m_states = WIT_STATE;
        } else {
            Serial.println(" ❌ Not detected");
            // Auto-restart recording for next attempt
            if (continuousRecording && !shouldRecord) {
            startRecording();
            }
        }
        
        acknowledgeData();
    }
}
// Modified Run_VerifyLaser with auto-calibration
bool Run_VerifyLaser() {
    static bool firstRun = true;
    
    // Auto-calibrate on first successful wake word
    if (firstRun) {
        Serial.println("\n📊 First wake word - calibrating detector...");
        laserDetector->calibrate(pitchBuffer1, pitchBuffer2, BUFFER_SIZE);
        firstRun = false;
        
        // On first run, assume it's legitimate (for calibration)
        Serial.println("✅ Calibration complete - proceeding normally");
        return true;
    }
    
    Serial.println("\n🔍 Checking for laser attacks...");
    
    LaserAttackDetector::DetectionResult result = 
        laserDetector->detectAttack(pitchBuffer1, pitchBuffer2, BUFFER_SIZE);
    
    laserDetector->printResults(result);
    
    if (result.attackDetected && result.confidence > 60) { // Only block high confidence attacks
        Serial.println("⚠️  SECURITY ALERT: Probable laser attack!");
        Serial.println("Recording may be compromised. Ignoring wake word.");
        return false;
    }
    
    Serial.println("✅ Audio verified - proceeding to Wit.ai");
    return true;
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