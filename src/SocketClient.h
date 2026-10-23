#pragma once

// Must come before <WebSocketsClient.h> pulls in WebSockets.h, whose own #ifndef guard only
// takes the library's 5000ms default if nothing has defined this macro yet. That default
// leaves zero margin for a "cold" TLS connect (no recently-cached route/ARP/DNS state) to a
// host that just plain takes a few seconds to answer on a fresh connection - confirmed on
// real hardware (WaterTank/Water2): a cold connect to api.sensordata.space measured ~5036ms
// from a clean state, landing right on top of the old 5000ms budget, so any tiny bit of
// extra jitter reliably tipped it into start_ssl_client() failing with a bare select()
// timeout - not a TLS/certificate/DNS problem, just too little time budgeted for a slow but
// otherwise healthy connection. 15s leaves real headroom.
#ifndef WEBSOCKETS_TCP_TIMEOUT
#define WEBSOCKETS_TCP_TIMEOUT 15000
#endif

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <WiFiClientSecure.h>
#include "System/System.h"

#if defined(ESP32) || defined(LIBRETUYA)
#include <AsyncTCP.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncWebServer.h>

#elif defined(ESP8266)
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266WiFi.h>
#include <Preferences.h>
#else
#error Platform not supported
#endif

#include "NVS/NVSManager.h"
#include "OTA/OTAManager.h"
#include "SocketClientDefs.h"
//- #include "WebserverManager/WebserverManager.h"
//- #include "WifiManager/WifiManager.h"
#include "TimeClient/TimeClient.h"
#include "Diagnostics/Diagnostics.h"

#define SC_TZ_LEN 48  // POSIX TZ string incl. terminator

class WifiManager;
class WebserverManager;
/**
 * @brief The SocketClient class provides functionality for connecting to a socket server
 */
class SocketClient {
    friend void SocketClient_webSocketEvent(WStype_t type, uint8_t *payload, size_t length);
    
   protected:
    WebSocketsClient *_webSocket;
    NVSManager *_nvsManager;
    Diagnostics *_diagnostics;
    WifiManager *_wifiManager;
    WebserverManager *_webserverManager;
    OTAManager *_otaManager;
    JsonDocument _doc;  //- used to store the JSON data
    TimeClient _tc;

    // Data.
    float _version = 0.2;  // change
    const char *_deviceApp = DEFAULT_APP_NAME;
    const char *_deviceType = DEVICE_TYPE;

    const char *_token = "";
    const char *_apPassword = nullptr; // nullptr/empty = open AP; see setPasswordAP()
    const char *_socketHostURL = DEFAULT_HOST;
    int _port = DEFAULT_PORT;
    bool _isSSL;

    DeviceInfo_t _deviceInfo;

    String _macAddress;
    String _localIP;
    bool _handleWifi = false;
    bool _led_state = false;
    uint64_t _led_blink_time = 0;  // used to turn led on and off

    uint32_t _local_time_offset = 0;
    // Timezone (POSIX TZ strings), persisted in NVS so local time is right from boot. The one
    // in effect is _tz_set if there is one, else _tz_srv. Fixed buffers: setTZ()/getTZ(buf)
    // may be called from another task, the apply + NVS write happen in loop() (_tzFlush()).
    char _tz_set[SC_TZ_LEN] = "";   // set with setTZ(tz, true): wins over the server's
    char _tz_srv[SC_TZ_LEN] = "";   // last timezone the server sent (or setTZ(tz, false))
    bool _tz_apply = false;         // zone in effect changed, not applied yet
    bool _tz_save_set = false;      // _tz_set changed, not written to NVS yet
    bool _tz_save_srv = false;      // same for _tz_srv
    void _tzLoad();
    void _tzFlush();

    SendStatusFunction sendStatus;
    ReceivedCommandFunction receivedCommand;
    EntityChangedFunction entityChanged;
    ConnectedFunction connected;
    FileReceivedFunction  _fileReceived;
    FileRequestedFunction _fileRequested;

    bool _debugLoggingEnabled = false;
    uint8_t _debugLogLevelIndex = DEBUG_LEVEL_INFO;  // send error/warning/info by default

    static uint8_t _levelStringToIndex(const char *level);
    void _applyDebugConfig(JsonObject debug);

    void gotMessageSocket(uint8_t *payload);
    void _init();
    void _sendFile(const String &filename, const String &requestId);
    void _sendFileError(const String &requestId, const String &filename, const char *error);
    void watchdog();
    static unsigned long last_dog;
    static unsigned long last_png;
    static const unsigned long tick_time = 6000;
    static unsigned long last_reconnect;
    static unsigned long reconnect_time;                       //- 30 sec, doubles on each failed
                                                                //  attempt (see watchdog()) up to the cap below
    static const unsigned long max_reconnect_time = 180000L;  //- 3 min cap
    static const unsigned long watchdog_time = (5 * tick_time);      //- 30 s; must be > heartbeat interval (15 s)

    // WifiManager's _onInternetRestored fires the instant WL_CONNECTED lands, which can be too
    // soon for hostByName() to succeed yet (DHCP-assigned DNS servers still settling, radio
    // waking from power-save, etc. - see WiFiGeneric.cpp's "DNS Failed" log). Rather than
    // resolving the socket host synchronously inside that callback, it just schedules a
    // reconnect() this many ms out; loop() fires it once the deadline passes. 0/false = none
    // pending.
    static const unsigned long WIFI_RECONNECT_GRACE_MS = 1500;
    bool _pendingWifiReconnect = false;
    unsigned long _pendingWifiReconnectAt = 0;

