#pragma once
#include <Arduino.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

// #if defined(ESP32) || defined(LIBRETUYA)
// #include <WebServer.h>
// #elif defined(ESP8266)
// #include <ESP8266WebServer.h>
// #else
// #error Platform not supported
// #endif

#include "../WifiManager/WifiManager.h"
#include "../SocketClientDefs.h"
class WebserverManager
{

protected:
    WifiManager *_wifiManager;
// #if defined(ESP32) || defined(LIBRETUYA)
//     WebServer _server;
// #elif defined(ESP8266)
//     ESP8266WebServer _server;
// #else
// #error Platform not supported
// #endif
    AsyncWebServer _server;
    AsyncWebSocket _ws;
    JsonDocument _wsMessage;

    void _setupWebServer();
    void _handleRoot(AsyncWebServerRequest *request);
    void _handleWifiConnect(AsyncWebServerRequest *request);
    void _onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                    void *arg, uint8_t *data, size_t len);

    void _handleWSConnected();
    void _handleSubToDevice();
    void _handleAskStatus();
    void _handleCommand();
    void _handleEntityChanged();

    // void _handleWifiLeave();
    // void _handleWifiScan();

    SendStatusFunction _sendStatus;
    ReceivedCommandFunction _receivedCommand;
    EntityChangedFunction _entityChanged;
    ConnectedFunction _connected;

public:
    WebserverManager(WifiManager *wifiManager, SendStatusFunction sendStatus, ReceivedCommandFunction receivedCommand, EntityChangedFunction entityChanged);

    void publishStatus(String statusJson);
    void loop();
};
