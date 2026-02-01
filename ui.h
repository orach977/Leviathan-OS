/*
 * ======================================================================================
 * FILE: ui.h
 * DESCRIPTION: Menu system logic with Fixed-Memory Architecture.
 * ======================================================================================
 */

#pragma once
#include "types.h"
#include "config.h"

class UI {
public:
    static UI& getInstance();
    UI(const UI&) = delete;            
    void operator=(const UI&) = delete;      

    void init();
    void update(); 

private:
    UI(); 
    
    SystemState state;
    
  
    APInfo scanResults[MAX_SCAN_RESULTS];
    size_t scanCount; 

    uint8_t rfSpectrum[128];
    
    // Menus
    void renderMainMenu();
    void renderWiFiMenu();
    void renderScanList();
    
    // Actions
    void handleInput(int key);
    void executeAction(int index);
};
