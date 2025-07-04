/**
 * Define all the sensors used here! 
 *
 */
#ifndef SENSORS_H
#define SENSORS_H

#include "pins.h"
#include "DHT.h"

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


/**
 * Initialize the sensors.
 */
void initSensors(void);


/**
 * Read the DHT sensor.
 */
DHTData readDHT(void);


/**
 * Read the light sensor.
 */
LightSensorData readLightSensor(void);

#endif // SENSORS_H