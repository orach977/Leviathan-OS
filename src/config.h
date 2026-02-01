/*
 * ======================================================================================
 * FILE: config.h
 * DESCRIPTION: Global Configuration Header with Safety and Security Settings.
 * ======================================================================================
 */

#pragma once

#include <Arduino.h>

// ======================================================================================
// 0. DEPLOYMENT PROFILE (OPERATIONAL CONTEXT)
// ======================================================================================
#define MODE_DEV            0   // Development: Logs enabled, weak security permitted
#define MODE_OPS            1   // Operations: Stealth, Hardened, Strict Checks

// [SELECTOR] Choose Operational Mode
#define SYSTEM_MODE         MODE_OPS 

// [SELECTOR] Resource Profile
#define PROFILE_STEALTH     0   // Low memory footprint, silent
#define PROFILE_PERFORMANCE 1   // Max buffers, aggressive

#define RESOURCE_PROFILE    PROFILE_PERFORMANCE

// ======================================================================================
// 1. HARDWARE PINOUT (PHYSICAL LAYER)
// ======================================================================================
#define PIN_SDA         0
#define PIN_SCL         1
#define PIN_BTN_A       9
#define PIN_BTN_B       10
#define PIN_BTN_C       20
#define PIN_BTN_D       21
#define PIN_LED_R       5
#define PIN_LED_G       6
#define PIN_NRF_CE      8
#define PIN_NRF_CSN     7

// --- MATRIX PIN INTEGRITY CHECKS  ---
#if (PIN_SDA == PIN_SCL) || (PIN_SDA == PIN_LED_R) || (PIN_SDA == PIN_NRF_CE)
    #error "[HW-CRITICAL] PIN_SDA Conflict detected."
#endif
#if (PIN_SCL == PIN_LED_R) || (PIN_SCL == PIN_NRF_CE)
    #error "[HW-CRITICAL] PIN_SCL Conflict detected."
#endif
#if (PIN_NRF_CE == PIN_NRF_CSN)
    #error "[HW-CRITICAL] SPI Radio Control Pin Conflict (CE == CSN)."
#endif
#if defined(PIN_SDA) && (PIN_SDA < 0 || PIN_SDA > 21)
    #error "[HW-CRITICAL] PIN_SDA out of valid ESP32-C3 GPIO range."
#endif

// ======================================================================================
// 2. DISPLAY CONFIG 
// ======================================================================================
#define SCREEN_W        128
#define SCREEN_H        32
#define OLED_ADDR       0x3C
#define OLED_CONTRAST   0xFF

// ======================================================================================
// 3. SYSTEM SECURITY & INTEGRITY
// ======================================================================================
#define SAFE_MODE             false      // Hardware Safety Interlock
#define WATCHDOG_TIMEOUT_MS   5000       // WDT Reset threshold
#define NVS_MAGIC_KEY         0x4C563231 // "LV21" Signature
#define MIN_RSSI_THRESHOLD    -85        // Target viability floor

// Buffer Sizes based on Profile
#if RESOURCE_PROFILE == PROFILE_PERFORMANCE
    #define MAX_INPUT_LEN     128
    #define MAX_LOGS          100
    #define MAX_CREDS         50
    #define MAX_SCAN_RESULTS  50
#else // STEALTH
    #define MAX_INPUT_LEN     64
    #define MAX_LOGS          20
    #define MAX_CREDS         10
    #define MAX_SCAN_RESULTS  15
#endif

// --- INTEGRITY VALIDATION ---
#ifndef NVS_MAGIC_KEY
    #error "[SEC-CRITICAL] NVS_MAGIC_KEY undefined. Storage unsafe."
#endif

// ======================================================================================
// 4. NETWORK & WEB INTERFACE 
// ======================================================================================
#define WIFI_SSID_AP          "LEVIATHAN_NET"

// Security Configuration
#define USE_DYNAMIC_AUTH      true       // Salted Password Generation

// Static Password Configuration
#define WIFI_PASS_FALLBACK    "L3v!@th4n_X9z" // Stronger default

// [SEC-CRITICAL] PREVENT WEAK PASSWORD IN OPS MODE
#if (SYSTEM_MODE == MODE_OPS) && !USE_DYNAMIC_AUTH
    #if defined(WIFI_PASS_FALLBACK) && (WIFI_PASS_FALLBACK == "leviathan")
        #error "[OPSEC-FAIL] Cannot use default weak password 'leviathan' in OPS MODE. Change WIFI_PASS_FALLBACK."
    #endif
#endif

#define WEB_PORT              80
#define WEB_SESSION_TIMEOUT   300000     // 5 Minutes

// Client limits based on profile
#if RESOURCE_PROFILE == PROFILE_PERFORMANCE
    #define MAX_WEB_CLIENTS   4
#else
    #define MAX_WEB_CLIENTS   1
#endif

// ======================================================================================
// 5. LOGGING & MONITORING
// ======================================================================================
// Log Levels: 0=SILENT, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG

#if SYSTEM_MODE == MODE_OPS
    #define SYSTEM_LOG_LEVEL  1   // ERROR ONLY in Operations
    #define ENABLE_SERIAL_LOG false // Disable Serial (Silent)
#else
    #define SYSTEM_LOG_LEVEL  4   // DEBUG in Dev
    #define ENABLE_SERIAL_LOG true
#endif

// ======================================================================================
// 6. TACTICAL PARAMETERS 
// ======================================================================================
#define ATTACK_TASK_STACK     8192
#define ATTACK_TASK_PRIO      1

// Stack & Heap Safety
#if ATTACK_TASK_STACK < 4096
    #error "[MEM-CRITICAL] Attack Stack unsafe for TLS/WiFi stack."
#endif

// Timing Parameters
#define DEAUTH_PACKET_DELAY   10
#define DEAUTH_BURST_SIZE     5
#define CHANNEL_HOP_DELAY     150
#define JAMMER_HOP_SPEED      50
