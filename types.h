/*
 * ======================================================================================
 * FILE: types.h
 * DESCRIPTION: Common data structures, enumerations, and safety helpers.
 * ======================================================================================
 */

#pragma once
#include "config.h"
#include <Arduino.h>
#include <vector>
#include <cstring> 

// ======================================================================================
//  SAFETY HELPERS 
// ======================================================================================

inline void safeStrCopy(char* dest, const char* src, size_t destSize) {
    if (!dest || destSize == 0) return; 
    
    if (!src) {
        memset(dest, 0, destSize);
        return;
    }
    strncpy(dest, src, destSize - 1);
    dest[destSize - 1] = '\0';
}

// ======================================================================================
// DATA STRUCTURES
// ======================================================================================

// Attack Vectors Enumeration
enum class AttackType {
    NONE,
    DEAUTH_TARGET,
    BEACON_LIST,
    BEACON_RANDOM,
    PROBE_SNIFF,
    BLE_SOUR,
    BLE_SAMS,
    BLE_WIN,
    BLE_GOOGLE,
    WEB_SERVER,
    EVIL_TWIN,
    DEAUTH_DETECT,
    RF_SCAN,
    RF_JAM
};

//  Credential Container
struct StoredCred {
    char data[MAX_INPUT_LEN]; 
    
    StoredCred() { memset(data, 0, sizeof(data)); }
    
    // Helper to set data safely using the centralized logic
    void set(const char* str) {
        safeStrCopy(data, str, sizeof(data));
    }
};

// Access Point Structure
struct APInfo {
    char ssid[33]; 
    uint8_t bssid[6];
    int rssi;
    int ch;
    bool selected;

    APInfo() : rssi(0), ch(0), selected(false) {
        memset(ssid, 0, sizeof(ssid));
        memset(bssid, 0, sizeof(bssid));
    }

    bool isValid() const {
        for(int i=0; i<6; i++) if(bssid[i] != 0) return true;
        return false;
    }

    bool isSSIDMeaningful() const {
        if (ssid[0] == '\0') return false;
        bool hasVisibleContent = false;
        for(size_t i = 0; i < sizeof(ssid) && ssid[i] != '\0'; i++) {
            if (ssid[i] < 32 || ssid[i] > 126) return false; 
            if (ssid[i] != ' ') hasVisibleContent = true;
        }
        return hasVisibleContent;
    }

    void setSSID(const char* newSSID) {
        safeStrCopy(ssid, newSSID, sizeof(ssid));
    }

    void setSSID(const String& newSSID) {
        setSSID(newSSID.c_str());
    }

    String getBSSIDString() const {
        char buf[18];
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
            bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
        return String(buf);
    }
};

// System State
struct SystemState {
    int menuLvl = 0;
    int cursor = 0;
    bool attacking = false;
    AttackType currentAttack = AttackType::NONE;
    char statusMsg[32];
    uint8_t targetBSSID[6] = {0};
    int targetCh = 1;
    unsigned long uptime = 0;

    SystemState() {
        memset(statusMsg, 0, sizeof(statusMsg));
        setStatus("READY");
    }

    void setStatus(const char* msg) {
        safeStrCopy(statusMsg, msg, sizeof(statusMsg));
    }
};
