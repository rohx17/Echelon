// ============================================================================
// main.cpp - Test with preloaded audio + LCD Display + NTP Time
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
#include "LcdTimeDisplay.h"  // Add LCD and Time display

VoiceDetector* detector;
LaserAttackDetector* laserDetector;
LcdTimeDisplay* lcdDisplay;  // LCD display instance

enum MainStates{
    WIFI_CONNECT,              // LCD display - Connecting ... / Connected
    START_WAKE_WORD_STATE,     // LCD display - Initializing Recording
    WAKE_WORD_STATE,           // LCD display - Waiting... / Detected
    WIT_STATE,                 // LCD display - POST WIT / 
    PROCESS_INTENT,            // LCD display - Display outcome
};

MainStates m_states;

void Run_WifiConnectionCheck();
void Run_WakeWord();
bool Run_VerifyLaser();
void Run_Wit();
void Run_Process();
void Run_Cleanup();
void connectWiFi();

void setup() {
    Serial.begin(1021600);
    while(!Serial);
    
    Serial.println("\n=== Voice Detection Test with LCD ===");
    
    // Initialize LCD Display first
    lcdDisplay = new LcdTimeDisplay();
    lcdDisplay->begin();
    lcdDisplay->updateStatus("Starting up...");
    
    checkMemory("Setup start");
    
    // Initialize detector
    detector = new VoiceDetector();    
    Serial.println("Model loaded!");
    lcdDisplay->updateStatus("Model OK");
    delay(500);
    
    laserDetector = new LaserAttackDetector();
    Serial.println("Laser attack detector initialized");
    lcdDisplay->updateStatus("Security OK");
    delay(500);

    MIC_setup();
    checkMemory("After MIC setup");
    lcdDisplay->updateStatus("Mic OK");
    delay(500);

    m_states = WIFI_CONNECT;
    delay(1000);
}

void loop() {
    // Update time display continuously
    lcdDisplay->updateTime();
    
    switch (m_states)
    {
        case WIFI_CONNECT:
            connectWiFi();
            m_states = START_WAKE_WORD_STATE;
            break;
            
        case START_WAKE_WORD_STATE:
            lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_INITIALIZING);
            continuousRecording = true;
            if (allocateWakeWordBuffers()) {
                startRecording();
                m_states = WAKE_WORD_STATE;
                lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_WAITING);
            }
            break;
            
        case WAKE_WORD_STATE:
            Run_WakeWord();
            break;
            
        case WIT_STATE:
            Run_Wit();
            break;
            
        case PROCESS_INTENT:
            Run_Cleanup();
            m_states = START_WAKE_WORD_STATE;
            break;
        
        default:
            break;
    }
    Run_WifiConnectionCheck();
}

void Run_Wit() {
    // Update LCD status
    lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_PROCESSING_WIT);
    
    // Allocate only ringBuffer1 for Wit.ai (3 seconds)
    if (!buffersAllocated) {
        if (!allocateWitBuffers()) {
            Serial.println("ERROR: Failed to allocate Wit.ai buffers!");
            lcdDisplay->updateStatus("Buffer Error!");
            delay(2000);
            return;
        }
    }
    
    if (WIT_loop()) {
        WIT_acknowledgeData();
        freeBuffers();  // Free Wit.ai buffer
        m_states = PROCESS_INTENT;
        Serial.println("\nReady to process");
        lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_INTENT_READY);
        checkMemory("After Wit.ai processing");
        delay(1500);  // Show status briefly
    }
}

// Add a new state to free all memory
void Run_Cleanup() {
    freeBuffers();
    checkMemory("After cleanup");
}

void Run_WakeWord() {
    // Allocate 4 buffers for wake word (1 second each)
    if (!buffersAllocated) {
        if (!allocateWakeWordBuffers()) {
            Serial.println("ERROR: Failed to allocate wake word buffers!");
            lcdDisplay->updateStatus("Buffer Error!");
            delay(2000);
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
            lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_DETECTED);
            delay(500);  // Show detection briefly
            
            continuousRecording = false;  // Stop continuous recording
            if (!Run_VerifyLaser()) {
                // Attack detected - restart wake word detection
                lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_LASER_ALERT);
                Serial.println("Restarting wake word detection...");
                delay(2000);  // Show alert for 2 seconds
                continuousRecording = true;
                acknowledgeData();
                startRecording();
                lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_WAITING);
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

// Modified Run_VerifyLaser with auto-calibration and LCD updates
bool Run_VerifyLaser() {
    static bool firstRun = true;
    
    lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_LASER_CHECK);
    
    // Auto-calibrate on first successful wake word
    if (firstRun) {
        Serial.println("\n📊 First wake word - calibrating detector...");
        lcdDisplay->updateStatus("Calibrating...");
        laserDetector->calibrate(pitchBuffer1, pitchBuffer2, BUFFER_SIZE);
        firstRun = false;
        
        // On first run, assume it's legitimate (for calibration)
        Serial.println("✅ Calibration complete - proceeding normally");
        delay(1000);
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
    static bool wasConnected = false;
    
    if (WiFi.status() != WL_CONNECTED) {
        if (wasConnected) {  // Only show message if we were previously connected
            Serial.println("WiFi disconnected! Reconnecting...");
            lcdDisplay->updateStatus("WiFi Lost!");
            wasConnected = false;
        }
        connectWiFi();
    } else {
        if (!wasConnected) {
            wasConnected = true;
        }
    }
}

void connectWiFi() {
    Serial.print("Connecting to WiFi");
    lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_WIFI_CONNECTING);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
        
        // Animate dots on LCD
        String dots = "";
        for(int i = 0; i < (attempts % 4); i++) {
            dots += ".";
        }
        String status = String("WiFi Connect") + dots;
        lcdDisplay->updateStatus(status.c_str());
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        
        lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_WIFI_CONNECTED);
        delay(1000);
        
        // Sync time after WiFi connection
        Serial.println("Syncing time with NTP server...");
        lcdDisplay->updateStatus("Time Sync...");
        lcdDisplay->forceTimeSync();
        delay(1000);
        
        m_states = WAKE_WORD_STATE;
        lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_WAITING);
    } else {
        Serial.println("\nWiFi connection FAILED!");
        lcdDisplay->updateStatus("WiFi Failed!");
    }
}