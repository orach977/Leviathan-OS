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

const char* bleOpts[] = {"APPLE SOUR", "SAMSUNG", "WINDOWS", "GOOGLE", "BACK"};
const int bleOptsCount = 5;

const char* rf24Opts[] = {"SPECTRUM", "JAMMER", "CARRIER DETECT", "BACK"};
const int rf24OptsCount = 4;

const char* evilTwinOpts[] = {"START", "STOP", "VIEW CREDS", "BACK"};
const int evilTwinOptsCount = 4;

const char* defenseOpts[] = {"DEAUTH DETECT", "LOGS", "BACK"};
const int defenseOptsCount = 3;

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
    disp.print("v0.1.0-alpha");
    disp.display();
    delay(1500);

    const char* disclaimer[] = {
        "User assumes ALL risk", 
        "L'utente si assume",
        "OGNI rischio d'uso.",
        "Creator NOT liable",
        "L'Autore NON risponde",
        "di usi illeciti."
    };

    for(int p=0; p<2; p++) {
        disp.clearDisplay();
        disp.setTextSize(1);
        disp.setCursor(0, 0);
        disp.print(p == 0 ? "LEGAL TERMS:" : "LIABILITY:");
        disp.drawFastHLine(0, 8, 128, WHITE);
        disp.setCursor(0, 11); disp.print(disclaimer[p*3]);
        disp.setCursor(0, 19); disp.print(disclaimer[p*3 + 1]);
        disp.setCursor(0, 27); disp.print(disclaimer[p*3 + 2]);
        disp.display();
        delay(2500);
    }

    disp.clearDisplay();
    disp.setCursor(0, 0);
    disp.print("FINAL CONFIRM:");
    disp.drawFastHLine(0, 8, 128, WHITE);
    disp.setCursor(0, 12);
    disp.print("Proceed = I ACCEPT");
    disp.setCursor(0, 20);
    disp.print("Procedi = ACCETTO");
    disp.setCursor(0, 28);
    disp.print("PRESS [A] TO START");
    disp.display();
    
    while(true) {
        int key = Hardware::getInstance().getKey();
        if(key == 1) break; 
        delay(50); 
    }
    
    disp.clearDisplay();
    disp.display();
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
            case 2: renderBLEMenu(); break;
            case 3: renderRF24Menu(); break;
            case 4: renderEvilTwinMenu(); break;
            case 5: renderDefenseMenu(); break;
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

void UI::renderBLEMenu() {
    auto& disp = Hardware::getInstance().getDisplay();
    for(int i=0; i<3; i++) {
        int idx = (state.cursor/3)*3 + i;
        if(idx >= bleOptsCount) break;
        
        int yPos = 9 + (i * 8);
        disp.setCursor(0, yPos);
        
        if(idx == state.cursor) disp.print(">");
        else disp.print(" ");
        
        disp.print(bleOpts[idx]);
    }
    drawScrollbar(bleOptsCount, state.cursor);
}

void UI::renderRF24Menu() {
    auto& disp = Hardware::getInstance().getDisplay();
    for(int i=0; i<3; i++) {
        int idx = (state.cursor/3)*3 + i;
        if(idx >= rf24OptsCount) break;
        
        int yPos = 9 + (i * 8);
        disp.setCursor(0, yPos);
        
        if(idx == state.cursor) disp.print(">");
        else disp.print(" ");
        
        disp.print(rf24Opts[idx]);
    }
    drawScrollbar(rf24OptsCount, state.cursor);
}

void UI::renderEvilTwinMenu() {
    auto& disp = Hardware::getInstance().getDisplay();
    for(int i=0; i<3; i++) {
        int idx = (state.cursor/3)*3 + i;
        if(idx >= evilTwinOptsCount) break;
        
        int yPos = 9 + (i * 8);
        disp.setCursor(0, yPos);
        
        if(idx == state.cursor) disp.print(">");
        else disp.print(" ");
        
        disp.print(evilTwinOpts[idx]);
    }
    drawScrollbar(evilTwinOptsCount, state.cursor);
}

