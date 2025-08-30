#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
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

#define NVS_WIFI_NAMESPACE      "WIFIPrefs"
#define NVS_WIFI_SSID_TOKEN     "ssid"
#define NVS_WIFI_PASSWORD_TOKEN "pass"

class NVSManager
{
protected:
    Preferences _wifi_preferences;
public:
    NVSManager();
    ~NVSManager();

    void saveWifiCredentials(String ssid, String password);
    void getWifiCredentials(String &ssid, String &password);
};
