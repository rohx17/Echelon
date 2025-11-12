// ============================================================================
// Updated main.cpp - SET_REMINDER state with DTMF detection
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
#include "LcdTimeDisplay.h"
#include "DTMFDetector.h"  // Add DTMF detector

VoiceDetector* detector;
LaserAttackDetector* laserDetector;
LcdTimeDisplay* lcdDisplay;
DTMFDetector* dtmfDetector;  // DTMF detector instance

enum MainStates{
    WIFI_CONNECT,
    START_WAKE_WORD_STATE,
    WAKE_WORD_STATE,
    WIT_STATE,
    PROCESS_INTENT,
    DTMF_INPUT_STATE,  // New state for DTMF input
};

MainStates m_states;
String reminderTime = "";  // Store the final time

void Run_WifiConnectionCheck();
void Run_WakeWord();
bool Run_VerifyLaser();
void Run_Wit();
void Run_Process();
void Run_Cleanup();
void Run_DTMFInput();  // New function for DTMF handling
void connectWiFi();
void handleIntentProcessing();

void setup() {
    Serial.begin(115200);  // Changed to match DTMF requirements
    while(!Serial);
    
    Serial.println("\n=== Voice Detection Test with LCD & DTMF ===");
    
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
    
    // Initialize DTMF detector
    dtmfDetector = new DTMFDetector();
    dtmfDetector->init();
    Serial.println("DTMF detector initialized");
    lcdDisplay->updateStatus("DTMF OK");
    delay(500);

    MIC_setup();
    checkMemory("After MIC setup");
    lcdDisplay->updateStatus("Mic OK");
    delay(500);

    m_states = WIFI_CONNECT;
    delay(1000);
}

void loop() {
    // Update time display continuously (except during DTMF input)
    if (m_states != DTMF_INPUT_STATE) {
        lcdDisplay->updateTime();
    }
    
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
            handleIntentProcessing();
            break;
            
        case DTMF_INPUT_STATE:
            Run_DTMFInput();
            break;
        
        default:
            break;
    }
    Run_WifiConnectionCheck();
}

void Run_DTMFInput() {
    static bool dtmfInitialized = false;
    static unsigned long lastUpdateTime = 0;
    
    // Initialize DTMF input on first entry
    if (!dtmfInitialized) {
        // Free wake word buffers to make room
        freeBuffers();
        checkMemory("After freeing wake word buffers");
        
        // Allocate DTMF buffer
        if (!dtmfDetector->allocateBuffer()) {
            Serial.println("[DTMF] Failed to allocate buffer!");
            lcdDisplay->updateStatus("DTMF Buf Err!");
            delay(2000);
            m_states = START_WAKE_WORD_STATE;
            return;
        }
        
        // Calibrate DC offset
        dtmfDetector->calibrateDCOffset(micPin1);
        
        // Reset time entry
        dtmfDetector->resetTimeEntry();
        
        // Update LCD with initial display
        lcdDisplay->updateStatus(dtmfDetector->getTimeDisplay().c_str());
        
        dtmfInitialized = true;
        Serial.println("\n[DTMF] Ready for time input:");
        Serial.println("  Digits 0-9: Enter time");
        Serial.println("  A: Toggle AM/PM");
        Serial.println("  C: Confirm");
        Serial.println("  D: Backspace");
    }
    
    // Record and detect DTMF
    char detected = dtmfDetector->recordAndDetect(micPin1);
    
    if (detected != '\0') {
        Serial.printf("[DTMF] Detected: %c\n", detected);
        
        // Process the detected tone
        bool isComplete = dtmfDetector->processTimeEntry(detected);
        
        // Update LCD display
        lcdDisplay->updateStatus(dtmfDetector->getTimeDisplay().c_str());
        
        if (isComplete) {
            // Get the final time value
            reminderTime = dtmfDetector->getTimeValue();
            Serial.printf("[DTMF] Reminder set for: %s\n", reminderTime.c_str());
            
            // Show confirmation
            String confirmMsg = "Set: " + reminderTime;
            lcdDisplay->updateStatus(confirmMsg.c_str());
            
            // Clean up DTMF
            dtmfDetector->freeBuffer();
            dtmfInitialized = false;
            checkMemory("After DTMF cleanup");
            
            // Wait a bit to show confirmation
            delay(2000);
            
            // Return to wake word detection
            m_states = START_WAKE_WORD_STATE;
        }
    }
    
    // Update display periodically to show cursor
    if (millis() - lastUpdateTime > 500) {
        lcdDisplay->updateStatus(dtmfDetector->getTimeDisplay().c_str());
        lastUpdateTime = millis();
    }
}

void handleIntentProcessing() {
    
    switch(p_states)
    {
        case EMPTY:
            Serial.println("📊 Nothing to process...");
            lcdDisplay->updateStatus("Empty");
            delay(2000);
            m_states = START_WAKE_WORD_STATE;
            break;
            
        case MORNING_PILL:
            Serial.println("📊 Processing MORNING PILL reminder");
            lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_MORNING_PILL);
            
            // Simulate processing
            delay(2000);
            
            // Show confirmation
            lcdDisplay->updateStatus("Pill Set: AM");
            Serial.println("✓ Morning pill reminder has been set");
            delay(2000);
            
            // Return to wake word detection
            m_states = START_WAKE_WORD_STATE;
            break;
        
        case EVENING_PILL:
            Serial.println("🌙 Processing EVENING PILL reminder");
            lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_EVENING_PILL);
        
            // Simulate processing
            delay(2000);
            
            // Show confirmation
            lcdDisplay->updateStatus("Pill Set: PM");
            Serial.println("✓ Evening pill reminder has been set");
            delay(2000);
            
            // Return to wake word detection
            m_states = START_WAKE_WORD_STATE;
            break;
        
        case VERIFY_ME:
            Serial.println("🔍 Processing VERIFY ME command");
            lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_VERIFYING);
            
            // Simulate verification process
            delay(1500);
            m_states = START_WAKE_WORD_STATE;
            break;
        
        case SET_REMINDER:
            Serial.println("⏰ Processing SET REMINDER command");
            lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_SET_REMINDER);
            
            // Transition to DTMF input state
            delay(1000);
            lcdDisplay->updateStatus("Time Input");
            delay(500);
            m_states = DTMF_INPUT_STATE;
            break;
        
        default:
            Serial.println("⚠️ Unknown intent - returning to wake word");
            lcdDisplay->updateStatus("Unknown Cmd");
            delay(2000);
            m_states = START_WAKE_WORD_STATE;
            break;
    }
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