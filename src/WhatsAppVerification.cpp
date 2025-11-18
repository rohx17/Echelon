// ============================================================================
// WhatsAppVerification.cpp - Implementation
// ============================================================================
#include "WhatsAppVerification.h"
#include "config.h"

WhatsAppVerification::WhatsAppVerification() {
    memset(verificationCode, 0, sizeof(verificationCode));
    memset(enteredCode, 0, sizeof(enteredCode));
    resetCodeEntry();
    codeGeneratedTime = 0;
}

void WhatsAppVerification::init(const char* phone, const char* key) {
    phoneNumber = phone;
    apiKey = key;
    Serial.println("[WhatsApp] Verification system initialized");
    Serial.printf("[WhatsApp] Phone: %s\n", phoneNumber);
}

String WhatsAppVerification::generateRandomCode() {
    String code = "";
    randomSeed(millis());
    for (int i = 0; i < CODE_LENGTH; i++) {
        code += String(random(0, 10));
    }
    strcpy(verificationCode, code.c_str());
    codeGeneratedTime = millis();
    Serial.printf("[WhatsApp] Generated verification code: %s\n", verificationCode);
    return code;
}

bool WhatsAppVerification::generateAndSendCode() {
    String code = generateRandomCode();
    
    // Format message with emojis for better visibility
    String message = "🔐 *Verification Code*\n\n";
    message += "Your code is: *" + code + "*\n\n";
    message += "⏱️ Valid for 5 minutes\n";
    message += "Enter via DTMF tones";
    
    return sendWhatsAppMessage(message);
}

bool WhatsAppVerification::sendWhatsAppMessage(const String& message) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure(); // CallMeBot uses HTTPS
    
    // Build CallMeBot URL
    String url = "https://api.callmebot.com/whatsapp.php";
    url += "?phone=" + String(phoneNumber);
    url += "&text=" + urlEncode(message);
    url += "&apikey=" + String(apiKey);
    
    Serial.println("[WhatsApp] Sending message via CallMeBot...");
    Serial.println("[WhatsApp] URL: " + url);
    
    http.begin(client, url);
    http.setTimeout(10000); // 10 second timeout
    
    int httpCode = http.GET();
    String response = http.getString();
    
    Serial.printf("[WhatsApp] HTTP Code: %d\n", httpCode);
    Serial.println("[WhatsApp] Response: " + response);
    
    http.end();
    
    if (httpCode == 200) {
        // CallMeBot returns "SUCCESS" in the response body when successful
        if (response.indexOf("SUCCESS") != -1 || response.indexOf("Message queued") != -1) {
            Serial.println("[WhatsApp] ✅ Message sent successfully!");
            return true;
        } else {
            Serial.println("[WhatsApp] ❌ Message failed - check API key");
            return false;
        }
    } else {
        Serial.printf("[WhatsApp] ❌ HTTP Error: %d\n", httpCode);
        return false;
    }
}

String WhatsAppVerification::urlEncode(const String& str) {
    String encoded = "";
    char c;
    char code0;
    char code1;
    
    for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (c == ' ') {
            encoded += '+';
        } else if (isalnum(c)) {
            encoded += c;
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) {
                code1 = (c & 0xf) - 10 + 'A';
            }
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) {
                code0 = c - 10 + 'A';
            }
            encoded += '%';
            encoded += code0;
            encoded += code1;
        }
    }
    return encoded;
}

void WhatsAppVerification::resetCodeEntry() {
    memset(enteredCode, '_', CODE_LENGTH);
    enteredCode[CODE_LENGTH] = '\0';
    codeEntryPos = 0;
}

bool WhatsAppVerification::processCodeEntry(char digit) {
    if (digit == 'C') {
        // Confirm
        if (isCodeComplete()) {
            return true;  // Ready to verify
        }
        Serial.println("[WhatsApp] Cannot confirm - code incomplete");
        return false;
    }
    else if (digit == 'D') {
        // Backspace
        if (codeEntryPos > 0) {
            codeEntryPos--;
            enteredCode[codeEntryPos] = '_';
            Serial.printf("[WhatsApp] Backspace - pos: %d\n", codeEntryPos);
        }
        return false;
    }
    else if (isdigit(digit) && codeEntryPos < CODE_LENGTH) {
        enteredCode[codeEntryPos] = digit;
        codeEntryPos++;
        Serial.printf("[WhatsApp] Added %c at position %d\n", digit, codeEntryPos-1);
        return false;
    }
    
    return false;
}

bool WhatsAppVerification::verifyCode() {
    if (isCodeExpired()) {
        Serial.println("[WhatsApp] Code has expired!");
        return false;
    }
    
    // Compare codes
    bool match = true;
    for (int i = 0; i < CODE_LENGTH; i++) {
        if (enteredCode[i] != verificationCode[i]) {
            match = false;
            break;
        }
    }
    
    if (match) {
        Serial.println("[WhatsApp] ✓ Verification successful!");
    } else {
        Serial.printf("[WhatsApp] ✗ Failed! Expected: %s, Got: %s\n", 
                     verificationCode, enteredCode);
    }
    
    return match;
}

bool WhatsAppVerification::isCodeExpired() {
    return (millis() - codeGeneratedTime) > CODE_TIMEOUT;
}

String WhatsAppVerification::getCodeDisplay() {
    return String(enteredCode);
}

bool WhatsAppVerification::isCodeComplete() {
    for (int i = 0; i < CODE_LENGTH; i++) {
        if (enteredCode[i] == '_') {
            return false;
        }
    }
    return true;
}