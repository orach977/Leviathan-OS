/*
 * ======================================================================================
 * FILE: web_interface.h
 * DESCRIPTION: Web Interface Header.
 * ======================================================================================
 */

#pragma once
#include <WebServer.h>
#include <DNSServer.h>
#include "types.h"

class WebInterface {
public:
    static WebInterface& getInstance();
    WebInterface(const WebInterface&) = delete;           
    void operator=(const WebInterface&) = delete;         
    
    // Lifecycle
    void start(bool evilTwinMode);
    void stop();
    void update(); 

private:
    WebInterface(); 
    
    WebServer server;
    DNSServer dnsServer;
    bool isEvilTwin;

    // HTTP Request Handlers
    void handleRoot();
    void handleScan();
    void handleAttack();
    void handleStop();
    void handleStatus();
    void handleCaptivePortal();
};
