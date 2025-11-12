// ============================================================================
// DTMFDetector.cpp - DTMF Detection Module Implementation
// ============================================================================
#include "DTMFDetector.h"
#include "utils.h"  // For checkMemory

// Initialize static constexpr members
constexpr float DTMFDetector::DTMF_ROW[4];
constexpr float DTMFDetector::DTMF_COL[4];
constexpr char DTMFDetector::DTMF_CHAR[4][4];

// Goertzel implementation
void DTMFDetector::Goertzel::init(float freq, float sampleRate) {
    float omega = (2.0 * PI * freq) / sampleRate;
    cosine = cos(omega);
    sine = sin(omega);
    coeff = 2.0 * cosine;
    reset();
}

void DTMFDetector::Goertzel::reset() {
    Q1 = 0;
    Q2 = 0;
}

void DTMFDetector::Goertzel::processSample(float sample) {
    float Q0 = coeff * Q1 - Q2 + sample;
    Q2 = Q1;
    Q1 = Q0;
}

float DTMFDetector::Goertzel::getMagnitudeSquared() {
    float real = (Q1 - Q2 * cosine);
    float imag = (Q2 * sine);
    return real * real + imag * imag;
}

// DTMFDetector implementation
DTMFDetector::DTMFDetector() : 
    threshold(DTMF_DETECTION_THRESHOLD),
    dcOffset(2048),
    lastDetected('\0'),
    lastDetectionTime(0),
    audioBuffer(nullptr),
    bufferAllocated(false),
    cursorPos(0),
    isPM(true) {
    resetTimeEntry();
}

DTMFDetector::~DTMFDetector() {
    freeBuffer();
}

bool DTMFDetector::allocateBuffer() {
    if (bufferAllocated) {
        return true; // Already allocated
    }
    
    checkMemory("Before DTMF buffer allocation");
    
    // Use regular heap allocation instead of PSRAM
    audioBuffer = (int16_t*)malloc(DTMF_BUFFER_SIZE * sizeof(int16_t));
    
    if (audioBuffer) {
        bufferAllocated = true;
        memset(audioBuffer, 0, DTMF_BUFFER_SIZE * sizeof(int16_t));
        Serial.printf("[DTMF] Buffer allocated successfully (%d bytes)\n", 
                     DTMF_BUFFER_SIZE * sizeof(int16_t));
        checkMemory("After DTMF buffer allocation");
        return true;
    } else {
        Serial.println("[DTMF] ERROR: Buffer allocation failed!");
        bufferAllocated = false;
        return false;
    }
}

void DTMFDetector::freeBuffer() {
    if (audioBuffer && bufferAllocated) {
        free(audioBuffer);
        audioBuffer = nullptr;
        bufferAllocated = false;
        Serial.println("[DTMF] Buffer freed");
        checkMemory("After DTMF buffer free");
    }
}

void DTMFDetector::init() {
    // Initialize Goertzel filters
    for (int i = 0; i < 4; i++) {
        rowTones[i].init(DTMF_ROW[i], DTMF_SAMPLE_RATE);
        colTones[i].init(DTMF_COL[i], DTMF_SAMPLE_RATE);
    }
    Serial.println("[DTMF] Detector initialized");
}

void DTMFDetector::calibrateDCOffset(int micPin) {
    Serial.println("[DTMF] Calibrating DC offset...");
    long sum = 0;
    for (int i = 0; i < 1000; i++) {
        sum += analogRead(micPin);
        delayMicroseconds(100);
    }
    dcOffset = sum / 1000;
    Serial.printf("[DTMF] DC Offset calibrated: %d\n", dcOffset);
}

char DTMFDetector::detectTone() {
    if (!bufferAllocated || !audioBuffer) {
        return '\0';
    }
    
    // Reset filters
    for (int i = 0; i < 4; i++) {
        rowTones[i].reset();
        colTones[i].reset();
    }
    
    // Process samples
    for (int i = 0; i < DTMF_BUFFER_SIZE; i++) {
        float sample = audioBuffer[i] / 2048.0;  // Normalize
        for (int j = 0; j < 4; j++) {
            rowTones[j].processSample(sample);
            colTones[j].processSample(sample);
        }
    }
    
    // Find peaks
    float maxRowPower = 0;
    float maxColPower = 0;
    int maxRowIndex = -1;
    int maxColIndex = -1;
    
    for (int i = 0; i < 4; i++) {
        float rowPower = rowTones[i].getMagnitudeSquared();
        float colPower = colTones[i].getMagnitudeSquared();
        
        if (rowPower > maxRowPower) {
            maxRowPower = rowPower;
            maxRowIndex = i;
        }
        
        if (colPower > maxColPower) {
            maxColPower = colPower;
            maxColIndex = i;
        }
    }
    
    // Check if strong enough
    if (maxRowPower > threshold && maxColPower > threshold) {
        // Validate - check if significantly stronger than others
        bool valid = true;
        for (int i = 0; i < 4; i++) {
            float rowPower = rowTones[i].getMagnitudeSquared();
            float colPower = colTones[i].getMagnitudeSquared();
            
            if (i != maxRowIndex && rowPower * 2.5 > maxRowPower) valid = false;
            if (i != maxColIndex && colPower * 2.5 > maxColPower) valid = false;
        }
        
        if (valid && maxRowIndex >= 0 && maxColIndex >= 0) {
            return DTMF_CHAR[maxRowIndex][maxColIndex];
        }
    }
    
    return '\0';
}

