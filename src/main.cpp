/*
 * ======================================================================================
 * DESCRIPTION: Main Entry Point with Fault-Tolerant RTOS Scheduling.
 * ======================================================================================
 */

#include <Arduino.h>
#include "config.h"
#include "hardware.h"
#include "attacks.h"
#include "ui.h"
#include "web_interface.h"
#include "nvs_flash.h" // Necessario per le funzioni nvs_

// --- GLOBALS ---
TaskHandle_t attackTaskHandle = NULL;

// --- TASKS ---
void attackTask(void *parameter) {
    for(;;) {
        AttackEngine::getInstance().runLoop();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

// --- SINGLE CONSOLIDATED SETUP ---
void setup() {
    // 0. Serial Init
    Serial.begin(115200);
    delay(2000); 
    Serial.println("\n--- [ LEVIATHAN OS v3.0 BOOT ] ---");

    // 1. Inizializza NVS (Fondamentale per la stabilità)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.println("Formattazione NVS in corso...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    Serial.println("Memoria NVS Pronta.");

    // 2. Initialize Hardware Abstraction Layer
    Hardware::getInstance().init();
    
    // 3. Initialize Engines
    AttackEngine::getInstance().init();
    UI::getInstance().init();
    
    // 4. Create High Priority Attack Task with Integrity Check
    BaseType_t result = xTaskCreate(
        attackTask,       
        "AttackCore",     
        ATTACK_TASK_STACK,
        NULL,             
        ATTACK_TASK_PRIO, 
        &attackTaskHandle 
    );

    // Critical Failure Detection
    if (result != pdPASS || attackTaskHandle == NULL) {
        if (ENABLE_SERIAL_LOG) Serial.println("[CRITICAL] Failed to create Attack Task!");
        
        // Fault State Indication (Infinite Blink)
        while(1) {
            Hardware::getInstance().setLed(true);
            delay(100);
            Hardware::getInstance().setLed(false);
            delay(100);
        }
    }
    
    if (ENABLE_SERIAL_LOG) Serial.println("[BOOT] Leviathan OS v3.0 Ready (RTOS OK)");
}

void loop() {
    // Main loop handles UI and Web 
    UI::getInstance().update();
    WebInterface::getInstance().update();
    
    vTaskDelay(5 / portTICK_PERIOD_MS);
}