    // socketOn()/socketOff()/wifiOn()/wifiOff() only record what is wanted (they may be called
    // from another task); _applyOnOff() in loop() does the work. The socket runs only while
    // both are wanted.
    volatile bool _socket_want = true;
    volatile bool _wifi_want = true;
    bool _socket_on = true;   // what loop() last applied: socket wanted and WiFi wanted
    bool _wifi_on = true;     // what loop() last applied
    void _applyOnOff();

    // Set by reboot() (and so by the server's "@reboot" message); loop() closes the socket
    // and reboots once the caller has returned.
    volatile bool _pendingReboot = false;
    volatile System::RebootReason _rebootReason = System::RebootReason::Software;

public:
    SocketClient();
    ~SocketClient();

    // resetBackoff: true (default) drops reconnect_time back to its 30s baseline - correct for
    // a genuinely new condition (initial boot, a fresh WiFi connection via WifiManager's
    // _onInternetRestored callback). watchdog() itself calls reconnect(false) so its own
    // doubling backoff (see watchdog()'s definition) isn't immediately undone by this call.
    void reconnect(bool resetBackoff = true);
    void stopReconnect();
    void sendStatusWithSocket(bool save = false);  //- do the default (no receiverid)
    void sendLog(const String &message);
    void sendDebugLog(uint8_t level, const String &message);
    void sendNotification(const String &message);
    void sendNotification(const String &message, const JsonDoc &data);
    bool isConnected() { return _webSocket && _webSocket->isConnected(); }
    void disconnect() { if (_webSocket) _webSocket->disconnect(); }

    // Live switches, both on at every boot (nothing is saved). socketOff() closes the
    // connection to the server and stops every reconnect until socketOn(); WiFi and anything
    // else using it carry on. wifiOff() closes the WiFi connection (and the AP, if it is up)
    // and stops WifiManager's retries until wifiOn(), which starts again from the saved
    // credentials; without handleWifi it is a plain WiFi.disconnect(true) / WiFi.begin(). The
    // socket connects only while both are on. Callable from any task: the getters reflect a
    // change at once, the next loop() carries it out.
    void socketOn()  { _socket_want = true; }
    void socketOff() { _socket_want = false; }
    void wifiOn()    { _wifi_want = true; }
    void wifiOff()   { _wifi_want = false; }
    bool isSocketOn() const { return _socket_want; }
    bool isWifiOn() const   { return _wifi_want; }

    // Reboots the device on the next loop(), closing the socket first. After the boot,
    // diagnostics report `reason` (see System.h) instead of the chip's plain
    // "software". The "@reboot" message calls this with System::RebootReason::Cmd.
    void reboot(System::RebootReason reason = System::RebootReason::Software);

    void init(const SocketClientConfig_t *config);
    void init(const char *socketHostURL, int port, bool _isSSL);
    void loop();

    // Getters.
    String getCurrentStatus();
    String getVersion();

    // Setters.
    void setAppAndVersion(const char *deviceApp, float version) {
        this->_deviceApp = deviceApp;
        this->_version = version;

        // Just saving the name and version in deviceInfo.
        this->_deviceInfo.app = deviceApp;
        this->_deviceInfo.version = version;
    }

    void setDeviceType(const char *deviceType) {
        this->_deviceType = deviceType;

        // Just saving the type in deviceInfo.
        this->_deviceInfo.type = deviceType;
    }

    void setSocketHost(const char *socketHostURL, int port, bool _isSSL) {
        this->_socketHostURL = socketHostURL;
        this->_port = port;
        this->_isSSL = _isSSL;
    }

    void setSendStatusFunction(SendStatusFunction func) { this->sendStatus = func; }

    void setReceivedCommandFunction(ReceivedCommandFunction func) { this->receivedCommand = func; }

    void setEntityChangedFunction(EntityChangedFunction func) { this->entityChanged = func; }

    void setConnectedFunction(ConnectedFunction func) { this->connected = func; }

    void setToken(const char *token) { this->_token = token; }

    // Opts the AP into WPA2 instead of the default open network. Must be called before init()
    // (like setToken()); password must be 8-63 chars (WPA2-PSK requirement) or it is ignored
    // and the AP stays open - see WifiManager::setApPassword().
    void setPasswordAP(const char *password) { this->_apPassword = password; }

    void initWebserver(int port = 80);

    AsyncWebServer* getServer();

    bool hasTime();
    bool getTime(int &hh, int &mm, int &ss) { return _tc.getTime(hh, mm, ss); }
    bool getDate(int &yy, int &mm, int &dd) { return _tc.getDate(yy, mm, dd); }

    // The timezone is SocketClient's: the last one the server sent is saved in NVS and put in
    // effect by init(), so local time is right before (or without) the server - only NTP
    // still needs the "connected" reply. setTZ() overrides it with a POSIX TZ string, e.g.
    // "EET-2EEST,M3.5.0/3,M10.5.0/4"; false if it isn't shaped like one. With keep (default)
    // it is saved and wins over whatever the server sends; without, it only stands in for the
    // server's value until the server sends one. nullptr/"" drops the kept one and follows
    // the server again. Callable from any task: getTZ(buf)/isTZPinned() reflect it at once,
    // the zone is applied and saved by the next loop().
    bool setTZ(const char *tz, bool keep = true);
    bool getTZ(char *buf, size_t n, bool *pinned = nullptr);  // in effect; false if none. Any task
    bool isTZPinned() const { return _tz_set[0] != '\0'; }    // true: set with setTZ(tz, true)
    static bool validTZ(const char *tz);
    // loop() thread only:
    const char *getTZ() const { return _tz_set[0] ? _tz_set : _tz_srv; }  // in effect, "" if none
    const char *getServerTZ() const { return _tz_srv; }                   // "" if never received
};

