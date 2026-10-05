#pragma once
#include <Arduino.h>
#include "SocketClientDefs.h"

#if defined(ESP32) || defined(LIBRETUYA)
#include <WiFi.h>
#include <Preferences.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <Preferences.h>
#else
#error Platform not supported
#endif

#define RW_MODE false
#define RO_MODE true

// SocketClient's own settings. The stored name stays "WIFIPrefs" (it used to hold only the
// WiFi credentials): renaming it would make deployed devices lose them on update.
#define NVS_SC_NAMESPACE        "WIFIPrefs"
#define NVS_WIFI_SSID_TOKEN     "ssid"
#define NVS_WIFI_PASSWORD_TOKEN "pass"
#define NVS_TZ_SET_TOKEN        "tz_set"   // timezone kept by setTZ()
#define NVS_TZ_SRV_TOKEN        "tz_srv"   // last timezone the server sent

class NVSManager
{
protected:
    Preferences _prefs;
public:
    NVSManager();
    ~NVSManager();

    uint32_t getUInt(const char* ns, const char* key, uint32_t defaultValue = 0);
    void     putUInt(const char* ns, const char* key, uint32_t value);
    String   getString(const char* ns, const char* key, const String& defaultValue = "");
    void     putString(const char* ns, const char* key, const String& value);

    void saveWifiCredentials(String ssid, String password);
    void getWifiCredentials(String& ssid, String& password);

    void saveTZ(const char* key, const char* tz);         // "" removes the key
    void getTZ(const char* key, char* buf, size_t n);     // "" if absent
};
