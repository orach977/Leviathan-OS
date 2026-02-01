/*
 * ======================================================================================
 * FILE: hardware.h
 * DESCRIPTION: Abstraction layer for OLED, Buttons, NVS, and Radio.
 * ======================================================================================
 */

#pragma once

#include "config.h"
#include "types.h" 
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RF24.h>
#include <Preferences.h>
#include <vector>

class Hardware {
public:
    static Hardware& getInstance();
    Hardware(const Hardware&) = delete;            
    void operator=(const Hardware&) = delete;      

    void init();
    void update();
    
    // Display Access
    Adafruit_SSD1306& getDisplay();
    void drawHeader(const char* title, bool active);
    
    // Input Handling
    int getKey();
    
    // Radio Control
    RF24& getRadio();

    void scanSpectrum(uint8_t (&spectrum)[128]);
    void jamFreq(int channel);
    
    // Status Indicators
    void setLed(bool state);
    
    // NVS (Non-Volatile Storage)
    void saveSettings(const uint8_t (&bssid)[6], int ch);
    void loadSettings(uint8_t (&bssid)[6], int* ch);
    
    void saveCred(const char* cred);
    
    std::vector<StoredCred> loadCreds(); 
    void clearCreds();

private:
    Hardware();
    
    Adafruit_SSD1306 display;
    RF24 radio;
    Preferences prefs;
    unsigned long lastInputTime;
    
    void initDisplay();
    void initRadio();
    void initGPIO();
};
