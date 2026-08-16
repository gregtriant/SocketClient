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
    bool _pending_save           = false; // true while connecting with unsaved candidate credentials
    bool _managed                = false; // true once init() has been called, i.e. loop() is being pumped

    // for AP mode
    String _ap_ssid     = "";
    String _ap_password = "";
    uint64_t _ap_time   = 0;

    void _wifiConnected();
    void _connectingToWifi(String ssid, String password);
    void _initAPMode();
    void _scanNetworks();

    std::function<void()> _onInternetRestored;
    std::function<void()> _onInternetLost;
public:
    WifiManager(NVSManager *nvsManager, const String& ap_ssid, const String& ap_password, std::function<void()> onInternetRestored = nullptr, std::function<void()> onInternetLost = nullptr);
    
    void init();
    void loop();

    String getIP();
    String getMacAddress();
    bool isConnecting() { return _connecting_time != 0; }
    bool isManaged() { return _managed; } // true if loop() is actively driving the connection

    // Attempts to connect with new candidate credentials without touching NVS yet.
    // They're only persisted once the connection actually succeeds (see _wifiConnected());
    // on failure the currently saved credentials are restored, untouched.
    void tryNewCredentials(String ssid, String password) {
        _pending_save = true;
        _connectingToWifi(ssid, password);
    }

    // Blocking variant for when nobody is pumping loop() (i.e. handleWifi is off): connects,
    // waits up to timeoutMs, saves to NVS only on success, and reconnects to whatever was
    // previously active on failure so a bad test doesn't disrupt the current connection.
    bool tryAndSaveCredentials(String ssid, String password, unsigned long timeoutMs = 15000);

    // void setInternetRestoredCallback(std::function<void()> cb) { _onInternetRestored = cb; }
};
