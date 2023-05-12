#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#if defined(ESP32) || defined(LIBRETUYA)
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #include <WiFi.h>
  #include <WiFiMulti.h>
  #include <WiFiClientSecure.h>
  #include <WebSocketsClient.h>
  #include <HTTPClient.h>
  #include <Update.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #include <ESP8266WiFi.h>
  #include <ESP8266WiFiMulti.h>
  #include <ESP8266httpUpdate.h>
#else
  #error Platform not supported
#endif

#include <timer.h>

#define USE_SERIAL Serial

typedef std::function<String()> DataToSendFunction;
typedef std::function<void(String data)> RecievedDataFunction;

void SocketClient_webSocketEvent(WStype_t type, uint8_t * payload, size_t length);

class SocketClient {
  friend void SocketClient_webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
protected:
  // data
  float version = 0.2;                             // change
  const char *deviceApp = "Test1";
  const char *deviceType = 
  #if defined(ESP32) || defined(LIBRETUYA)
  "ESP32";
  #elif defined(ESP8266)
  "ESP8266";
  #else
  "UNKNOWN";
  #endif

  const char *socketHostURL = "sensordata.space";  // socket host  // change 192.168.0.87
  int port = 80; // socket port                    // change

  char macAddress[20];
  String localIP;
  #if defined(ESP32) || defined(LIBRETUYA)
  WiFiMulti wiFiMulti;
  #elif defined(ESP8266)
  ESP8266WiFiMulti wiFiMulti;
  #endif

  WebSocketsClient webSocket;
  DataToSendFunction defineDataToSend;
  RecievedDataFunction recievedData;

  // Sockets
  // void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
  void gotMessageSocket(uint8_t * payload);
  void sendDataWithSocket(DynamicJsonDocument doc);
  void getDataFromSocket(DynamicJsonDocument recievedDoc); // TODO

public:
  // OTA Functions // for esp8266
  void updatingMode(String updateURL);
  static void update_started();
  static void update_finished();
  static void update_progress(int cur, int total);
  static void update_error(int err);
  // OTA Functions // for esp32
  void checkUpdate(String host);
  void updateFirmware(uint8_t *data, size_t len);
  int totalLength;       //total size of firmware
  int currentLength = 0; //current size of written firmware
  
  protected:
  static void watchdog(void *v);
  static unsigned long last_dog;
  static const unsigned long tick_time = 6000;
  static const unsigned long watchdog_time = (5*tick_time/2);
  Timer timer;
  public:

  // public methods
  SocketClient();
  bool isSSL;

  void reconnect() { 
    if(isSSL)
      webSocket.beginSSL(socketHostURL, port, "/"); // server address, port and URL
    else
      webSocket.begin(socketHostURL, port, "/"); // server address, port and URL
  }

  void init(){
    init(isSSL);
  }

  void init(bool _isSSL) {
    isSSL = _isSSL;

    String mac = WiFi.macAddress();
    mac.toCharArray(macAddress, 50);
    // init local IP
    localIP = WiFi.localIP().toString();
    webSocket.onEvent(SocketClient_webSocketEvent);   // initialte our event handler
    // webSocket.setAuthorization("user", "Password"); // use HTTP Basic Authorization this is optional remove if not needed
    webSocket.setReconnectInterval(5000); // try ever 5000 again if connection has failed
    reconnect();
    timer.every(tick_time,watchdog,this);
  }

  void loop() {
    this->webSocket.loop();
    this->timer.update();
  }
  // setters
  void setAppAndVersion(const char * deviceApp, float version) {
    this->deviceApp = deviceApp;
    this->version = version;
  }
  void setDeviceType(const char * deviceType) {
    this->deviceType = deviceType;
  }
  void setSocketHost(const char * socketHostURL, int port, bool _isSSL) {
    this->socketHostURL = socketHostURL;
    this->port = port;
    this->isSSL = _isSSL;
  }
  void setDataToSendFunciton(DataToSendFunction func) {
    this->defineDataToSend = func;
  }
  void setRecievedDataFunciton(RecievedDataFunction func) {
    this->recievedData = func;
  }
};