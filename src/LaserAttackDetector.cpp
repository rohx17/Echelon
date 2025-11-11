// ============================================================================
// LaserAttackDetector.cpp - TUNED VERSION
// ============================================================================
#include "LaserAttackDetector.h"
// Calibration function - learns your mic characteristics
void LaserAttackDetector::calibrate(int16_t* buf1, int16_t* buf2, size_t bufferSize) {
    Serial.println("\n📊 Calibrating laser detector...");
    
    // Calculate baseline correlation for your mics
    baselineCorrelation = calculateCorrelationQ8(buf1, buf2, bufferSize);
    isCalibrated = true;
    
    Serial.print("Baseline correlation: ");
    Serial.print(baselineCorrelation * 100 / 256);
    Serial.println("%");
    
    // Auto-adjust if baseline is very low
    if (baselineCorrelation < 128) { // Less than 50%
        Serial.println("⚠️  Mics have very different characteristics");
        Serial.println("    Adjusting sensitivity for your hardware...");
    }
}

LaserAttackDetector::DetectionResult LaserAttackDetector::detectAttack(
    int16_t* pitchBuffer1, 
    int16_t* pitchBuffer2, 
    size_t bufferSize) {
    
    DetectionResult result = {false, 0, 0, 0, 255};
    
    // 1. Calculate global correlation
    result.globalCorr = calculateCorrelationQ8(pitchBuffer1, pitchBuffer2, bufferSize);
    
    // 2. Sliding window analysis
    int windows = 0;
    int anomalousWindows = 0;
    int32_t sumCorr = 0;
    
    // ADAPTIVE THRESHOLD based on calibration
    int16_t adaptiveThreshold = THRESHOLD_Q8;
    if (isCalibrated) {
        // Set threshold to 70% of baseline correlation
        adaptiveThreshold = (baselineCorrelation * 7) / 10;
        if (adaptiveThreshold < 51) adaptiveThreshold = 51; // Min 0.2
    }
    
    for (size_t i = 0; i <= bufferSize - WINDOW_SIZE; i += STRIDE) {
        int16_t windowCorr = calculateCorrelationQ8(
            &pitchBuffer1[i], 
            &pitchBuffer2[i], 
            WINDOW_SIZE
        );
        
        sumCorr += windowCorr;
        windows++;
        
        // Use adaptive threshold
        if (windowCorr < adaptiveThreshold) {
            anomalousWindows++;
        }
        
        if (windowCorr < result.minWindowCorr) {
            result.minWindowCorr = windowCorr;
        }
    }
    
    result.anomalyRatio = (anomalousWindows * 100) / windows;
    
    // 3. ADJUSTED Detection Logic for your hardware
    
    // Use relative drop from baseline if calibrated
    if (isCalibrated) {
        int16_t dropFromBaseline = baselineCorrelation - result.globalCorr;
        int16_t dropPercentage = (dropFromBaseline * 100) / (baselineCorrelation + 1);
        
        // Attack if correlation drops >40% from baseline
        if (dropPercentage > 40) {
            result.attackDetected = true;
            result.confidence = dropPercentage;
        }
        // Or if anomaly ratio is very high (>60% instead of 20%)
        else if (result.anomalyRatio > 60) {
            result.attackDetected = true;
            result.confidence = result.anomalyRatio;
        }
        // Or if minimum correlation is extremely low
        else if (result.minWindowCorr < 26) { // 0.1
            result.attackDetected = true;
            result.confidence = 50;
        }
    } else {
        // Fallback: Use fixed thresholds but much more lenient
        if (result.globalCorr < GLOBAL_THRESH) { // 0.3
            result.attackDetected = true;
            result.confidence = 100 - (result.globalCorr * 100 / 256);
        }
        else if (result.anomalyRatio > 80) { // Very high threshold
            result.attackDetected = true;
            result.confidence = result.anomalyRatio / 2;
        }
    }
    
    // Boost confidence only for extreme cases
    if (result.attackDetected && result.globalCorr < 26) { // <0.1
        result.confidence = 100;
    }
    
    return result;
}

// IMPROVED correlation calculation with DC removal
int16_t LaserAttackDetector::calculateCorrelationQ8(int16_t* buf1, int16_t* buf2, int size) {
    // First pass: calculate means (DC offset)
    int32_t mean1 = 0, mean2 = 0;
    for (int i = 0; i < size; i++) {
        mean1 += buf1[i];
        mean2 += buf2[i];
    }
    mean1 /= size;
    mean2 /= size;
    
    // Second pass: calculate correlation with DC removed
    int64_t sum11 = 0, sum22 = 0, sum12 = 0;
    
    for (int i = 0; i < size; i++) {
        int32_t v1 = buf1[i] - mean1;  // Remove DC
        int32_t v2 = buf2[i] - mean2;  // Remove DC
        
        sum11 += v1 * v1;
        sum22 += v2 * v2;
        sum12 += v1 * v2;
    }
    
    // Avoid division by zero
    if (sum11 <= 0 || sum22 <= 0) {
        return 0;
    }
    
    // Simple integer square root approximation
    // sqrt(a*b) ≈ (a+b)/2 when a≈b
    int64_t denomApprox = (sum11 + sum22) / 2;
    if (denomApprox == 0) return 0;
    
    // Calculate correlation in Q8 format
    int16_t correlation = (sum12 * 256) / denomApprox;
    
    // Clamp to valid range
    if (correlation > 256) correlation = 256;
    if (correlation < 0) correlation = 0;  // Use 0 instead of abs()
    
    return correlation;
}

void LaserAttackDetector::printResults(const DetectionResult& result) {
    Serial.println("\n=== LASER ATTACK DETECTION ===");
    Serial.print("Global Correlation: ");
    Serial.print(result.globalCorr * 100 / 256);
    Serial.println("%");
    
    if (isCalibrated) {
        Serial.print("(Baseline: ");
        Serial.print(baselineCorrelation * 100 / 256);
        Serial.println("%)");
        
        int16_t drop = ((baselineCorrelation - result.globalCorr) * 100) / (baselineCorrelation + 1);
        Serial.print("Drop from baseline: ");
        Serial.print(drop);
        Serial.println("%");
    }
    
    Serial.print("Anomalous Windows: ");
    Serial.print(result.anomalyRatio);
    Serial.println("%");
    
    Serial.print("Min Window Corr: ");
    Serial.print(result.minWindowCorr * 100 / 256);
    Serial.println("%");
    
    if (result.attackDetected) {
        Serial.println("\n⚠️  ATTACK DETECTED!");
        Serial.print("Confidence: ");
        Serial.print(result.confidence);
        Serial.println("%");
    } else {
        Serial.println("\n✅ SECURE - No attack detected");
    }
}