char DTMFDetector::recordAndDetect(int micPin) {
    if (!bufferAllocated || !audioBuffer) {
        return '\0';
    }
    
    // Record audio buffer at 8kHz
    unsigned long startTime = micros();
    for (int i = 0; i < DTMF_BUFFER_SIZE; i++) {
        audioBuffer[i] = analogRead(micPin) - dcOffset;
        
        // Maintain 8kHz sample rate (125 microseconds per sample)
        while (micros() - startTime < (i + 1) * 125) {}
    }
    
    // Detect DTMF tone
    char detected = detectTone();
    
    // Debounce detection
    if (detected != '\0') {
        if (detected != lastDetected || millis() - lastDetectionTime > 200) {
            lastDetected = detected;
            lastDetectionTime = millis();
            return detected;
        }
    } else {
        // Reset after no detection
        if (millis() - lastDetectionTime > 500) {
            lastDetected = '\0';
        }
    }
    
    return '\0';
}

void DTMFDetector::resetTimeEntry() {
    timeEntry = "____";  // 4 digits for HH:MM
    cursorPos = 0;
    isPM = true;
}

bool DTMFDetector::processTimeEntry(char digit) {
    if (digit == 'A') {
        // Toggle AM/PM
        isPM = !isPM;
        Serial.printf("[DTMF] Toggled to %s\n", isPM ? "PM" : "AM");
        return false;  // Not complete
    }
    else if (digit == 'D') {
        // Backspace
        if (cursorPos > 0) {
            cursorPos--;
            timeEntry[cursorPos] = '_';
            Serial.printf("[DTMF] Backspace - cursor at %d\n", cursorPos);
        }
        return false;  // Not complete
    }
    else if (digit == 'C') {
        // Confirm
        if (isTimeComplete()) {
            Serial.println("[DTMF] Time entry confirmed");
            return true;  // Complete!
        } else {
            Serial.println("[DTMF] Cannot confirm - time incomplete");
            return false;
        }
    }
    else if (isdigit(digit) && cursorPos < 4) {
        // Validate time entry
        bool valid = true;
        
        if (cursorPos == 0) {
            // First digit of hour (0-1 for 12hr format)
            if (digit > '1') valid = false;
        }
        else if (cursorPos == 1) {
            // Second digit of hour
            if (timeEntry[0] == '1' && digit > '2') valid = false;  // Max 12
            if (timeEntry[0] == '0' && digit == '0') valid = false;  // Min 01
        }
        else if (cursorPos == 2) {
            // First digit of minute (0-5)
            if (digit > '5') valid = false;
        }
        // cursorPos == 3: Second digit of minute (0-9) - always valid
        
        if (valid) {
            timeEntry[cursorPos] = digit;
            cursorPos++;
            Serial.printf("[DTMF] Added %c at position %d\n", digit, cursorPos-1);
        } else {
            Serial.printf("[DTMF] Invalid digit %c for position %d\n", digit, cursorPos);
        }
        return false;  // Not complete yet
    }
    
    return false;
}

String DTMFDetector::getTimeDisplay() {
    String display = "";
    
    // Format: HH:MM AM/PM
    display += timeEntry[0];
    display += timeEntry[1];
    display += ':';
    display += timeEntry[2];
    display += timeEntry[3];
    display += ' ';
    display += isPM ? "PM" : "AM";
    
    return display;
}

String DTMFDetector::getTimeValue() {
    if (!isTimeComplete()) {
        return "";
    }
    
    // Convert to 24hr format for processing
    String hour = String(timeEntry[0]) + String(timeEntry[1]);
    String minute = String(timeEntry[2]) + String(timeEntry[3]);
    
    int hr = hour.toInt();
    
    // Convert to 24hr format
    if (!isPM && hr == 12) {
        hr = 0;  // 12 AM = 00:00
    } else if (isPM && hr != 12) {
        hr += 12;  // PM hours
    }
    
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", hr, minute.toInt());
    
    return String(timeStr);
}

bool DTMFDetector::isTimeComplete() {
    for (int i = 0; i < 4; i++) {
        if (timeEntry[i] == '_') {
            return false;
        }
    }
    return true;
}

