// ============================================================================
// WitAiProcess.h - Process Wit AI 
// ============================================================================
#ifndef WIT_AI_PROCESS_H
#define WIT_AI_PROCESS_H

extern int16_t* ringBuffer1;
extern volatile bool witBuffersAllocated;

bool WIT_loop(); 
void WIT_acknowledgeData(); 
void startRecording_wit();

#endif