/*
 * ======================================================================================
 * FILE: attacks.h
 * DESCRIPTION: Core logic for WiFi and BLE attacks
 * ======================================================================================
 */

#pragma once

#include "config.h"
#include "types.h" 
#include <WiFi.h>
#include <freertos/semphr.h>
#include <freertos/queue.h> 
#include <cstdint> 

struct AttackLog {
    char data[64];
    unsigned long timestamp;
    AttackLog() : timestamp(0) { memset(data, 0, sizeof(data)); }
    void set(const char* msg) {
        safeStrCopy(data, msg, sizeof(data));
        timestamp = millis();
    }
};


struct PacketMsg {
    uint8_t type;         
    uint8_t payload[36]; 
    size_t len;
    int rssi;
};

class AttackEngine {
public:
    static AttackEngine& getInstance();
    AttackEngine(const AttackEngine&) = delete;            
    void operator=(const AttackEngine&) = delete;          

    // Lifecycle
    void init();
    
    // Control Interface
    void setAttack(AttackType type);
    void stopAttack();
    bool isAttacking() const;
    AttackType getCurrentAttackType() const;
    
    // Configuration
    void setTarget(const uint8_t (&bssid)[6], int channel);
    
    // Data Access (Deterministic / Zero-Allocation)
    size_t scanNetworks(APInfo* buffer, size_t maxCount);
    
    // Log Accessors - Populates external buffer
    size_t getHandshakes(AttackLog* buffer, size_t maxCount);
    size_t getProbes(AttackLog* buffer, size_t maxCount);
    
    int getDeauthCount();
    void clearLogs(); 
    
    // Core Logic (Called by FreeRTOS Task)
    void runLoop();
    
    // Sniffer Callback (Static ISR Context)
    static void snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type);

private:
    AttackEngine(); 
    
    // Concurrency & Safety
    SemaphoreHandle_t mutex;
    QueueHandle_t packetQueue; 
    
    // State
    AttackType currentAttack;
    bool active;
    uint8_t targetBSSID[6];
    int targetCh;
    
    AttackLog handshakeLogs[MAX_LOGS];
    size_t handshakeCount;
    
    AttackLog probeLogs[MAX_LOGS];
    size_t probeCount;
    
    volatile int deauthCounter;
    
    // Deterministic Channel Map
    static const uint8_t VALID_CHANNELS[13];

    // Internal Helpers
    void processPacketQueue();
    void logHandshake(const char* msg);
    void logProbe(const char* mac);

    // Internal Attack Vectors
    void sendDeauth();
    void sendBeacons(bool rickroll);
    void startBLE(int type);
    void stopBLE();
    
    // Packet construction
    uint8_t deauthPacket[26];
    void buildDeauthPacket();
    
    static AttackEngine* instance; 
};
