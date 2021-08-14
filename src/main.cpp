#include <WiFiManager.h>
#include "SocketClient.h"

SocketClient testClient;


String defineDataToSend() {
  String stringToSend = "Hello from esp";
  return stringToSend;
}

void recievedData(String data) {
  Serial.println(data);
  // do something with the data
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  WiFiManager wifiManager;
  // wifiManager.resetSettings();
  wifiManager.autoConnect("ESPAutoConnectAP");

  // test client
  testClient.setHostPort(80);
  testClient.setSocketHostURL("sensordata.space");  //192.168.0.87
  testClient.setVersion(0.3);
  testClient.setDeviceApp("Your_app_name");
  testClient.setDeviceType("Wemos D1 mini");
  testClient.setUpdateURL("http://192.168.0.87/update/files/firmware.bin");
  testClient.setDataToSendFunciton(defineDataToSend);
  testClient.setRecievedDataFunciton(recievedData);
  testClient.init();
}

void loop() {
  testClient.loop();
}