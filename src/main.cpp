#include "SocketClient.h"
//#include "globals.h"

const char* ssid     = "dobbielink";     // declared in globals.h        
const char* password = "2c26c57261f8"; // declared in globals.h   

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

  // Connect to the network
  WiFi.begin(ssid, password);         
  Serial.print("\nConnecting to "); 
  Serial.print(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print('.');
  }
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP()); 


  // test client
  testClient.setSocketHost("sensordata.ddnsfree.com", 443, true);  //192.168.0.56
  testClient.setAppAndVersion("Development", 0.03);
  //-testClient.setDeviceType("ESP8266");
  testClient.setDataToSendFunciton(defineDataToSend);
  testClient.setRecievedDataFunciton(recievedData);
  
  testClient.init(); // if you dont want ssl use .init and change the port.
}

void loop() {
  testClient.loop();
}