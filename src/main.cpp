// ============================================================================
// main.cpp
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
#include "DTMFDetector.h"  
#include "main.h"
#include "WhatsAppVerification.h"

VoiceDetector* detector;
LaserAttackDetector* laserDetector;
LcdTimeDisplay* lcdDisplay;
DTMFDetector* dtmfDetector;
WhatsAppVerification* whatsappVerifier;

bool defenceSet;

enum MainStates{
    WIFI_CONNECT,
    START_WAKE_WORD_STATE,
    WAKE_WORD_STATE,
    WIT_STATE,
    PROCESS_INTENT,
    DTMF_INPUT_STATE,
    VERIFY_CODE_INPUT_STATE, 
};

MainStates m_states;
String reminderTime = "";

void Run_WifiConnectionCheck();
void Run_WakeWord();
bool Run_VerifyLaser();
void Run_Wit();
void Run_Process();
void Run_Cleanup();
void Run_DTMFInput();
void Run_VerificationCodeInput();

void wifiScanner();
void connectWiFi();
void handleIntentProcessing();


void setup() {
    Serial.begin(115200);
    while(!Serial);
    
    checkMemory("Setup start");
    
    // Initialize LCD Display first
    lcdDisplay = new LcdTimeDisplay();
    lcdDisplay->begin();
    lcdDisplay->updateStatus("Starting up...");
    
    // Initialize detector
    detector = new VoiceDetector();    
    Serial.println("Model loaded!");
    lcdDisplay->updateStatus("Model OK");
    delay(500);
    
    laserDetector = new LaserAttackDetector();
    Serial.println("Laser attack detector initialized");
    defenceSet = true;
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

    whatsappVerifier = new WhatsAppVerification();
    whatsappVerifier->init(
        WHATSAPP_PHONE_NUMBER,  
        WHATSAPP_API_KEY        
    );
    
    Serial.println("WhatsApp verification initialized");
    lcdDisplay->updateStatus("WhatsApp OK");
    delay(500);

    m_states = WIFI_CONNECT;
    delay(1000);
}

void loop() {
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

        case VERIFY_CODE_INPUT_STATE:
            Run_VerificationCodeInput();
            break;
        
        default:
            break;
    }
    Run_WifiConnectionCheck();
}

void Run_DTMFInput() {
    static bool dtmfInitialized = false;
    static unsigned long lastUpdateTime = 0;
    
    if (!dtmfInitialized) {
        freeBuffers();
        checkMemory("After freeing wake word buffers");
        
        if (!dtmfDetector->allocateBuffer()) {
            Serial.println("[DTMF] Failed to allocate buffer!");
            lcdDisplay->updateStatus("DTMF Buf Err!");
            delay(2000);
            m_states = START_WAKE_WORD_STATE;
            return;
        }
        
        dtmfDetector->calibrateDCOffset(micPin1);
        dtmfDetector->resetTimeEntry();
        lcdDisplay->updateStatus(dtmfDetector->getTimeDisplay().c_str());
        
        dtmfInitialized = true;
        Serial.println("\n[DTMF] Ready for time input:");
        Serial.println("  Digits 0-9: Enter time");
        Serial.println("  A: Toggle AM/PM");
        Serial.println("  C: Confirm");
        Serial.println("  D: Backspace");
    }
    
    char detected = dtmfDetector->recordAndDetect(micPin1);
    
    if (detected != '\0') {
        Serial.printf("[DTMF] Detected: %c\n", detected);
        bool isComplete = dtmfDetector->processTimeEntry(detected);
        lcdDisplay->updateStatus(dtmfDetector->getTimeDisplay().c_str());
        
        if (isComplete) {
            // Get the final time value (12-hour format with AM/PM)
            reminderTime = dtmfDetector->getTimeValue();
            Serial.printf("[DTMF] Reminder set for: %s\n", reminderTime.c_str());
            
            // Show confirmation with AM/PM
            String confirmMsg = "Set: " + reminderTime;
            lcdDisplay->updateStatus(confirmMsg.c_str());
            
            dtmfDetector->freeBuffer();
            dtmfInitialized = false;
            checkMemory("After DTMF cleanup");
            
            delay(3000);
            m_states = START_WAKE_WORD_STATE;
        }
    }
    
    if (millis() - lastUpdateTime > 500) {
        lcdDisplay->updateStatus(dtmfDetector->getTimeDisplay().c_str());
        lastUpdateTime = millis();
    }
}

