// ============================================================================
// LcdTimeDisplay.cpp
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "LcdTimeDisplay.h"


const char* LcdTimeDisplay::STATUS_WIFI_CONNECTING = "WiFi Connect...";
const char* LcdTimeDisplay::STATUS_WIFI_CONNECTED = "WiFi OK";
const char* LcdTimeDisplay::STATUS_INITIALIZING = "Initializing...";
const char* LcdTimeDisplay::STATUS_WAITING = "Waiting...";
const char* LcdTimeDisplay::STATUS_DETECTED = "Wake Detected!";
const char* LcdTimeDisplay::STATUS_PROCESSING_WIT = "Processing...";
const char* LcdTimeDisplay::STATUS_INTENT_READY = "Intent Ready";
const char* LcdTimeDisplay::STATUS_LASER_CHECK = "Security Check";
const char* LcdTimeDisplay::STATUS_LASER_ALERT = "!LASER ATTACK!";

const char* LcdTimeDisplay::STATUS_MORNING_PILL = "Morning Pill";
const char* LcdTimeDisplay::STATUS_EVENING_PILL = "Evening Pill";
const char* LcdTimeDisplay::STATUS_VERIFYING = "Verifying...";
const char* LcdTimeDisplay::STATUS_HI_ROHIT = "Hi Rohit";
const char* LcdTimeDisplay::STATUS_HI_STRANGER = "Hi Stranger";
const char* LcdTimeDisplay::STATUS_SET_REMINDER = "Set Reminder";

LcdTimeDisplay::LcdTimeDisplay() {
    lcd = nullptr;
    ntpUDP = nullptr;
    timeClient = nullptr;
    lastTimeUpdate = 0;
    lastLcdUpdate = 0;
    currentStatus = "";
}

LcdTimeDisplay::~LcdTimeDisplay() {
    if (timeClient) {
        timeClient->end();
        delete timeClient;
    }
    if (ntpUDP) delete ntpUDP;
    if (lcd) delete lcd;
}

void LcdTimeDisplay::begin() {
    // Initialize LCD
    lcd = new LiquidCrystal(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
    lcd->begin(LCD_COLS, LCD_ROWS);
    
    // Clear and show welcome
    displayWelcomeMessage();
    
    // Initialize NTP Client
    ntpUDP = new WiFiUDP();
    timeClient = new NTPClient(*ntpUDP, NTP_SERVER, UTC_OFFSET_SEC);
}

void LcdTimeDisplay::displayWelcomeMessage() {
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print("Voice Assistant");
    lcd->setCursor(0, 1);
    lcd->print("Initializing...");
    delay(2000);
}

void LcdTimeDisplay::clearDisplay() {
    lcd->clear();
}

void LcdTimeDisplay::updateStatus(const char* status) {
    if (currentStatus != status) {
        currentStatus = status;
        
        // Clear first row and update status
        lcd->setCursor(0, 0);
        lcd->print("                "); // Clear the row
        lcd->setCursor(0, 0);
        
        // Truncate status if too long
        String displayStatus = String(status);
        if (displayStatus.length() > LCD_COLS) {
            displayStatus = displayStatus.substring(0, LCD_COLS);
        }
        lcd->print(displayStatus);
    }
}

void LcdTimeDisplay::forceTimeSync() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!timeClient->isTimeSet()) {
            timeClient->begin();
        }
        timeClient->forceUpdate();
        Serial.println("NTP Time synchronized");
    }
}

void LcdTimeDisplay::updateTime() {
    unsigned long currentMillis = millis();
    
    // Update NTP time periodically
    if (currentMillis - lastTimeUpdate >= NTP_UPDATE_INTERVAL) {
        if (WiFi.status() == WL_CONNECTED && timeClient) {
            timeClient->update();
            lastTimeUpdate = currentMillis;
        }
    }
    
    // Update LCD display every second
    if (currentMillis - lastLcdUpdate >= TIME_UPDATE_INTERVAL) {
        lastLcdUpdate = currentMillis;
        
        if (WiFi.status() == WL_CONNECTED && timeClient && timeClient->isTimeSet()) {
            // Get hours, minutes, seconds
            int hours = timeClient->getHours();
            int minutes = timeClient->getMinutes();
            int seconds = timeClient->getSeconds();
            
            // Get day of week
            int day = timeClient->getDay();
            const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            
            // Convert to 12-hour format
            bool isPM = false;
            if (hours >= 12) {
                isPM = true;
                if (hours > 12) {
                    hours -= 12;
                }
            }
            if (hours == 0) {
                hours = 12;  // Midnight case
            }
            
            // Format time string with leading zeros
            char timeBuffer[12];
            sprintf(timeBuffer, "%2d:%02d:%02d %s", 
                    hours, minutes, seconds, 
                    isPM ? "PM" : "AM");
            
            // Format: "Mon 12:34:56 PM"
            String displayTime = String(days[day]) + " " + String(timeBuffer);
            
            // Update second row with time
            lcd->setCursor(0, 1);
            lcd->print("                "); // Clear the row
            lcd->setCursor(0, 1);
            
            // Center the time display if it's shorter than LCD width
            int padding = (LCD_COLS - displayTime.length()) / 2;
            if (padding > 0) {
                lcd->setCursor(padding, 1);
            }
            
            lcd->print(displayTime);
        } else {
            // Show waiting for time sync
            lcd->setCursor(0, 1);
            lcd->print("Time: Syncing...");
        }
    }
}