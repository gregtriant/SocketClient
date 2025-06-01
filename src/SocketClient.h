#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
#include "SocketClientDefs.h"

#if defined(ESP32) || defined(LIBRETUYA)
#include <WiFi.h>
#include <AsyncTCP.h>
// #include <WiFi.h>
// #include <WiFiMulti.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
// #include <ESP8266WiFiMulti.h>
#include <ESP8266httpUpdate.h>
#include <Preferences.h>
#include <ESP8266WebServer.h>
#else
#error Platform not supported
#endif


/**
 * @brief Configuration structure for the SocketClient.
 * This structure holds various settings for the SocketClient, including device
 * settings, server settings, and function pointers for handling events.
 */
typedef struct {
  /* device settings */
  const char *name;       // name of the app
  const float version;    // version of the app
  const char *type;       // type of the device (e.g. ESP32, ESP8266)
  const int ledPin;       // pin for the LED (optional, can be -1 if not used)
  
  /* server settings */
  const char *host;       // host of the socket server
  const int port;         // port of the socket server
  const bool isSSL;       // is the socket server using SSL
  const char *token;      // token for authentication

  /* functions */
  void (*sendStatus)(JsonDoc &doc);      // function to send status
  void (*receivedCommand)(JsonDoc &doc); // function to handle received commands
  void (*entityChanged)(JsonDoc &doc);   // function to handle entity changes
  void (*connected)(JsonDoc &doc);       // function to handle connection events
} SocketClientConfig;


void SocketClient_webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

class SocketClient
{
  friend void SocketClient_webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

protected:
  // data
  float version = 0.2; // change
  const char *deviceApp = "Test1";
  const char *deviceType =
#if defined(ESP32) || defined(LIBRETUYA)
      "ESP32";
#elif defined(ESP8266)
      "ESP8266";
#else
      "UNKNOWN";
#endif

  Preferences _wifi_preferences;
  const char *token = "";
  const char *socketHostURL = "sensordata.space"; // socket host  // change 192.168.0.87
  int port = 80;                                  // socket port                    // change

  char _macAddress[20];
  String _localIP;

  bool _handleWifi = false;
  String _wifi_ssid;
  String _wifi_password;
  int _led_pin = -1; // led pin for indicating state
  bool _wifi_connecting = false;
  bool _led_state = false;
  uint64_t _led_blink_time = 0; // used to turn led on and off
  uint32_t _connection_retries = 0;
  uint64_t _ap_time = 0; // used to check how much time spent in AP mode

  // server for recieving wifi commands
#if defined(ESP32) || defined(LIBRETUYA)
  WebServer _server;
#elif defined(ESP8266)
  ESP8266WebServer _server;
#endif
  // #if defined(ESP32) || defined(LIBRETUYA)
  // WiFiMulti wiFiMulti;
  // #elif defined(ESP8266)
  // ESP8266WiFiMulti wiFiMulti;
  // #endif

  WebSocketsClient webSocket;
  SendStatusFunction sendStatus;
  ReceivedCommandFunction receivedCommand;
  EntityChangedFunction entityChanged;
  ConnectedFunction connected;

  // Sockets
  // void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
  void gotMessageSocket(uint8_t *payload);

  // void sendStatusWithSocket(DynamicJsonDocument doc);
  // void getDataFromSocket(DynamicJsonDocument receivedDoc); // TODO
  void _getWifiCredentialsFromNVS();
  void _initAPMode();
  void _wifiConnected();
  void _setupWebServer();
  void _handleRoot();
  void _handleConnect();


public:
  void sendStatusWithSocket(bool save = false); //- do the default (no receiverid)
  void sendLog(const String &message);
  void sendNotification(const String &message);
  void sendNotification(const String &message, const JsonDoc &data);
  bool isConnected() { return webSocket.isConnected(); }
  void disconnect() { webSocket.disconnect(); }

