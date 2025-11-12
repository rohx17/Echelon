// ============================================================================
// WitAiProcess.h - Process Wit AI 
// ============================================================================
#ifndef WIT_AI_PROCESS_H
#define WIT_AI_PROCESS_H

extern int16_t* ringBuffer1;
extern volatile bool witBuffersAllocated;

enum ProcessStates{
    SET_REMINDER,       //LCD display - Connecting ... / Connected
    VERIFY_ME,          //LCD display  - Initializing Recording
    MORNING_PILL,       //LCD display  - Waiting... / Detected
    NIGHT_PILL,         //LCD display  - POST WIT / 
};

extern ProcessStates p_states;

bool WIT_loop(); 
void WIT_acknowledgeData(); 
void startRecording_wit();

#endif