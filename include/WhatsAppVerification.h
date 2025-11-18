// ============================================================================
// WhatsAppVerification.h - WhatsApp Verification via CallMeBot
// ============================================================================
#ifndef WHATSAPP_VERIFICATION_H
#define WHATSAPP_VERIFICATION_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

class WhatsAppVerification {
private:
    static const int CODE_LENGTH = 4;
    char verificationCode[CODE_LENGTH + 1];
    char enteredCode[CODE_LENGTH + 1];
    int codeEntryPos;
    unsigned long codeGeneratedTime;
    static const unsigned long CODE_TIMEOUT = 300000; // 5 minutes
    
    // CallMeBot credentials
    const char* phoneNumber;
    const char* apiKey;
    
    // Helper function for URL encoding
    String urlEncode(const String& str);
    
public:
    WhatsAppVerification();
    void init(const char* phone, const char* key);
    
    // Generate and send code
    bool generateAndSendCode();
    String generateRandomCode();
    bool sendWhatsAppMessage(const String& message);
    
    // Code entry management
    void resetCodeEntry();
    bool processCodeEntry(char digit);
    bool verifyCode();
    bool isCodeExpired();
    String getCodeDisplay();
    bool isCodeComplete();
};

#endif