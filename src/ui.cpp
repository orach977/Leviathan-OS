/*
 * ======================================================================================
 * FILE: ui.cpp
 * DESCRIPTION: UI Implementation adapted for Zero-Allocation Core & OLED 128x32.
 * ======================================================================================
 */

#include "ui.h"
#include "hardware.h"
#include "attacks.h"
#include "web_interface.h"

// [UX] Refresh Rate Limit (20 FPS)
#define UI_REFRESH_RATE_MS 50 

// Menu Options Arrays
const char* mainOpts[] = {"WIFI OPS", "BLE OPS", "RF24 OPS", "EVIL TWIN", "DEFENSE"};
const int mainOptsCount = 5;

const char* wifiOpts[] = {"SCAN TARGETS", "DEAUTH TGT", "BEACON FLOOD", "PROBE SNIFF", "BACK"};
const int wifiOptsCount = 5;

UI& UI::getInstance() {
    static UI instance;
    return instance;
}

UI::UI() : scanCount(0) {
    state.menuLvl = 0;
    state.cursor = 0;
    memset(scanResults, 0, sizeof(scanResults));
    memset(rfSpectrum, 0, sizeof(rfSpectrum)); 
}

void UI::init() {
    auto& disp = Hardware::getInstance().getDisplay();
    disp.clearDisplay();
    disp.setCursor(20, 10);
    disp.setTextSize(2);
    disp.print("LEVIATHAN");
    disp.setTextSize(1);
    disp.setCursor(35, 25);
    disp.print("MIL-STD v3.0");
    disp.display();
    delay(2000);
}

void UI::update() {
    // [UX] Frame Rate Limiting
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate < UI_REFRESH_RATE_MS) return;
    lastUpdate = millis();

    int key = Hardware::getInstance().getKey();
    if(key != 0) handleInput(key);
    
    auto& disp = Hardware::getInstance().getDisplay();
    disp.clearDisplay();
    
    bool active = AttackEngine::getInstance().isAttacking();
    
    // Dynamic Header
    char headerBuf[32];
    if (state.menuLvl == 0) safeStrCopy(headerBuf, "MAIN MENU", 32);
    else if (state.menuLvl == 10) {
        snprintf(headerBuf, 32, "SCAN: %d Found", (int)scanCount);
    } else {
        safeStrCopy(headerBuf, "OPERATIONS", 32);
    }

    Hardware::getInstance().drawHeader(headerBuf, active);

    // Spectrum Visualizer Override
    if (state.currentAttack == AttackType::RF_SCAN) {
        Hardware::getInstance().scanSpectrum(rfSpectrum);
        for(int i=0; i<128; i++) {
            if(rfSpectrum[i] > 0) disp.drawLine(i, 32, i, 32-rfSpectrum[i], WHITE);
        }
    } 
    else {
        switch(state.menuLvl) {
            case 0: renderMainMenu(); break;
            case 1: renderWiFiMenu(); break;
            case 10: renderScanList(); break;
            default: renderMainMenu(); break;
        }
    }
    
    disp.display();
}

// Helper to draw scrollbar for 128x32 layout
// Top offset: 9px (Header is 8px + 1px spacing)
// Height available: 23px
void drawScrollbar(int totalItems, int cursor) {
    auto& disp = Hardware::getInstance().getDisplay();
    if (totalItems <= 3) return; // No scroll needed

    int viewHeight = 23;
    int scrollTrackX = 126;
    int barHeight = max(5, viewHeight / ((totalItems + 2) / 3)); // Min 5px bar
    int scrollY = 9 + ((cursor / 3) * (viewHeight - barHeight) / max(1, (totalItems-1)/3));

    // Draw Track
    disp.drawFastVLine(scrollTrackX + 1, 9, viewHeight, WHITE);
    // Draw Thumb
    disp.fillRect(scrollTrackX, scrollY, 2, barHeight, WHITE);
}

void UI::renderMainMenu() {
    auto& disp = Hardware::getInstance().getDisplay();
    for(int i=0; i<3; i++) {
        int idx = (state.cursor/3)*3 + i;
        if(idx >= mainOptsCount) break;
        
        // Layout: Y=9, Y=17, Y=25 
        int yPos = 9 + (i * 8);
        disp.setCursor(0, yPos);
        
        if(idx == state.cursor) disp.print(">");
        else disp.print(" ");
        
        disp.print(mainOpts[idx]);
    }
    drawScrollbar(mainOptsCount, state.cursor);
}

