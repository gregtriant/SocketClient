#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "../Log/Log.h"

#define MAX_HA_ENTITIES 8

typedef struct {
    const char *server;
    int         port;
    const char *username     = "";
    const char *password     = "";
    const char *deviceName;
    const char *displayName  = nullptr;
    const char *manufacturer = nullptr;
    const char *model        = nullptr;
} HAMqttConfig_t;

struct HAEntity {
    String id;    // e.g. "button1"
    String name;  // e.g. "Home Notify Button 1"
};

class HAMqtt {
public:
    HAMqtt(const HAMqttConfig_t *config);

    void addEntity(const HAEntity &entity);
    void loop();
    void publishEvent(const String &entityId, const String &action);
    bool isConnected();

private:
    const HAMqttConfig_t *_config;
    WiFiClient   _wifiClient;
    PubSubClient _mqttClient;

    HAEntity _entities[MAX_HA_ENTITIES];
    uint8_t  _entityCount = 0;

    String _baseTopic;
    String _deviceId;  // MAC-derived, populated on first connect

    void _connect();
    void _setupDiscovery();
    void _publishDiscoveryForEntity(const HAEntity &entity);
};
