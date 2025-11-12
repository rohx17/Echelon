// LcdTimeDisplay.h
#ifndef LCD_TIME_DISPLAY_H
#define LCD_TIME_DISPLAY_H

#include <Arduino.h>
#include <LiquidCrystal.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// LCD Pin definitions for ESP32-S3
#define LCD_RS  4
#define LCD_EN  5
#define LCD_D4  6
#define LCD_D5  7
#define LCD_D6  15
#define LCD_D7  16

// LCD dimensions
#define LCD_COLS 16
#define LCD_ROWS 2

// NTP Settings
#define NTP_SERVER "pool.ntp.org"
#define UTC_OFFSET_SEC -18000  // Change this to your timezone offset in seconds
// Examples: EST = -18000, PST = -28800, CET = 3600

class LcdTimeDisplay {
private:
    LiquidCrystal* lcd;
    WiFiUDP* ntpUDP;
    NTPClient* timeClient;
    
    unsigned long lastTimeUpdate;
    unsigned long lastLcdUpdate;
    const unsigned long TIME_UPDATE_INTERVAL = 1000;  // Update display every second
    const unsigned long NTP_UPDATE_INTERVAL = 3600000; // Sync with NTP every hour
    
    String currentStatus;
    
public:
    LcdTimeDisplay();
    ~LcdTimeDisplay();
    
    void begin();
    void updateStatus(const char* status);
    void updateTime();
    void forceTimeSync();
    void displayWelcomeMessage();
    void clearDisplay();
    
    // Status messages for different states
    static const char* STATUS_WIFI_CONNECTING;
    static const char* STATUS_WIFI_CONNECTED;
    static const char* STATUS_INITIALIZING;
    static const char* STATUS_WAITING;
    static const char* STATUS_DETECTED;
    static const char* STATUS_PROCESSING_WIT;
    static const char* STATUS_INTENT_READY;
    static const char* STATUS_LASER_CHECK;
    static const char* STATUS_LASER_ALERT;
    
    static const char* STATUS_MORNING_PILL;
    static const char* STATUS_EVENING_PILL;
    static const char* STATUS_VERIFYING;
    static const char* STATUS_HI_ROHIT;
    static const char* STATUS_HI_STRANGER;
    static const char* STATUS_SET_REMINDER;
};

    // Status messages for different states (declarations only)
    static const char* STATUS_WIFI_CONNECTING;
    static const char* STATUS_WIFI_CONNECTED;
    static const char* STATUS_INITIALIZING;
    static const char* STATUS_WAITING;
    static const char* STATUS_DETECTED;
    static const char* STATUS_PROCESSING_WIT;
    static const char* STATUS_INTENT_READY;
    static const char* STATUS_LASER_CHECK;
    static const char* STATUS_LASER_ALERT;

    static const char* STATUS_MORNING_PILL;
    static const char* STATUS_EVENING_PILL;
    static const char* STATUS_VERIFYING;
    static const char* STATUS_HI_ROHIT;
    static const char* STATUS_HI_STRANGER;
    static const char* STATUS_SET_REMINDER;

#endif // LCD_TIME_DISPLAY_H