#pragma once
#include <Arduino.h>

#if defined(ESP32) || defined(LIBRETUYA)
#include <WebServer.h>
#elif defined(ESP8266)
#include <ESP8266WebServer.h>
#else
#error Platform not supported
#endif

#include "../WifiManager/WifiManager.h"

class WebserverManager
{

protected:
    WifiManager *_wifiManager;
#if defined(ESP32) || defined(LIBRETUYA)
    WebServer _server;
#elif defined(ESP8266)
    ESP8266WebServer _server;
#else
#error Platform not supported
#endif

    void _setupWebServer();
    void _handleRoot();
    void _handleWifiConnect();
    // void _handleWifiLeave();
    // void _handleWifiScan();

public:
    WebserverManager(WifiManager *wifiManager);

    void loop();
};
