#include "sensors.h"

#include "pins.h"

static DHT dht(DHTPIN, DHTTYPE);

void initSensors(void)
{
    dht.begin();
}


LightSensorData readLightSensor(void)
{
  int lightValue = analogRead(LIGHT_SENSOR_PIN);
  float percentage = (lightValue / 4095.0) * 100; // Assuming a 12-bit ADC (0-4095)
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


DHTData readDHT(void)
{
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
