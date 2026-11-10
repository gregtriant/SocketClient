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

// How the manager tells its owner why an update stopped. Optional: nothing here depends on
// anyone having installed it.
//
// There is deliberately no progress callback. How far a download has got is counted by the
// server as it streams the image -- the only source for a device already in the field, and
// within a socket buffer of what this chip has actually received. What this side knows and
// the server cannot see is why an update *failed*: tooLarge about a second after the GET,
// flashFailed at the end of the flash. Without those the server waits out its whole
// three-minute reboot budget and then says something vague.
typedef std::function<void(const char *code, const char *detail)> OtaErrorFn;

// How long the download may deliver nothing at all before we give up on it. A half-open TCP
// connection never trips client.connected(), so without this the read loop spins forever:
// loop() never returns, the device stops reporting, stops reading sensors and never reboots,
// and only a power cycle brings it back. Generous enough for a slow link, short enough that
// the server's own 3-minute reboot budget isn't what notices.
static const uint32_t OTA_STALL_TIMEOUT_MS = 30000;

class OTAManager
{
protected:
    OtaErrorFn _onError = nullptr;

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
    static void update_error(int err);
#endif

public:
    OTAManager();
    ~OTAManager();

    // Install once, at construction time of the owner. May be left null.
    void setCallbacks(OtaErrorFn onError);

    void startOTA(String updateURL);
};
