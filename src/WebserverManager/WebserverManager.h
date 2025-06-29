#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
#include "SocketClientDefs.h"

#if defined(ESP32) || defined(LIBRETUYA)
#include <WiFi.h>
#include <AsyncTCP.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <Preferences.h>
#include <ESP8266WebServer.h>
#else
#error Platform not supported
#endif

#include "../WifiManager/WifiManager.h"

class WebserverManager
{

protected:
    WifiManager *_wifiManager;
    WebServer _server;

    void _setupWebServer();
    void _handleRoot();
    void _handleWifiConnect();
    // void _handleWifiLeave();
    // void _handleWifiScan();

public:
    WebserverManager(WifiManager *wifiManager);

    void loop();
};
