#include <Arduino.h>
#include <SocketClient.h>

#include "globals.h"

#define VERSION 1.0

#ifdef LED_BUILTIN
#define LED_PIN LED_BUILTIN
#else
#define LED_PIN 2
#endif

const char *my_ssid = "your_wifi_ssid";          // replace with your WiFi SSID
const char *my_password = "your_wifi_password";  // replace with your WiFi password
const char *token = "your_token";                // replace with your device token from sensordata.space

SocketClient testClient;
int counter = 0;

void sendStatus(JsonDoc status) { status["counter"] = counter; }

void receivedCommand(JsonDoc doc) {
    if (strcmp(doc["data"], "counterInc") == 0) {
        counter++;
    } else if (strcmp(doc["data"], "counterDec") == 0) {
        counter--;
    }
    testClient.sendStatusWithSocket();
}

void entityChanged(JsonDoc doc) {
    if (strcmp(doc["entity"], "counter") == 0) {
        counter = doc["value"].as<int>();
    }
    testClient.sendStatusWithSocket();
}

void connected(JsonDoc doc) {
    Serial.print("Connected data:  ");
    serializeJson(doc, Serial);
    Serial.println();
    // do something with the connected data

    testClient.sendNotification("Connected!");
    testClient.sendStatusWithSocket(true);
}


/**
 * Configuration for the SocketClient
 */
SocketClientConfig config = {
    .name = "TestEsp8266",
    .version = VERSION,
    .type = "ESP32",
    .ledPin = LED_PIN,
    .host = "api.sensordata.space",
    .port = 443,
    .isSSL = true,
    .token = token,    // from globals.h
    .handleWifi = true,
    .sendStatus = sendStatus,
    .receivedCommand = receivedCommand,
    .entityChanged = entityChanged,
    .connected = connected,
};


void setup() {
    Serial.begin(115200);
    delay(500);

    // connect wifi
    // Serial.print("Connecting to WiFi: ");
    // WiFi.begin(my_ssid, my_password);
    // while (WiFi.status() != WL_CONNECTED) {
    //     Serial.print(".");
    //     delay(500);
    // }
    // Serial.println("Connected!");
    // Serial.print("IP address: ");
    // Serial.println(WiFi.localIP());

    // testClient.setAppAndVersion("TestEsp8266", VERSION);
    // testClient.setToken(token);
    // testClient.setSendStatusFunction(sendStatus);
    // testClient.setReceivedCommandFunction(receivedCommand);
    // testClient.setEntityChangedFunction(entityChanged);
    // testClient.setConnectedFunction(connected);
    // testClient.init("api.sensordata.space", 443, true);  // if you dont want ssl use .init and change the port.

    // or just 
    testClient.init(&config);
}

void loop() { testClient.loop(); }
