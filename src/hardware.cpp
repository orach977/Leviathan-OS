/*
 * ======================================================================================
 * FILE: hardware.cpp
 * DESCRIPTION: Implementation of Hardware HAL with Safety Checks.
 * ======================================================================================
 */

#include "hardware.h"

Hardware& Hardware::getInstance() {
    static Hardware instance;
    return instance;
}

Hardware::Hardware() 
    : display(SCREEN_W, SCREEN_H, &Wire, -1), 
      radio(PIN_NRF_CE, PIN_NRF_CSN), 
      lastInputTime(0) 
{
}

void Hardware::init() {
    initGPIO();

    if (!Wire.begin(PIN_SDA, PIN_SCL)) {
        if (ENABLE_SERIAL_LOG) Serial.println("[HW-ERR] I2C Bus Critical Failure!");
    }

    initDisplay();
    initRadio();

    if (!prefs.begin("leviathan", false)) { 
        if (ENABLE_SERIAL_LOG) Serial.println("[HW-ERR] NVS Mount Failed!");
        for(int i=0; i<5; i++) {
             digitalWrite(PIN_LED_R, HIGH); delay(50);
             digitalWrite(PIN_LED_R, LOW); delay(50);
        }
    }
    
    if (ENABLE_SERIAL_LOG) Serial.println("[HW-INIT] Hardware Layer Ready");
}

void Hardware::initGPIO() {
    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);
    pinMode(PIN_BTN_C, INPUT_PULLUP);
    pinMode(PIN_BTN_D, INPUT_PULLUP);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    digitalWrite(PIN_LED_R, LOW);
    digitalWrite(PIN_LED_G, LOW);
}

void Hardware::initDisplay() {
    bool displayReady = false;
    int retries = 3;

    while (retries > 0 && !displayReady) {
        if(display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
            displayReady = true;
        } else {
            retries--;
            if (ENABLE_SERIAL_LOG) Serial.println("[HW-WARN] Display Init Retry...");
            delay(100); 
        }
    }

    if(!displayReady) {
        if (ENABLE_SERIAL_LOG) Serial.println("[HW-ERR] Display Init Failed!");
        for(int i=0; i<3; i++) {
            digitalWrite(PIN_LED_R, HIGH); delay(200);
            digitalWrite(PIN_LED_R, LOW); delay(200);
        }
    } else {
        display.clearDisplay();
        display.setTextColor(WHITE);
        display.setTextSize(1);
        display.display();
    }
}

void Hardware::initRadio() {
    if(radio.begin()) {
        radio.setPALevel(RF24_PA_MAX);
        radio.setDataRate(RF24_2MBPS);
        radio.setAutoAck(false); 
    } else {
        if (ENABLE_SERIAL_LOG) Serial.println("[HW-ERR] NRF24 Radio Not Found!");
    }
}

void Hardware::update() {
}

Adafruit_SSD1306& Hardware::getDisplay() { return display; }

void Hardware::drawHeader(const char* title, bool active) {
    display.setCursor(0,0);
    display.print(title);
    if (active) {
        display.setCursor(110, 0);
        display.print("[!]");
    }
    display.drawLine(0, 8, 128, 8, WHITE);
}

int Hardware::getKey() {
    if (millis() - lastInputTime < 150) return 0;
    int key = 0;
    if(!digitalRead(PIN_BTN_A)) key = 1;
    else if(!digitalRead(PIN_BTN_B)) key = 2;
    else if(!digitalRead(PIN_BTN_C)) key = 3;
    else if(!digitalRead(PIN_BTN_D)) key = 4;
    if (key != 0) lastInputTime = millis();
    return key;
}

RF24& Hardware::getRadio() { return radio; }

void Hardware::scanSpectrum(uint8_t (&spectrum)[128]) {
    for(int i=0; i<128; i++) {
        radio.setChannel(i);
        radio.startListening();
        delayMicroseconds(120); 
        radio.stopListening();
        if(radio.testCarrier()) {
            if(spectrum[i] < 30) spectrum[i] += 2; 
        } else {
            if(spectrum[i] > 0) spectrum[i]--; 
        }
    }
}

void Hardware::jamFreq(int channel) {
    if (channel < 0) channel = 0;
    if (channel > 125) channel = 125;
    radio.setChannel(channel);
    radio.startConstCarrier(RF24_PA_MAX, channel);
}

void Hardware::setLed(bool state) {
    digitalWrite(PIN_LED_R, state ? HIGH : LOW);
}

void Hardware::saveSettings(const uint8_t (&bssid)[6], int ch) {
    size_t written = 0;
    written += prefs.putBytes("bssid", bssid, 6);
    written += prefs.putInt("ch", ch);
    if (written == 0 && ENABLE_SERIAL_LOG) Serial.println("[HW-ERR] NVS Write Failed!");
}

void Hardware::loadSettings(uint8_t (&bssid)[6], int* ch) {
    if (prefs.isKey("bssid")) {
        prefs.getBytes("bssid", bssid, 6);
    }
    *ch = prefs.getInt("ch", 1);
}

void Hardware::saveCred(const char* cred) {
    if (!cred) return;
    size_t len = strlen(cred);
    if (len == 0 || len >= MAX_INPUT_LEN) {
        if (ENABLE_SERIAL_LOG) Serial.println("[HW-WARN] Invalid Cred Length");
        return;
    }
    int count = prefs.getInt("cred_cnt", 0);
    if(count >= MAX_CREDS) return; 
    
    String key = "c" + String(count);
    prefs.putString(key.c_str(), cred);
    prefs.putInt("cred_cnt", count + 1);
}

std::vector<StoredCred> Hardware::loadCreds() {
    std::vector<StoredCred> creds;
    int count = prefs.getInt("cred_cnt", 0);
    if (count > MAX_CREDS) count = MAX_CREDS;

    for(int i=0; i<count; i++) {
        String key = "c" + String(i);
        String c = prefs.getString(key.c_str(), "");
        
        if(c.length() > 0 && c.length() < MAX_INPUT_LEN) {
            StoredCred sc;
            // [DRY] Using the helper logic embedded in StoredCred::set
            sc.set(c.c_str()); 
            creds.push_back(sc);
        }
    }
    return creds;
}

void Hardware::clearCreds() {
    prefs.putInt("cred_cnt", 0);
}
