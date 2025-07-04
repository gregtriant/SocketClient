#pragma once
#include <Arduino.h>

#if defined(ESP32) || defined(LIBRETUYA)
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#else
#error Platform not supported
#endif

#include "../Log/Log.h"

class OTAManager
{
protected:
#if defined(ESP32) || defined(LIBRETUYA)
    void _checkUpdate(String host);
    void _updateFirmware(uint8_t *data, size_t len);
    int _totalLength;       // total size of firmware
    int _currentLength = 0; // current size of written firmware
#endif

#ifdef ESP8266
    void updatingMode(String updateURL);
    static void update_started();
    static void update_finished();
    static void update_progress(int cur, int total);
    static void update_error(int err);
#endif

public:
    OTAManager();
    ~OTAManager();

    void startOTA(String updateURL);
};
