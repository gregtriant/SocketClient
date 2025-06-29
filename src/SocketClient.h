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

#include "NVS/NVSManager.h"
#include "WifiManager/WifiManager.h"
#include "WebserverManager/WebserverManager.h"

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
  const bool handleWifi;  // the socket client will handle wifi connection

  /* functions */
  void (*sendStatus)(JsonDoc &doc);      // function to send status
  void (*receivedCommand)(JsonDoc &doc); // function to handle received commands
  void (*entityChanged)(JsonDoc &doc);   // function to handle entity changes
  void (*connected)(JsonDoc &doc);       // function to handle connection events
} SocketClientConfig;


void SocketClient_webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

/**
 * @brief The SocketClient class provides functionality for connecting to a socket server
 */
class SocketClient
{
  friend void SocketClient_webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

protected:
  NVSManager *_nvsManager;
  WifiManager *_wifiManager;
  WebserverManager *_webserverManager;

  // data
  float _version = 0.2; // change
  const char *_deviceApp = DEFAULT_APP_NAME;
  const char *_deviceType = DEVICE_TYPE;

  
  const char *_token = "";
  const char *_socketHostURL = DEFAULT_HOST;
  int _port = DEFAULT_PORT;
  bool _isSSL;

  // char _macAddress[20];
  String _macAddress;
  String _localIP;

  bool _handleWifi = false;
  // String _wifi_ssid;
  // String _wifi_password;
  // int _led_pin = -1; // led pin for indicating state
  // bool _wifi_connecting = false;
  bool _led_state = false;
  uint64_t _led_blink_time = 0; // used to turn led on and off

  // Wifi
  // uint32_t _connection_retries = 0;
  // uint64_t _ap_time = 0; // used to check how much time spent in AP mode

  // server for recieving wifi commands
#if defined(ESP32) || defined(LIBRETUYA)
  WebServer _server;
#elif defined(ESP8266)
  ESP8266WebServer _server;
#endif

  WebSocketsClient webSocket;
  SendStatusFunction sendStatus;
  ReceivedCommandFunction receivedCommand;
  EntityChangedFunction entityChanged;
  ConnectedFunction connected;

  // Sockets
  void gotMessageSocket(uint8_t *payload);

  // void _getWifiCredentialsFromNVS();
  // void _initAPMode();
  // void _wifiConnected();
  // void _setupWebServer();
  // void _handleRoot();
  // void _handleConnect();
  // void initWifi();
  // void initWifi(const char *ssid, const char *password);

  void _init();

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
  ~SocketClient();

  void reconnect();

  void init(const SocketClientConfig *config);

  void init(const char *socketHostURL, int port, bool _isSSL);

  void loop();

  // setters
  void setAppAndVersion(const char *deviceApp, float version)
  {
    this->_deviceApp = deviceApp;
    this->_version = version;
  }

  void setDeviceType(const char *deviceType)
  {
    this->_deviceType = deviceType;
  }

  void setSocketHost(const char *socketHostURL, int port, bool _isSSL)
  {
    this->_socketHostURL = socketHostURL;
    this->_port = port;
    this->_isSSL = _isSSL;
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
    this->_token = token;
  }

  // void setLedPin(int led_pin)
  // {
  //   _led_pin = led_pin;
  //   _led_state = false;
  //   pinMode(_led_pin, OUTPUT);
  //   digitalWrite(_led_pin, LOW); // turn off led
  // }

  // void setLedState(bool state)
  // {
  //   _led_state = state;
  //   if (_led_pin != -1)
  //   {
  //     digitalWrite(_led_pin, _led_state ? HIGH : LOW);
  //   }
  // }

  // void toggle_led_time(uint64_t now, int period, bool print_dot = false, bool count_retires = false) {
  //   // printf("Toggle LED time: %llu, period: %d, print_dot: %d, count_retires: %d\n", now, period, print_dot, count_retires);
  //   // printf("LEd pin: %d, led state: %d, blink time: %llu, connection retries: %d\n", _led_pin, _led_state, _led_blink_time, _connection_retries);
  //   if (now - _led_blink_time > (uint64_t)period && _led_pin != -1) {
  //     if (count_retires) {
  //       _connection_retries++;
  //     }
  //     if (print_dot) {
  //       Serial.print(".");
  //     }
  //     _led_blink_time = now;
  //     _led_state = 1 - _led_state;
  //     setLedState(_led_state);
  //   }
  // }
};
