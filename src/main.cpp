/*
 * ======================================================================================
 * DESCRIPTION: Main Entry Point with Fault-Tolerant RTOS Scheduling.
 * ======================================================================================
 */

#include <Arduino.h>
#include <esp_task_wdt.h> 
#include "config.h"
#include "hardware.h"
#include "attacks.h"
#include "ui.h"
#include "web_interface.h"
#include "nvs_flash.h" 

// --- GLOBALS ---
TaskHandle_t attackTaskHandle = NULL;

// --- TASKS ---
void attackTask(void *parameter) {
    for(;;) {
        AttackEngine::getInstance().runLoop();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void setup() {
    // 0. Serial Init
    Serial.begin(115200);
    delay(2000); 
    Serial.println("\n--- [ Leviathan OS v 0.1.0 alpha BOOT ] ---");

    // 1. Initialize NVS Flash
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    Serial.println("nvs_flash initialized.");

    // 2. Initialize Hardware HAL
    Hardware::getInstance().init();

    // --- TEST LED ---
    Serial.println("loading test LEDs...");
  
    pinMode(3, OUTPUT); 
    pinMode(4, OUTPUT); 
    
    digitalWrite(3, HIGH);
    digitalWrite(4, HIGH);
    delay(2000); 
    digitalWrite(3, LOW);
    digitalWrite(4, LOW);
    // ----------------------------------------------

    // 3. Initialize Engines
    AttackEngine::getInstance().init();
    
    UI::getInstance().init();
    
    // 4. Create Attack Task
    BaseType_t result = xTaskCreate(
        attackTask, "AttackCore", ATTACK_TASK_STACK, NULL, ATTACK_TASK_PRIO, &attackTaskHandle 
    );

    if (result != pdPASS || attackTaskHandle == NULL) {
        while(1) {
            digitalWrite(3, HIGH); delay(100);
            digitalWrite(3, LOW);  delay(100);
        }
    }
    
    // arm the watchdog
  
    Serial.println("[WDT] Arming Watchdog System...");
    esp_task_wdt_init(WATCHDOG_TIMEOUT_MS / 1000, true); 
    esp_task_wdt_add(NULL); 
    
    if (ENABLE_SERIAL_LOG) Serial.println("[BOOT] Leviathan OS v 0.1.0 alpha (RTOS OK)");
}

void loop() {
    // Main loop handles UI and Web 
    UI::getInstance().update();
    WebInterface::getInstance().update();
    
    // Feed the dog (Reset timer)
    esp_task_wdt_reset(); 
    
    vTaskDelay(5 / portTICK_PERIOD_MS);
}