#pragma once
#include <Arduino.h>


#if defined(ESP32) || defined(LIBRETUYA)
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Updater.h>
#else
#error Platform not supported
#endif

#include "../WifiManager/WifiManager.h"
#include "../SocketClientDefs.h"

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
    DeviceInfo_t *_deviceInfo;

    std::function<String()> _getCurrentStatus = nullptr;
public:
    WebserverManager(WifiManager *wifiManager, DeviceInfo_t *deviceInfo, std::function<String()> getCurrentStatus);

    void loop();
};