  // OTA Functions // for esp8266
  void updatingMode(String updateURL);
  static void update_started();
  static void update_finished();
  static void update_progress(int cur, int total);
  static void update_error(int err);
  // OTA Functions // for esp32
  void checkUpdate(String host);
  void updateFirmware(uint8_t *data, size_t len);
  int totalLength;       // total size of firmware
  int currentLength = 0; // current size of written firmware

protected:
  static bool watchdog(void *v);
  static unsigned long last_dog;
  static unsigned long last_png;
  static const unsigned long tick_time = 6000;
  static unsigned long last_reconnect;
  static unsigned long reconnect_time;                     //- 30 sec
  static const unsigned long max_reconnect_time = 600000L; //- 10 min
  static const unsigned long watchdog_time = (5 * tick_time / 2);

  //- notimer Timer<1> timer;
public:
  // public methods
  SocketClient();
  bool isSSL;

  void reconnect();

  void initWifi();
  void initWifi(const char *ssid, const char *password);

  void init();
  
  void init(const SocketClientConfig *config)
  {
    RETURN_IF_NULLPTR(config);
    ASSIGN_IF_NOT_NULLPTR(deviceApp, config->name);
    ASSIGN_IF_NOT_NULLPTR(deviceType, config->type);
    ASSIGN_IF_NOT_NULLPTR(socketHostURL, config->host);
    ASSIGN_IF_NOT_NULLPTR(token, config->token);
    ASSIGN_IF_NOT_NULLPTR(sendStatus, config->sendStatus);
    ASSIGN_IF_NOT_NULLPTR(receivedCommand, config->receivedCommand);
    ASSIGN_IF_NOT_NULLPTR(entityChanged, config->entityChanged);
    ASSIGN_IF_NOT_NULLPTR(connected, config->connected);

    version = config->version;
    port = config->port;
    isSSL = config->isSSL;
    if (config->ledPin > 0) {
      setLedPin(config->ledPin);
    }
    init();
  }

  void init(const char *socketHostURL, int port, bool _isSSL)
  {
    setSocketHost(socketHostURL, port, _isSSL);
    init();
  }
  void loop();

  // setters
  void setAppAndVersion(const char *deviceApp, float version)
  {
    this->deviceApp = deviceApp;
    this->version = version;
  }

  void setDeviceType(const char *deviceType)
  {
    this->deviceType = deviceType;
  }

  void setSocketHost(const char *socketHostURL, int port, bool _isSSL)
  {
    this->socketHostURL = socketHostURL;
    this->port = port;
    this->isSSL = _isSSL;
  }

  void setSendStatusFunction(SendStatusFunction func)
  {
    this->sendStatus = func;
  }

  void setReceivedCommandFunction(ReceivedCommandFunction func)
  {
    this->receivedCommand = func;
  }

  void setEntityChangedFunction(EntityChangedFunction func)
  {
    this->entityChanged = func;
  }

  void setConnectedFunction(ConnectedFunction func)
  {
    this->connected = func;
  }

  void setToken(const char *token)
  {
    this->token = token;
  }

  void setLedPin(int led_pin)
  {
    _led_pin = led_pin;
    _led_state = false;
    pinMode(_led_pin, OUTPUT);
    digitalWrite(_led_pin, LOW); // turn off led
  }

  void setLedState(bool state)
  {
    _led_state = state;
    if (_led_pin != -1)
    {
      digitalWrite(_led_pin, _led_state ? HIGH : LOW);
    }
  }

  void toggle_led_time(uint64_t now, int period, bool print_dot = false, bool count_retires = false) {
    // printf("Toggle LED time: %llu, period: %d, print_dot: %d, count_retires: %d\n", now, period, print_dot, count_retires);
    // printf("LEd pin: %d, led state: %d, blink time: %llu, connection retries: %d\n", _led_pin, _led_state, _led_blink_time, _connection_retries);
    if (now - _led_blink_time > (uint64_t)period && _led_pin != -1) {
      if (count_retires) {
        _connection_retries++;
      }
      if (print_dot) {
        Serial.print(".");
      }
      _led_blink_time = now;
      _led_state = 1 - _led_state;
      setLedState(_led_state);
    }
  }
};
