// ============================================================================
// config.h - Configuration for WiFi and Wit.ai
// ============================================================================
#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID "RomseySt"
#define WIFI_PASSWORD "123AppleMango"

#define WHATSAPP_PHONE_NUMBER "+18577075157" 
#define WHATSAPP_API_KEY "3734632"  
#define VERIFICATION_CODE_TIMEOUT 300000
#define MAX_VERIFICATION_ATTEMPTS 3


// Wit.ai Configuration
// Get your token from https://wit.ai/apps
// 1. Create a new app on wit.ai
// 2. Go to Settings > API Details
// 3. Copy the Server Access Token
#define WIT_AI_TOKEN "FLBUXSTRXZN6ETX7TP3MOBYTNHIZDIJL"

// Optional: Audio feedback pins (if you have buzzer/LED)
// #define BUZZER_PIN 25
// #define LED_PIN 26

#endif // CONFIG_H