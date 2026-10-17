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
#define NVS_DIAG_REBOOT_KEY "rebootCount"
#define NVS_DIAG_OLD_RESET_KEY "resetCount"   // before 1.9.0; begin() moves it into rebootCount

class Diagnostics {
public:
    using SendFn = std::function<void(const String&)>;

    Diagnostics(NVSManager* nvsManager, SendFn sendFn);

    void begin();
    void onSocketConnected();
    void onConnected(bool enabled);
    void onDebugConfig(bool enabled);
    void loop();

private:
    NVSManager* _nvsManager;
    SendFn _sendFn;
    bool _enabled = false;
    uint32_t _rebootCount = 0;
    // Deliberately RAM-only, unlike _rebootCount: it must start over at every boot,
    // so that a drop back to 1 is what distinguishes a reboot from a plain
    // reconnect. Counts every handshake including the first one after boot, so 1
    // means "still on the connection we opened at boot".
    uint32_t _reconnectCount = 0;
    String _rebootReason;   // a code from System::rebootReasonCode(), see System.h
    unsigned long _lastSent = 0;
    bool _hasSent = false;
    static const unsigned long INTERVAL = 3600000UL;

    void _send();
};
