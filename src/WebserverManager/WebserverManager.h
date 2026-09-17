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

    // True once the network stack's tcpip task is safe to touch (AsyncWebServer::begin() calls
    // into lwIP and asserts - "Invalid mbox" - if that task isn't running yet). Driven by
    // WiFi.getMode() rather than _wifiManager's own state so it's correct regardless of who
    // brought WiFi up: SocketClient's own WifiManager (handleWifi=true), external code
    // (handleWifi=false), or AP+STA fallback with no real internet connection at all - the
    // /sc/wifi/connect provisioning page must be reachable in that last case too, so this
    // deliberately does NOT wait for an actual STA connection/IP.
    bool _networkStackReady();
    void _tryBeginServer();
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

    // SocketClient::initWebserver() can be (and is, in practice) called before SocketClient::init()
    // - at that point SocketClient's own WifiManager doesn't exist yet, so the constructor above
    // captures a null _wifiManager. SocketClient::init() calls this right after it creates the
    // real WifiManager, so /sc/wifi/connect etc. work regardless of call order.
    void setWifiManager(WifiManager *wifiManager) { _wifiManager = wifiManager; }
};
