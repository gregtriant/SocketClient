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

/**
 * @brief Configuration structure for the SocketClient.
 * This structure holds various settings for the SocketClient, including device
 * settings, server settings, and function pointers for handling events.
 */
typedef struct {
    /* device settings */
    const char *name;     // name of the app
    const float version;  // version of the app
    const char *type;     // type of the device (e.g. ESP32, ESP8266)
    const int ledPin;     // pin for the LED (optional, can be -1 if not used)

    /* server settings */
    const char *host;       // host of the socket server
    const int port;         // port of the socket server
    const bool isSSL;       // is the socket server using SSL
    const char *token;      // token for authentication
    const bool handleWifi;  // the socket client will handle wifi connection

    /* functions */
    SendStatusFunction sendStatus;
    ReceivedCommandFunction receivedCommand;
    EntityChangedFunction entityChanged;
    ConnectedFunction connected;
} SocketClientConfig;

void SocketClient_webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

/**
 * @brief The SocketClient class provides functionality for connecting to a socket server
 */
class SocketClient {
    friend void SocketClient_webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

   protected:
    NVSManager *_nvsManager;
    WifiManager *_wifiManager;
    WebserverManager *_webserverManager;
    OTAManager *_otaManager;
    DynamicJsonDocument _doc;  //- used to store the JSON data

    // data
    float _version = 0.2;  // change
    const char *_deviceApp = DEFAULT_APP_NAME;
    const char *_deviceType = DEVICE_TYPE;

    const char *_token = "";
    const char *_socketHostURL = DEFAULT_HOST;
    int _port = DEFAULT_PORT;
    bool _isSSL;

    String _macAddress;
    String _localIP;
    bool _handleWifi = false;
    bool _led_state = false;
    uint64_t _led_blink_time = 0;  // used to turn led on and off

    WebSocketsClient webSocket;
    SendStatusFunction sendStatus;
    ReceivedCommandFunction receivedCommand;
    EntityChangedFunction entityChanged;
    ConnectedFunction connected;

    void gotMessageSocket(uint8_t *payload);
    void _init();
    static bool watchdog(void *v);
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
    bool isConnected() { return webSocket.isConnected(); }
    void disconnect() { webSocket.disconnect(); }

    void init(const SocketClientConfig *config);
    void init(const char *socketHostURL, int port, bool _isSSL);
    void loop();

    // setters
    void setAppAndVersion(const char *deviceApp, float version) {
        this->_deviceApp = deviceApp;
        this->_version = version;
    }

    void setDeviceType(const char *deviceType) { this->_deviceType = deviceType; }

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
};
