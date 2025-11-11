// ============================================================================
// LaserAttackDetector.h - TUNED VERSION
// ============================================================================
#ifndef LASER_ATTACK_DETECTOR_H
#define LASER_ATTACK_DETECTOR_H

#include <Arduino.h>

class LaserAttackDetector {
private:
    static const int WINDOW_SIZE = 800;  // 50ms at 16kHz
    static const int STRIDE = 400;       // 25ms stride
    
    // ADJUSTED THRESHOLDS for your hardware
    static const int THRESHOLD_Q8 = 102;     // 0.4 (was 0.7) - adjusted for your mics
    static const int GLOBAL_THRESH = 77;     // 0.3 (was 0.8) - much lower for your setup
    static const int MIN_WINDOW_THRESH = 51; // 0.2 (was 0.5)
    
    // Baseline correlation from calibration
    int16_t baselineCorrelation = 135; // Default ~0.53 (your measured value)
    bool isCalibrated = false;
    
    int16_t calculateCorrelationQ8(int16_t* buf1, int16_t* buf2, int size);
    
public:
    struct DetectionResult {
        bool attackDetected;
        uint8_t confidence;
        int16_t globalCorr;
        uint8_t anomalyRatio;
        int16_t minWindowCorr;
    };
    
    // Calibration function - call this with normal audio
    void calibrate(int16_t* buf1, int16_t* buf2, size_t bufferSize);
    
    DetectionResult detectAttack(int16_t* pitchBuffer1, int16_t* pitchBuffer2, size_t bufferSize);
    void printResults(const DetectionResult& result);
};

#endif