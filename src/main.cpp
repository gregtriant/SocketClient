#include <Arduino.h>
#include <arduino-timer.h>

#include "SocketClient.h"
#include "globals.h"
#include "pins.h"
#include "sensors.h"

#define VERSION 1.0

SocketClient socketClient;
auto send_data_timer = timer_create_default();

void sendStatus(JsonDoc &status)
{
  status.clear();
  DHTData dhtData = readDHT();
  LightSensorData lightSensorData = readLightSensor();
  status["Temperature C"] = dhtData.temperature;
  status["Humidity %"] = dhtData.humidity;
  status["HeatIndex C"] = dhtData.heatIndex;
  status["LightValue"] = lightSensorData.lightSensorValue;
  status["Light %"] = lightSensorData.lightSensorPercentage;
}

void receivedCommand(JsonDoc &doc)
{
  if (strcmp(doc["data"], "count") == 0)
  {
    socketClient.sendStatusWithSocket();
  }
}

void entityChanged(JsonDoc &doc)
{
  // String stringData = "";
  // serializeJson(doc, stringData);
  // Serial.print("Entity update: ");
  // Serial.println(stringData);
  if (strcmp(doc["entity"], "led") == 0)
  {
    // setLedState(atoi(doc["value"]));
  }
  else if (strcmp(doc["entity"], "slider") == 0)
  {
    // setSliderState(doc["value"]);
    socketClient.sendStatusWithSocket();
  }
}

void connectedFunction(JsonDoc &doc)
{
  Serial.print("Connected data:  ");
  serializeJson(doc, Serial);
  Serial.println();
  // do something with the connected data

  // send connected notification
  doc.clear();
  socketClient.sendNotification("Connected!");
  socketClient.sendStatusWithSocket(true);
}

bool sendStatusNow(void* context) {
  socketClient.sendStatusWithSocket(true);
  return true; // return true to keep the timer running
}

// socket client config
SocketClientConfig config = {
	.name = "Solar",
	.version = VERSION,
	.type = "ESP32",
  .ledPin = LED_PIN,
	.host = "api.sensordata.space",
	.port = 443, // 3030,
	.isSSL = true,
	.token = token, // from globals.h
  .handleWifi = true,
	.sendStatus = sendStatus,
	.receivedCommand = receivedCommand,
	.entityChanged = entityChanged,
	.connected = connectedFunction,
};

void setup()
{
  Serial.begin(115200);
  delay(1000);

  initSensors();
  socketClient.init(&config);
  send_data_timer.every(5*60*1000, sendStatusNow, nullptr);
}


void loop()
{
  socketClient.loop();
  send_data_timer.tick();
}
