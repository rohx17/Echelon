// ============================================================================
// motor.cpp
// ============================================================================

#include "motor.h"
#include "Arduino.h"


// =========================
//  Pill Dispenser Controller
// =========================

// ---- Ultrasonic sensor pins (HC-SR04) ----
const int TRIG_PIN = 8;
const int ECHO_PIN = 9;  // NOTE: through voltage divider if 5V

// Distance threshold for "user is near" (in cm)
const float DETECT_DISTANCE_CM = 40.0;


// ---- Stepper pins (28BYJ-48 via ULN2003) ----
const int IN1 = 17;
const int IN2 = 18;
const int IN3 = 8;
const int IN4 = 3;

// ---- Stepper state ----
int stepIndex = 0;  

// Each row: IN1, IN2, IN3, IN4
const int stepSequence[8][4] = {
{1, 0, 0, 0},
{1, 1, 0, 0},
{0, 1, 0, 0},
{0, 1, 1, 0},
{0, 0, 1, 0},
{0, 0, 1, 1},
{0, 0, 0, 1},
{1, 0, 0, 1}
};

// ---- Motion tuning ----
const int STEP_DELAY_MS     = 3;        
const int STEPS_PER_REV     = 4096;     
const int TOTAL_CHAMBERS    = 15;
const int STEPS_PER_CHAMBER = 2050;

const int EMPTY_CHAMBER = 14;

// Track used chambers by day (0..6 = Mon..Sun)
bool amUsed[7] = {false, false, false, false, false, false, false};
bool pmUsed[7] = {false, false, false, false, false, false, false};

// Track where the wheel is currently aligned (0..14)
int currentChamber = EMPTY_CHAMBER;


// =========================
//      HELPER FUNCTIONS
// =========================

int modInt(int a, int m) {
    int r = a % m;
    return (r < 0) ? r + m : r;
}

void setStep(int a, int b, int c, int d) {
    digitalWrite(IN1, a);
    digitalWrite(IN2, b);
    digitalWrite(IN3, c);
    digitalWrite(IN4, d);
}

void releaseMotor() {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
}

// Step the motor a given number of half-steps in a direction (+1 or -1)
void stepMotor(int stepsToTake, int direction) {
    if (direction == 0 || stepsToTake <= 0) return;

    int dir = (direction > 0) ? 1 : -1;

    for (int i = 0; i < stepsToTake; i++) {
        setStep(stepSequence[stepIndex][0],
                stepSequence[stepIndex][1],
                stepSequence[stepIndex][2],
                stepSequence[stepIndex][3]);

        stepIndex += dir;
        if (stepIndex >= 8) stepIndex = 0;
        if (stepIndex < 0)  stepIndex = 7;

        delay(STEP_DELAY_MS);
    }
}

// Move wheel so that targetChamber (0..14) is aligned
void moveToChamber(int targetChamber) {
    targetChamber = modInt(targetChamber, TOTAL_CHAMBERS);
    int diff = targetChamber - currentChamber;  

    diff = modInt(diff, TOTAL_CHAMBERS);   // normalize 0..14

    int cw  = diff;
    int ccw = diff - TOTAL_CHAMBERS;       // negative

    int chamberDelta = (abs(ccw) < abs(cw)) ? ccw : cw;
    int direction    = (chamberDelta >= 0) ? 1 : -1;
    int stepsToMove  = abs(chamberDelta) * STEPS_PER_CHAMBER;

    Serial.print("Moving from chamber ");
    Serial.print(currentChamber);
    Serial.print(" to ");
    Serial.print(targetChamber);
    Serial.print(" (delta = ");
    Serial.print(chamberDelta);
    Serial.print(", steps = ");
    Serial.print(stepsToMove);
    Serial.println(")");

    stepMotor(stepsToMove, direction);
    currentChamber = targetChamber;
}

// ---- Ultrasonic measurement ----
float getDistanceCm() {
    // Send 10 µs trigger pulse
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Measure echo pulse width (timeout 30 ms = 30000 µs)
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);

    if (duration == 0) {
        // No echo detected (out of range or bad reading)
        return -1.0;
    }

    // Speed of sound ~343 m/s => 0.0343 cm/µs
    float distance = (duration * 0.0343) / 2.0;
    return distance;
}

// One step helper (BACKWARD direction you like)
void stepOneChamberBackward(const char *label) {
    Serial.print(label);
    Serial.println(": stepping one chamber BACKWARD...");
    int target = currentChamber - 1;      
    moveToChamber(target);
}


// =========================
//    PILL & PROXIMITY LOGIC
// =========================

// Gated by proximity sensor: logs distance but does NOT hard-block while debugging
void handleMorningCommand() {
    float d = getDistanceCm();

    Serial.print("Proximity check for AM: ");
    Serial.print(d);
    Serial.println(" cm");

    // If sensor is giving nonsense, still allow movement but warn
    if (d < 0) {
        Serial.println("Warning: no echo from sensor, dispensing AM anyway (debug).");
    } else if (d >= DETECT_DISTANCE_CM) {
        Serial.println("User not close enough by threshold, but dispensing AM anyway (debug).");
    } else {
        Serial.println("User detected close enough, dispensing AM.");
    }

    stepOneChamberBackward("AM");
}

void handleNightCommand() {
    float d = getDistanceCm();

    Serial.print("Proximity check for PM: ");
    Serial.print(d);
    Serial.println(" cm");

    if (d < 0) {
        Serial.println("Warning: no echo from sensor, dispensing PM anyway (debug).");
    } else if (d >= DETECT_DISTANCE_CM) {
        Serial.println("User not close enough by threshold, but dispensing PM anyway (debug).");
    } else {
        Serial.println("User detected close enough, dispensing PM.");
    }

    stepOneChamberBackward("PM");
}

// Move to EMPTY/REFILL marker (chamber 14)
void moveToEmpty() {
    moveToChamber(EMPTY_CHAMBER);
    Serial.println("Moved to EMPTY chamber (refill marker).");
}

// Test helper: move exactly one chamber backward
void testOneChamberStep() {
    stepOneChamberBackward("TEST");
}

  // =========================
  //       ARDUINO CORE
  // =========================

void MOTOR_setup() {
    // Ultrasonic pins
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // Stepper pins
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);

    releaseMotor();

    Serial.println("15-chamber pill dispenser with proximity ready.");
    Serial.println("Commands:");
    Serial.println("  m = AM pill (one chamber backward, proximity logged)");
    Serial.println("  n = PM pill (one chamber backward, proximity logged)");
    Serial.println("  e = move to EMPTY chamber (index 14)");
    Serial.println("  t = test: move one chamber backward");


    moveToEmpty();
}

// void MOTOR_loop() {
//     if (Serial.available()) {
//         char c = Serial.read();

//         if (c == 'm') {
//         handleMorningCommand();
//         } else if (c == 'n') {
//         handleNightCommand();
//         } else if (c == 'e') {
//         moveToEmpty();
//     }

//     // Tiny delay
//     delay(10);
// }
