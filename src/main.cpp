#include "SocketClient.h"
//#include "globals.h"

const char* ssid     = "dobbielink";     // declared in globals.h        
const char* password = "2c26c57261f8"; // declared in globals.h   

SocketClient testClient;
String message = "hello";
int count = 0;
int led_state = 0;
int slider = 0;

void setLedState(int ledState) {
  led_state = ledState;
  digitalWrite(LED_BUILTIN, 1 - ledState);
}
void setSliderState(int sliderState) {
  slider = sliderState;
}

DynamicJsonDocument sendStatus() {
  DynamicJsonDocument status(1024);
  status["message"] = message + String(count);
  status["led"] = String(led_state);
  status["slider"] = String(slider);

  return status;
}

void receivedCommand(DynamicJsonDocument doc) {
  // String stringData = "";
  // serializeJson(doc, stringData);
  // Serial.print("Command: ");
  // Serial.println(stringData);
  if (strcmp(doc["data"], "count") == 0) {
    count++;
    testClient.sendStatusWithSocket();
  }
}

void entityChanged(DynamicJsonDocument doc) {
  // String stringData = "";
  // serializeJson(doc, stringData);
  // Serial.print("Entity update: ");
  // Serial.println(stringData);
  if (strcmp(doc["entity"], "led") == 0) 
  {
    setLedState(atoi(doc["value"]));
  } 
  else if (strcmp(doc["entity"], "slider") == 0) 
  {
    setSliderState(doc["value"]);
    testClient.sendStatusWithSocket();
  }
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

  pinMode(LED_BUILTIN, OUTPUT);
  setLedState(led_state);

  // test client
  testClient.setSocketHost("api.sensordata.space", 80, false);  //192.168.0.56
  testClient.setAppAndVersion("Development", 0.03);
  //-testClient.setDeviceType("ESP8266");
  testClient.setSendStatusFunction(sendStatus);
  testClient.setReceivedCommandFunction(receivedCommand);
  testClient.setEntityChangedFunction(entityChanged);
  
  testClient.init(); // if you dont want ssl use .init and change the port.
}

void loop() {
  testClient.loop();
}