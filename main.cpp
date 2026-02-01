/*
 * ======================================================================================
 * DESCRIPTION: Main Entry Point with Fault-Tolerant RTOS Scheduling.
 * ======================================================================================
 */

#include "config.h"
#include "hardware.h"
#include "attacks.h"
#include "ui.h"
#include "web_interface.h"


TaskHandle_t attackTaskHandle = NULL;

// --- TASKS ---
void attackTask(void *parameter) {
    
    for(;;) {
      
        AttackEngine::getInstance().runLoop();
        

        vTaskDelay(10 / portTICK_PERIOD_MS);

        // [Debug] Check stack usage
        // uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        // if (uxHighWaterMark < 500) Serial.println("[WARN] Low Stack!");
    }
}

// --- STANDARD ARDUINO ---

void setup() {
    // 1. Initialize Hardware Abstraction Layer
    Hardware::getInstance().init();
    
    // 2. Initialize Engines
    AttackEngine::getInstance().init();
    UI::getInstance().init();
    
    // 3. Create High Priority Attack Task with Integrity Check
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
