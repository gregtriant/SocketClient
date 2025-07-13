#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
#include "SocketClientDefs.h"

#if defined(ESP32) || defined(LIBRETUYA)
#include <WiFi.h>
#include <AsyncTCP.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <DNSServer.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <Preferences.h>
#else
#error Platform not supported
#endif

#include "../Log/Log.h"
#include "../NVS/NVSManager.h"

class WifiManager 
{

protected:
    NVSManager *_nvsManager;

    String _wifi_ssid            = "";
    String _wifi_password        = "";
    String _local_ip             = "";
    String _mac_address          = "";
    uint64_t _connecting_time    = 0;
    uint8_t _connecting_attempts = 0;
    wl_status_t _wifi_status     = WL_IDLE_STATUS; // current wifi status

    // for AP mode
    String _ap_ssid     = "";
    String _ap_password = "";
    uint64_t _ap_time   = 0;

    void _wifiConnected();
    void _connectingToWifi(String ssid, String password);
    void _initAPMode();

    std::function<void()> _onInternetRestored;
    std::function<void()> _onInternetLost;
public:
    WifiManager(NVSManager *nvsManager, const String& ap_ssid, const String& ap_password, std::function<void()> onInternetRestored = nullptr, std::function<void()> onInternetLost = nullptr);
    
    void init();
    void loop();

    String getIP();
    String getMacAddress();

    void saveWifiCredentials(String ssid, String password) {
        _wifi_ssid = ssid;
        _wifi_password = password;
        _nvsManager->saveWifiCredentials(ssid, password);
    }
};