void handleIntentProcessing() {
    
    switch(p_states)
    {
        case EMPTY:
            Serial.println("Nothing to process...");
            lcdDisplay->updateStatus("Empty");
            delay(2000);
            m_states = START_WAKE_WORD_STATE;
            break;
            
        case MORNING_PILL:
            Serial.println("Processing MORNING PILL reminder");
            lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_MORNING_PILL);
            
            // Simulate processing
            delay(2000);
            
            lcdDisplay->updateStatus("Pill Set: AM");
            Serial.println("Morning pill reminder has been set");
            delay(2000);
            
            m_states = START_WAKE_WORD_STATE;
            break;
        
        case EVENING_PILL:
            Serial.println("Processing EVENING PILL reminder");
            lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_EVENING_PILL);
        
            delay(2000);
            
            lcdDisplay->updateStatus("Pill Set: PM");
            Serial.println("Evening pill reminder has been set");
            delay(2000);
            
            m_states = START_WAKE_WORD_STATE;
            break;
        
        case VERIFY_ME:
            Serial.println("Processing VERIFY ME command");
            lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_VERIFYING);
            
            Serial.println("[VERIFY] Sending verification code via WhatsApp...");
            lcdDisplay->updateStatus("Sending code...");
            
            if (whatsappVerifier->generateAndSendCode()) {
                Serial.println("[VERIFY] WhatsApp message sent successfully!");
                lcdDisplay->updateStatus("Check WhatsApp!");
                delay(2500);
                
                lcdDisplay->updateStatus("Enter 4 digits");
                delay(2000);
                
                m_states = VERIFY_CODE_INPUT_STATE;
            } else {
                Serial.println("[VERIFY] Failed to send WhatsApp message!");
                Serial.println("[VERIFY] Check your CallMeBot API key and phone number");
                lcdDisplay->updateStatus("WhatsApp Failed!");
                delay(3000);
                
                lcdDisplay->updateStatus("Check API key");
                delay(2000);
                
                m_states = START_WAKE_WORD_STATE;
            }
            break;
        
        case SET_REMINDER:
            Serial.println("Processing SET REMINDER command");
            lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_SET_REMINDER);
            
            delay(1000);
            lcdDisplay->updateStatus("Time Input");
            delay(500);
            m_states = DTMF_INPUT_STATE;
            break;
        
        case STOP_DEFENCE:
            Serial.println("Processing STOP DEFENCE command");
            defenceSet = false;
            lcdDisplay->updateStatus("No Security :(");
            delay(2000);
            m_states = START_WAKE_WORD_STATE;
            break;
        
        default:
            Serial.println("Unknown intent - returning to wake word");
            lcdDisplay->updateStatus("Unknown Cmd");
            delay(2000);
            m_states = START_WAKE_WORD_STATE;
            break;
    }
}

void Run_VerificationCodeInput() {
    static bool verificationInitialized = false;
    static unsigned long lastUpdateTime = 0;
    static unsigned long lastBlinkTime = 0;
    static bool showCursor = true;
    static int attemptCount = 0;
    const int MAX_ATTEMPTS = 3;
    
    if (!verificationInitialized) {
        freeBuffers();
        checkMemory("After freeing buffers for verification");
        
        // Allocate DTMF buffer
        if (!dtmfDetector->allocateBuffer()) {
            Serial.println("[VERIFY] Failed to allocate DTMF buffer!");
            lcdDisplay->updateStatus("Buffer Error!");
            delay(2000);
            m_states = START_WAKE_WORD_STATE;
            return;
        }
        
        dtmfDetector->calibrateDCOffset(micPin1);
        whatsappVerifier->resetCodeEntry();
        String display = "Code: " + whatsappVerifier->getCodeDisplay();
        lcdDisplay->updateStatus(display.c_str());
        
        verificationInitialized = true;
        attemptCount = 0;
        
        Serial.println("\n[VERIFY] Ready for verification code input:");
        Serial.println("  Enter 4-digit code from WhatsApp");
        Serial.println("  C: Confirm");
        Serial.println("  D: Backspace");
        Serial.println("  Waiting for DTMF tones...");
    }
    
    if (whatsappVerifier->isCodeExpired()) {
        Serial.println("[VERIFY] Code expired!");
        lcdDisplay->updateStatus("Code Expired!");
        delay(2000);
        
        dtmfDetector->freeBuffer();
        verificationInitialized = false;
        m_states = START_WAKE_WORD_STATE;
        return;
    }
    
    char detected = dtmfDetector->recordAndDetect(micPin1);
    
    if (detected != '\0') {
        Serial.printf("[VERIFY] DTMF Detected: %c\n", detected);
        
        bool isComplete = whatsappVerifier->processCodeEntry(detected);
        
        String display = "Code: " + whatsappVerifier->getCodeDisplay();
        lcdDisplay->updateStatus(display.c_str());
        
        if (isComplete) {
            if (whatsappVerifier->verifyCode()) {
                Serial.println("[VERIFY] ✓ Identity verified via WhatsApp!");
                lcdDisplay->updateStatus("Verified! ✓");
                
                // Here we can set a flag for verified user
                // isUserVerified = true;
                
                delay(3000);
                
                lcdDisplay->updateStatus("Welcome Back!");
                delay(2000);
            } else {
                attemptCount++;
                Serial.printf("[VERIFY] ✗ Wrong code! Attempt %d/%d\n", 
                             attemptCount, MAX_ATTEMPTS);
                
                if (attemptCount >= MAX_ATTEMPTS) {
                    lcdDisplay->updateStatus("Max attempts!");
                    delay(3000);
                    
                    dtmfDetector->freeBuffer();
                    verificationInitialized = false;
                    m_states = START_WAKE_WORD_STATE;
                    return;
                } else {
                    lcdDisplay->updateStatus("Wrong! Try again");
                    delay(2000);
                    whatsappVerifier->resetCodeEntry();
                    display = "Code: " + whatsappVerifier->getCodeDisplay();
                    lcdDisplay->updateStatus(display.c_str());
                    return;  // Don't exit, allow retry
                }
            }
            
            // Cleanup after verification (success or max attempts)
            dtmfDetector->freeBuffer();
            verificationInitialized = false;
            checkMemory("After verification cleanup");
            m_states = START_WAKE_WORD_STATE;
        }
    }
    
    if (millis() - lastBlinkTime > 500) {
        showCursor = !showCursor;
        String display = "Code: " + whatsappVerifier->getCodeDisplay();
        if (showCursor && !whatsappVerifier->isCodeComplete()) {
            display += "_";
        }
        lcdDisplay->updateStatus(display.c_str());
        lastBlinkTime = millis();
    }
}

