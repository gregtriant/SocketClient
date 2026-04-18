#include "HAMqtt.h"
#include <WiFi.h>

HAMqtt::HAMqtt(const HAMqttConfig_t *config) : _mqttClient(_wifiClient) {
    _config = config;
    _baseTopic = "homeassistant/device/" + String(config->deviceName);
    _mqttClient.setBufferSize(1024);
    _mqttClient.setServer(config->server, config->port);
    MY_LOGD(MQTT_TAG, "MQTT configured: %s:%d", config->server, config->port);
}

void HAMqtt::addEntity(const HAEntity &entity) {
    if (_entityCount < MAX_HA_ENTITIES) {
        _entities[_entityCount++] = entity;
    }
}

void HAMqtt::_connect() {
    if (WiFi.status() != WL_CONNECTED) {
        MY_LOGD(MQTT_TAG, "WiFi not connected - skipping MQTT");
        return;
    }
    if (_mqttClient.connected()) return;

    // Derive device ID from MAC on first connect
    if (_deviceId.isEmpty()) {
        _deviceId = WiFi.macAddress();
        _deviceId.replace(":", "");
        _deviceId.toLowerCase();
    }

    String clientId = String(_config->deviceName) + "-" + WiFi.macAddress();
    MY_LOGD(MQTT_TAG, "Connecting to %s:%d (clientId: %s)", _config->server, _config->port, clientId.c_str());

    bool ok;
    if (_config->username && strlen(_config->username) > 0) {
        ok = _mqttClient.connect(clientId.c_str(), _config->username, _config->password);
    } else {
        ok = _mqttClient.connect(clientId.c_str());
    }

    if (ok) {
        MY_LOGD(MQTT_TAG, "Connected!");
        _setupDiscovery();
    } else {
        MY_LOGE(MQTT_TAG, "Connection failed, rc=%d", _mqttClient.state());
    }
}

void HAMqtt::loop() {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 5000) {
        lastCheck = millis();
        if (!_mqttClient.connected()) {
            _connect();
        }
    }
    if (_mqttClient.connected()) {
        _mqttClient.loop();
    }
}

void HAMqtt::_setupDiscovery() {
    MY_LOGD(MQTT_TAG, "Publishing HA autodiscovery (%d entities)", _entityCount);
    for (uint8_t i = 0; i < _entityCount; i++) {
        _publishDiscoveryForEntity(_entities[i]);
        delay(100);
    }
}

void HAMqtt::_publishDiscoveryForEntity(const HAEntity &entity) {
    const char *displayName  = (_config->displayName  && strlen(_config->displayName)  > 0) ? _config->displayName  : _config->deviceName;
    const char *manufacturer = (_config->manufacturer && strlen(_config->manufacturer) > 0) ? _config->manufacturer : "Custom";
    const char *model        = (_config->model        && strlen(_config->model)        > 0) ? _config->model        : "ESP32";

    JsonDocument device;
    device["identifiers"][0] = _deviceId;
    device["name"]           = displayName;
    device["manufacturer"]   = manufacturer;
    device["model"]          = model;

    JsonDocument config;
    config["name"]                  = entity.name;
    config["unique_id"]             = _deviceId + "_" + entity.id;
    config["state_topic"]           = _baseTopic + "/" + entity.id + "/state";
    config["json_attributes_topic"] = _baseTopic + "/" + entity.id + "/state";
    config["device"]                = device;

    String payload;
    serializeJson(config, payload);

    String topic = "homeassistant/sensor/" + _deviceId + "_" + entity.id + "/config";
    bool result = _mqttClient.publish(topic.c_str(), payload.c_str(), true);
    MY_LOGD(MQTT_TAG, "Discovery [%s]: %s (%d bytes)", entity.id.c_str(), result ? "OK" : "FAILED", payload.length());
}

void HAMqtt::publishEvent(const String &entityId, const String &action) {
    if (!_mqttClient.connected()) {
        MY_LOGW(MQTT_TAG, "Cannot publish [%s] - not connected", entityId.c_str());
        return;
    }
    String topic   = _baseTopic + "/" + entityId + "/state";
    String payload = "{\"action\":\"" + action + "\",\"timestamp\":" + String(millis()) + "}";
    bool result = _mqttClient.publish(topic.c_str(), payload.c_str());
    MY_LOGD(MQTT_TAG, "Event [%s/%s]: %s", entityId.c_str(), action.c_str(), result ? "OK" : "FAILED");
}

bool HAMqtt::isConnected() {
    return _mqttClient.connected();
}
