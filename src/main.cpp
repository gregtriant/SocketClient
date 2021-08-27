#include "SocketClient.h"
#include "globals.h"

const char* ssid     = myssid;     // declared in globals.h        
const char* password = mypassword; // declared in globals.h   

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
  Serial.println("\nConnection established!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP()); 


  // test client
  testClient.setSocketHost("sensordata.space", 80);  //192.168.0.87
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