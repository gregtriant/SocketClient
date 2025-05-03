#include <Arduino.h>
#include <arduino-timer.h>
#include "SocketClient.h"
#include "globals.h"
#include "DHT.h"

#define VERSION 1.0

#define LED_PIN 33
#define LIGHT_SENSOR_PIN 35
#define DHTPIN 32
#define DHTTYPE DHT11

typedef struct {
  int lightSensorValue;        // Raw value from the light sensor (0-4095 for 12-bit ADC)
  float lightSensorPercentage; // Percentage of light (0-100%)
} LightSensorData;
typedef struct {
  float temperature; // Temperature in Celsius
  float humidity;    // Humidity in percentage
  float heatIndex;   // Heat index in Celsius
} DHTData;

DHT dht(DHTPIN, DHTTYPE);
SocketClient testClient;
auto send_data_timer = timer_create_default();

// functions
DHTData readDHT();
LightSensorData readLightSensor();

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
    testClient.sendStatusWithSocket();
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
    testClient.sendStatusWithSocket();
  }
}

bool sendStatusNow(void* context) {
  testClient.sendStatusWithSocket(true);
  return true; // return true to keep the timer running
}

void setup()
{

  Serial.begin(115200);
  delay(1000);


  testClient.initWifi();
  testClient.setLedPin(LED_PIN); // set the led pin to blink when connected

  testClient.setAppAndVersion("Solar", VERSION);
  testClient.setToken(token);
  testClient.setSendStatusFunction(sendStatus);
  testClient.setReceivedCommandFunction(receivedCommand);
  testClient.setEntityChangedFunction(entityChanged);

  testClient.setConnectedFunction([](JsonDoc &doc) {
    Serial.print("Connected data:  ");
    serializeJson(doc, Serial);
    Serial.println();
    // do something with the connected data

    // send connected notification
    doc.clear();
    testClient.sendNotification("Connected!");
    testClient.sendStatusWithSocket(true);
  });
  testClient.init("api.sensordata.space", 443, true); // if you dont want ssl use .init and change the port.
  
  
  dht.begin();
  send_data_timer.every(5*60*1000, sendStatusNow, nullptr);
}


void loop()
{
  testClient.loop();
  send_data_timer.tick();
}


LightSensorData readLightSensor()
{
  delay(2000);
  // read the light sensor value
  int lightValue = analogRead(LIGHT_SENSOR_PIN);
  float percentage = (lightValue / 4095.0) * 100; // Assuming a 12-bit ADC (0-4095)
  // send the light value to the server
  Serial.print("Light: ");
  Serial.println(lightValue);
  Serial.print("Light %: ");
  Serial.println(percentage);

  LightSensorData lightSensorData = {
    .lightSensorValue = lightValue,
    .lightSensorPercentage = percentage,
  };

  return lightSensorData;
}

DHTData readDHT() {
  delay(2000);

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)

  // Check if any reads failed and exit early (to try again).
  DHTData dhtData = {
    .temperature = 0,
    .humidity = 0,
    .heatIndex = 0,
  };
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return dhtData;
  }

  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));

  dhtData = {
    .temperature = t,
    .humidity = h,
    .heatIndex = hic,
  };
  
  return dhtData;
}