void Run_Wit() {
    
    lcdDisplay->updateStatus("Listening...");
    
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
        freeBuffers();
        m_states = PROCESS_INTENT;
        Serial.println("\nReady to process");
        lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_INTENT_READY);
        checkMemory("After Wit.ai processing");
        delay(1500);
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
        
        if (score > 0.95) {
            Serial.println(" 😊 WAKE WORD DETECTED!");
            lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_DETECTED);
            delay(500);  
            
            continuousRecording = false;  
            if (!Run_VerifyLaser() && defenceSet) {
                // Attack detected - restart wake word detection
                lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_LASER_ALERT);
                Serial.println("Restarting wake word detection...");
                delay(2000);  
                continuousRecording = true;
                acknowledgeData();
                startRecording();
                lcdDisplay->updateStatus(LcdTimeDisplay::STATUS_WAITING);
                return;  
            }
            else if(!Run_VerifyLaser() && !defenceSet){
                lcdDisplay->updateStatus("Ok Attacker :(");
                delay(1000);  
            }
            freeBuffers();  
            startRecording_wit();
            m_states = WIT_STATE;
        } else {
            Serial.println(" ❌ Not detected");
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
        Serial.println("\nFirst wake word - calibrating detector...");
        lcdDisplay->updateStatus("Calibrating...");
        laserDetector->calibrate(pitchBuffer1, pitchBuffer2, BUFFER_SIZE);
        firstRun = false;
        
        // On first run, assume it's legitimate (for calibration)
        Serial.println("Calibration complete - proceeding normally");
        delay(1000);
        return true;
    }
    
    Serial.println("\nChecking for laser attacks...");
    
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
        if (wasConnected) {  
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

void wifiScanner(){
    Serial.println("Scan start");
 
    // WiFi.scanNetworks will return the number of networks found.
    int n = WiFi.scanNetworks();
    Serial.println("Scan done");
    if (n == 0) {
        Serial.println("no networks found");
    } else {
        Serial.print(n);
        Serial.println(" networks found");
        Serial.println("Nr | SSID                             | RSSI | CH | Encryption");
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.printf("%2d",i + 1);
            Serial.print(" | ");
            Serial.printf("%-32.32s", WiFi.SSID(i).c_str());
            Serial.print(" | ");
            Serial.printf("%4d", WiFi.RSSI(i));
            Serial.print(" | ");
            Serial.printf("%2d", WiFi.channel(i));
            Serial.print(" | ");
            switch (WiFi.encryptionType(i))
            {
            case WIFI_AUTH_OPEN:
                Serial.print("open");
                break;
            case WIFI_AUTH_WEP:
                Serial.print("WEP");
                break;
            case WIFI_AUTH_WPA_PSK:
                Serial.print("WPA");
                break;
            case WIFI_AUTH_WPA2_PSK:
                Serial.print("WPA2");
                break;
            case WIFI_AUTH_WPA_WPA2_PSK:
                Serial.print("WPA+WPA2");
                break;
            case WIFI_AUTH_WPA2_ENTERPRISE:
                Serial.print("WPA2-EAP");
                break;
            case WIFI_AUTH_WPA3_PSK:
                Serial.print("WPA3");
                break;
            case WIFI_AUTH_WPA2_WPA3_PSK:
                Serial.print("WPA2+WPA3");
                break;
            case WIFI_AUTH_WAPI_PSK:
                Serial.print("WAPI");
                break;
            default:
                Serial.print("unknown");
            }
            Serial.println();
            delay(10);
        }
    }
    Serial.println("");
 
    // Delete the scan result to free memory for code below.
    WiFi.scanDelete();
}

void connectWiFi() {
    Serial.print("Scanning WiFi...");
    lcdDisplay->updateStatus("Scanning WiFi...");
    wifiScanner();

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