#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>

#if defined(ESP32) || defined(LIBRETUYA)
#include <WiFi.h>
#include <AsyncTCP.h>
// #include <WiFi.h>
// #include <WiFiMulti.h>
#include <HTTPClient.h>
#include <Update.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266httpUpdate.h>
#else
#error Platform not supported
#endif

//- notimer #include <arduino-timer.h>

#define USE_SERIAL Serial

class JsonDoc : public StaticJsonDocument<256>
{
public:
  JsonDoc()
  {
  }
};

typedef std::function<void(JsonDoc &doc)> SendStatusFunction;
typedef std::function<void(JsonDoc &doc)> ReceivedCommandFunction;
typedef std::function<void(JsonDoc &doc)> EntityChangedFunction;
typedef std::function<void(JsonDoc &doc)> ConnectedFunction;

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

  const char *token = "";
  const char *socketHostURL = "sensordata.space"; // socket host  // change 192.168.0.87
  int port = 80;                                  // socket port                    // change

  char macAddress[20];
  String localIP;
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

  void init();
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
};
