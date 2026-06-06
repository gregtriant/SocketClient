// src/Diagnostics/Diagnostics.h
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include "../NVS/NVSManager.h"

#if defined(ESP32) || defined(LIBRETUYA)
#include <WiFi.h>
#include <esp_system.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#define NVS_DIAG_NAMESPACE "sc-diag"
#define NVS_DIAG_RESET_KEY "resetCount"

class Diagnostics {
public:
    using SendFn = std::function<void(const String&)>;

    Diagnostics(NVSManager* nvsManager, SendFn sendFn);

    void begin();
    void onConnected(bool enabled);
    void onDebugConfig(bool enabled);
    void loop();

private:
    NVSManager* _nvsManager;
    SendFn _sendFn;
    bool _enabled = false;
    uint32_t _resetCount = 0;
    String _resetReason;
    unsigned long _lastSent = 0;
    static const unsigned long INTERVAL = 3600000UL;

    void _send();

#if defined(ESP32) || defined(LIBRETUYA)
    static String _resetReasonToString(esp_reset_reason_t reason);
#endif
};