void UI::renderWiFiMenu() {
    auto& disp = Hardware::getInstance().getDisplay();
    for(int i=0; i<3; i++) {
        int idx = (state.cursor/3)*3 + i;
        if(idx >= wifiOptsCount) break;
        
        int yPos = 9 + (i * 8);
        disp.setCursor(0, yPos);
        
        if(idx == state.cursor) disp.print(">");
        else disp.print(" ");
        
        disp.print(wifiOpts[idx]);
    }
    drawScrollbar(wifiOptsCount, state.cursor);
}

void UI::renderScanList() {
    auto& disp = Hardware::getInstance().getDisplay();
    if(scanCount == 0) {
        disp.setCursor(0,15); disp.print("NO TARGETS FOUND");
        return;
    }
    
    // Scrollable List
    int startIdx = (state.cursor / 3) * 3;
    
    for(int i=0; i<3; i++) {
        int idx = startIdx + i;
        if(idx >= (int)scanCount) break;
        
        APInfo& ap = scanResults[idx];
        
        int yPos = 9 + (i * 8);
        disp.setCursor(0, yPos);
        
        if(idx == state.cursor) disp.print(">");
        else disp.print(" ");
        
    
        char ssidBuf[16];
        snprintf(ssidBuf, sizeof(ssidBuf), "%-14.14s", ap.ssid);
        disp.print(ssidBuf);
        
        // Draw RSSI right-aligned
        disp.setCursor(100, yPos);
        disp.print(ap.rssi);
    }
    drawScrollbar((int)scanCount, state.cursor);
}

void UI::handleInput(int key) {
    // 1=A(Sel), 2=B(Back), 3=C(Up), 4=D(Down)
    
    if(key == 2) { // Back / Stop
        if(AttackEngine::getInstance().isAttacking()) {
            AttackEngine::getInstance().stopAttack();
            state.currentAttack = AttackType::NONE;
        } else if(state.menuLvl > 0) {
            state.menuLvl = 0;
            state.cursor = 0;
        }
        return;
    }
    
    if(key == 3) state.cursor--;
    if(key == 4) state.cursor++;
    
    // Bounds Checking
    int max = 0;
    if(state.menuLvl == 0) max = mainOptsCount - 1;
    if(state.menuLvl == 1) max = wifiOptsCount - 1;
    if(state.menuLvl == 10) max = (scanCount > 0) ? (int)scanCount - 1 : 0;
    
    if(state.cursor < 0) state.cursor = 0;
    if(state.cursor > max) state.cursor = max;
    
    if(key == 1) { // Select
        executeAction(state.cursor);
    }
}

void UI::executeAction(int index) {
    if(state.menuLvl == 0) {
        if(index == 0) { state.menuLvl = 1; state.cursor = 0; } // WiFi
        else if(index == 3) { // Evil Twin
            WebInterface::getInstance().start(true);
            state.currentAttack = AttackType::EVIL_TWIN;
            AttackEngine::getInstance().setAttack(AttackType::EVIL_TWIN);
        }
    }
    else if(state.menuLvl == 1) { // WiFi Menu
        if(index == 0) { // Scan
            Hardware::getInstance().drawHeader("SCANNING...", true);
            Hardware::getInstance().getDisplay().display();
            
            //  Zero-Allocation Scan Call
            scanCount = AttackEngine::getInstance().scanNetworks(scanResults, MAX_SCAN_RESULTS);
            
            //  Handle empty scan result
            if(scanCount == 0) {
                // Keep scanCount 0, renderScanList handles the "NO TARGETS" msg
            }
            
            state.menuLvl = 10; // View Results
            state.cursor = 0;
        }
        else if(index == 1) { // Deauth (Blind/Last Target)
             AttackEngine::getInstance().setAttack(AttackType::DEAUTH_TARGET);
        }
        else if(index == 3) { // Probe Sniff
             AttackEngine::getInstance().setAttack(AttackType::PROBE_SNIFF);
        }
    }
    else if(state.menuLvl == 10) { // Scan Results
        if(scanCount > 0) {
            APInfo& target = scanResults[index];
            AttackEngine::getInstance().setTarget(target.bssid, target.ch);
            
            // Auto-return to WiFi menu ready to attack
            state.menuLvl = 1;
            state.cursor = 1; // Highlight Deauth
        }
    }
}
