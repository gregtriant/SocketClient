#pragma once
#include <Arduino.h>

#include "WebserverManager/WebserverManager.h"
#include "WifiManager/WifiManager.h"

#include <ESPAsyncWebServer.h>

#if defined(ESP32) || defined(LIBRETUYA)
#include <WiFi.h>
#include <Update.h>

#elif defined(ESP8266)
#include <ESP8266WiFi.h>
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
    AsyncWebServer _server;

    void _setupWebServer();
    // Restricts reboot, WiFi connect and WiFi scan to clients on the device's own local
    // network (see WifiManager::isLocalAddress) - a device that can merely route a request to
    // this server (e.g. via port forwarding or a proxy) must not be able to reboot it, change
    // its WiFi credentials or trigger a scan. Fails closed (denies) if WifiManager is unavailable.
    bool _isLocalRequest(AsyncWebServerRequest *request);
    void _handleRoot(AsyncWebServerRequest *request);
    void _sendPage(AsyncWebServerRequest *request);
    void _sendRebootPage(AsyncWebServerRequest *request);
    void _sendUploadPage(AsyncWebServerRequest *request);
    void _sendWifiPage(AsyncWebServerRequest *request);
    void _handleWifiConnect(AsyncWebServerRequest *request);
    DeviceInfo_t *_deviceInfo;

    std::function<String()> _getCurrentStatus = nullptr;
    bool _started = false;
public:
    WebserverManager(int port, WifiManager *wifiManager, DeviceInfo_t *deviceInfo, std::function<String()> getCurrentStatus);

    void loop();

    AsyncWebServer* getServer() { return &_server; }
};
