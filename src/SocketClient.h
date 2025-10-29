#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <WiFiClientSecure.h>

#if defined(ESP32) || defined(LIBRETUYA)
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <WiFi.h>

#elif defined(ESP8266)
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <Preferences.h>

#else
#error Platform not supported
#endif

#include "NVS/NVSManager.h"
#include "OTA/OTAManager.h"
#include "SocketClientDefs.h"
#include "WebserverManager/WebserverManager.h"
#include "WifiManager/WifiManager.h"



void SocketClient_webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

/**
 * @brief The SocketClient class provides functionality for connecting to a socket server
 */
class SocketClient {
    friend void SocketClient_webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

   protected:
    WebSocketsClient *_webSocket;
    NVSManager *_nvsManager;
    WifiManager *_wifiManager;
    WebserverManager *_webserverManager;
    OTAManager *_otaManager;
    JsonDocument _doc;  //- used to store the JSON data

    // Data.
    float _version = 0.2;  // change
    const char *_deviceApp = DEFAULT_APP_NAME;
    const char *_deviceType = DEVICE_TYPE;

    const char *_token = "";
    const char *_socketHostURL = DEFAULT_HOST;
    int _port = DEFAULT_PORT;
    bool _isSSL;

    DeviceInfo_t _deviceInfo;

    String _macAddress;
    String _localIP;
    bool _handleWifi = false;
    bool _led_state = false;
    uint64_t _led_blink_time = 0;  // used to turn led on and off

    uint32_t _local_time_offset = 0;
    String _local_time_zone = "";

    SendStatusFunction sendStatus;
    ReceivedCommandFunction receivedCommand;
    EntityChangedFunction entityChanged;
    ConnectedFunction connected;

    void gotMessageSocket(uint8_t *payload);
    void _init();
    // static bool watchdog(void *v);
    static unsigned long last_dog;
    static unsigned long last_png;
    static const unsigned long tick_time = 6000;
    static unsigned long last_reconnect;
    static unsigned long reconnect_time;                      //- 30 sec
    static const unsigned long max_reconnect_time = 600000L;  //- 10 min
    static const unsigned long watchdog_time = (5 * tick_time / 2);

public:
    SocketClient();
    ~SocketClient();

    void reconnect();
    void stopReconnect();
    void sendStatusWithSocket(bool save = false);  //- do the default (no receiverid)
    void sendLog(const String &message);
    void sendNotification(const String &message);
    void sendNotification(const String &message, const JsonDoc &data);
    bool isConnected() { return _webSocket->isConnected(); }
    void disconnect() { _webSocket->disconnect(); }

    void init(const SocketClientConfig_t *config);
    void init(const char *socketHostURL, int port, bool _isSSL);
    void loop();

    // Getters.
    String getCurrentStatus();
    String getVersion();

    // Setters.
    void setAppAndVersion(const char *deviceApp, float version) {
        this->_deviceApp = deviceApp;
        this->_version = version;

        // Just saving the name and version in deviceInfo.
        this->_deviceInfo.name = deviceApp;
        this->_deviceInfo.version = version;
    }

    void setDeviceType(const char *deviceType) {
        this->_deviceType = deviceType;

        // Just saving the type in deviceInfo.
        this->_deviceInfo.type = deviceType;
    }

    void setSocketHost(const char *socketHostURL, int port, bool _isSSL) {
        this->_socketHostURL = socketHostURL;
        this->_port = port;
        this->_isSSL = _isSSL;
    }

    void setSendStatusFunction(SendStatusFunction func) { this->sendStatus = func; }

    void setReceivedCommandFunction(ReceivedCommandFunction func) { this->receivedCommand = func; }

    void setEntityChangedFunction(EntityChangedFunction func) { this->entityChanged = func; }

    void setConnectedFunction(ConnectedFunction func) { this->connected = func; }

    void setToken(const char *token) { this->_token = token; }

    bool hasTime();
    void syncTime(bool toBegin = false);
    bool getTime(int *hh,int *mm,int *ss=nullptr);
    bool getDate(int *yy,int *mm,int *dd);
};
