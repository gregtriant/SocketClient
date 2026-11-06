#pragma once
#include <Arduino.h>

#if defined(ESP32) || defined(LIBRETUYA)
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#if defined(ESP32)
#include <WiFiClientSecure.h>
#endif
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#else
#error Platform not supported
#endif

#include <functional>

#include "../Log/Log.h"

// How the manager tells its owner how far along an update is, and why one stopped.
// Both are optional: nothing here depends on anyone having installed them.
typedef std::function<void(uint32_t written, uint32_t total)> OtaProgressFn;
typedef std::function<void(const char *code, const char *detail)> OtaErrorFn;

// A 1 MB image is tens of thousands of writes. The server and the browsers want a handful
// of updates a second out of that, not tens of thousands of socket frames, so a report has
// to be either late enough or large enough to be worth sending.
static const uint32_t OTA_PROGRESS_MIN_INTERVAL_MS = 500;
static const int OTA_PROGRESS_MIN_PERCENT = 2;

class OTAManager
{
protected:
    OtaProgressFn _onProgress = nullptr;
    OtaErrorFn _onError = nullptr;
    uint32_t _lastReportMs = 0;
    int _lastReportPercent = -1;

    // Applies the throttle above. A report where written == total always goes out: it is
    // what moves the UI out of "sending", and swallowing it would leave the bar stuck
    // just short of the end for the whole reboot.
    void _report(uint32_t written, uint32_t total);
    void _fail(const char *code, const char *detail);

#if defined(ESP32) || defined(LIBRETUYA)
    void _checkUpdate(String host);
    bool _updateFirmware(uint8_t *data, size_t len);
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

    // Install once, at construction time of the owner. Either may be left null.
    void setCallbacks(OtaProgressFn onProgress, OtaErrorFn onError);

    void startOTA(String updateURL);
};
