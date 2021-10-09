#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266httpUpdate.h>

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
  const char *deviceType = "Wemos D1 mini";
  const char *socketHostURL = "sensordata.space";  // socket host  // change 192.168.0.87
  int port = 80; // socket port                    // change
  const char *updateURL = "http://192.168.0.87/update/files/firmware.bin";  // change

  char macAddress[20];
  String localIP;

  ESP8266WiFiMulti WiFiMulti;
  WebSocketsClient webSocket;
  DataToSendFunction defineDataToSend;
  RecievedDataFunction recievedData;

  //Sockets
  void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
  void gotMessageSocket(uint8_t * payload);
  void sendDataWithSocket(DynamicJsonDocument doc);
  void getDataFromSocket(DynamicJsonDocument recievedDoc); // TODO
  //OTA
  void updatingMode();

public:
  // public methods
  SocketClient();

  void init() {
    // initial setup
    // init mac address
    String mac = WiFi.macAddress();
    mac.toCharArray(macAddress, 50);
    // init local IP
    localIP = WiFi.localIP().toString();
    webSocket.begin(socketHostURL, port, "/"); // server address, port and URL
    webSocket.onEvent(SocketClient_webSocketEvent);   // initialte our event handler
    // webSocket.setAuthorization("user", "Password"); // use HTTP Basic Authorization this is optional remove if not needed
    webSocket.setReconnectInterval(5000); // try ever 5000 again if connection has failed
  }

  void initSSL() {
    // initial setup
    // init mac address
    String mac = WiFi.macAddress();
    mac.toCharArray(macAddress, 50);
    // init local IP
    localIP = WiFi.localIP().toString();
    webSocket.beginSSL(socketHostURL, port, "/"); // server address, port and URL
    webSocket.onEvent(SocketClient_webSocketEvent);   // initialte our event handler
    // webSocket.setAuthorization("user", "Password"); // use HTTP Basic Authorization this is optional remove if not needed
    webSocket.setReconnectInterval(5000); // try ever 5000 again if connection has failed
  }

  void loop() {
    this->webSocket.loop();
  }
  // setters
  void setAppAndVersion(const char * deviceApp, float version) {
    this->deviceApp = deviceApp;
    this->version = version;
  }
  void setDeviceType(const char * deviceType) {
    this->deviceType = deviceType;
  }
  void setSocketHost(const char * socketHostURL, int port) {
    this->socketHostURL = socketHostURL;
    this->port = port;
  }
  void setDataToSendFunciton(DataToSendFunction func) {
    this->defineDataToSend = func;
  }
  void setRecievedDataFunciton(RecievedDataFunction func) {
    this->recievedData = func;
  }
};