void UI::renderDefenseMenu() {
    auto& disp = Hardware::getInstance().getDisplay();
    for(int i=0; i<3; i++) {
        int idx = (state.cursor/3)*3 + i;
        if(idx >= defenseOptsCount) break;
        
        int yPos = 9 + (i * 8);
        disp.setCursor(0, yPos);
        
        if(idx == state.cursor) disp.print(">");
        else disp.print(" ");
        
        disp.print(defenseOpts[idx]);
    }
    drawScrollbar(defenseOptsCount, state.cursor);
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
    if(state.menuLvl == 2) max = bleOptsCount - 1;
    if(state.menuLvl == 3) max = rf24OptsCount - 1;
    if(state.menuLvl == 4) max = evilTwinOptsCount - 1;
    if(state.menuLvl == 5) max = defenseOptsCount - 1;
    if(state.menuLvl == 10) max = (scanCount > 0) ? (int)scanCount - 1 : 0;
    
    if(state.cursor < 0) state.cursor = 0;
    if(state.cursor > max) state.cursor = max;
    
    if(key == 1) { // Select
        executeAction(state.cursor);
    }
}

void UI::executeAction(int index) {
    if(state.menuLvl == 0) {
        if(index == 0) { state.menuLvl = 1; state.cursor = 0; }
        else if(index == 1) { state.menuLvl = 2; state.cursor = 0; }
        else if(index == 2) { state.menuLvl = 3; state.cursor = 0; }
        else if(index == 3) { state.menuLvl = 4; state.cursor = 0; }
        else if(index == 4) { state.menuLvl = 5; state.cursor = 0; }
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
    else if(state.menuLvl == 2) {
        if(index == 0) {
            state.currentAttack = AttackType::BLE_SOUR;
            AttackEngine::getInstance().setAttack(AttackType::BLE_SOUR);
        }
        else if(index == 1) {
            state.currentAttack = AttackType::BLE_SAMS;
            AttackEngine::getInstance().setAttack(AttackType::BLE_SAMS);
        }
        else if(index == 2) {
            state.currentAttack = AttackType::BLE_WIN;
            AttackEngine::getInstance().setAttack(AttackType::BLE_WIN);
        }
        else if(index == 3) {
            state.currentAttack = AttackType::BLE_GOOGLE;
            AttackEngine::getInstance().setAttack(AttackType::BLE_GOOGLE);
        }
    }
    else if(state.menuLvl == 3) {
        if(index == 0) {
            state.currentAttack = AttackType::RF_SCAN;
            AttackEngine::getInstance().setAttack(AttackType::RF_SCAN);
        }
        else if(index == 1) {
            state.currentAttack = AttackType::RF_JAM;
            AttackEngine::getInstance().setAttack(AttackType::RF_JAM);
        }
        else if(index == 2) {
            Hardware::getInstance().scanSpectrum(rfSpectrum);
        }
    }
    else if(state.menuLvl == 4) {
        if(index == 0) {
            WebInterface::getInstance().start(true);
            state.currentAttack = AttackType::EVIL_TWIN;
            AttackEngine::getInstance().setAttack(AttackType::EVIL_TWIN);
        }
        else if(index == 1) {
            WebInterface::getInstance().stop();
            AttackEngine::getInstance().stopAttack();
            state.currentAttack = AttackType::NONE;
        }
        else if(index == 2) {
            auto creds = Hardware::getInstance().loadCreds();
            if(creds.empty()) {
                scanCount = 0;
            } else {
                scanCount = creds.size();
                for(size_t i=0; i<creds.size() && i<MAX_SCAN_RESULTS; i++) {
                    safeStrCopy(scanResults[i].ssid, creds[i].data, sizeof(scanResults[i].ssid));
                }
            }
            state.menuLvl = 10;
            state.cursor = 0;
        }
    }
    else if(state.menuLvl == 5) {
        if(index == 0) {
            state.currentAttack = AttackType::DEAUTH_DETECT;
            AttackEngine::getInstance().setAttack(AttackType::DEAUTH_DETECT);
        }
        else if(index == 1) {
            int deauthCount = AttackEngine::getInstance().getDeauthCount();
            char logBuf[32];
            snprintf(logBuf, sizeof(logBuf), "DEAUTH: %d", deauthCount);
            auto& disp = Hardware::getInstance().getDisplay();
            disp.clearDisplay();
            disp.setCursor(0, 10);
            disp.print(logBuf);
            disp.display();
            delay(2000);
        }
    }
    else if(state.menuLvl == 10) {
        if(scanCount > 0) {
            APInfo& target = scanResults[index];
            AttackEngine::getInstance().setTarget(target.bssid, target.ch);
            state.menuLvl = 1;
            state.cursor = 1;
        }
    }
}
