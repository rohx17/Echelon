// ============================================================================
// DTMFDetector.h - DTMF Detection Module (Goertzel Algorithm)
// ============================================================================
#ifndef DTMF_DETECTOR_H
#define DTMF_DETECTOR_H

#include <Arduino.h>

#define DTMF_SAMPLE_RATE 8000
#define DTMF_BUFFER_SIZE 800      // Dynamic buffer size
#define DTMF_DETECTION_THRESHOLD 10

class DTMFDetector {
private:
    // Goertzel Algorithm Implementation
    class Goertzel {
    private:
        float coeff;
        float Q1, Q2;
        float sine, cosine;
        
    public:
        void init(float freq, float sampleRate);
        void reset();
        void processSample(float sample);
        float getMagnitudeSquared();
    };
    
    // DTMF Frequencies
    static constexpr float DTMF_ROW[4] = {697, 770, 852, 941};
    static constexpr float DTMF_COL[4] = {1209, 1336, 1477, 1633};
    
    // DTMF Character map
    static constexpr char DTMF_CHAR[4][4] = {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };
    
    // Goertzel filters
    Goertzel rowTones[4];
    Goertzel colTones[4];
    
    // Detection parameters
    float threshold;
    int dcOffset;
    char lastDetected;
    unsigned long lastDetectionTime;
    
    // Dynamic buffer
    int16_t* audioBuffer;
    bool bufferAllocated;
    
    // Time entry state
    String timeEntry;
    int cursorPos;
    bool isPM;
    
public:
    DTMFDetector();
    ~DTMFDetector();
    
    // Memory management
    bool allocateBuffer();
    void freeBuffer();
    
    // Initialization
    void init();
    void calibrateDCOffset(int micPin);
    
    // Detection
    char detectTone();
    char recordAndDetect(int micPin);
    
    // Time entry handling
    void resetTimeEntry();
    bool processTimeEntry(char digit);
    String getTimeDisplay();
    String getTimeValue();
    bool isTimeComplete();
    
    // Getters
    bool isBufferAllocated() { return bufferAllocated; }
    int getDCOffset() { return dcOffset; }
};
#endif