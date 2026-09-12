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
    bool _everConnected          = false; // true once WiFi has connected at least once since boot; gates AP+STA fallback
    uint64_t _boot_time          = 0;     // millis() at construction; start of the post-boot AP-fallback grace window

    // No channel awareness of any kind here - WifiManager doesn't know or care what else
    // (e.g. EspxNet) shares the radio. Its only job re: that is timing: bound each connect
    // attempt and back off hard between retries so it doesn't hog the radio - see
    // _reconnect_backoff_stage below and _connectingToWifi()'s give-up branch.
    uint32_t _reconnect_backoff_stage = 0;   // 0 = no give-up yet since last connect; N = the
                                              // Nth consecutive give-up - see
                                              // _wifiReconnectBackoffMs() for the resulting delay
    static const uint64_t RECONNECT_BACKOFF_BASE_MS = 30UL * 1000;        // 30s - first backoff
                                                                            // after a give-up
    static const uint64_t RECONNECT_BACKOFF_MAX_MS  = 30UL * 60 * 1000;   // 30 min cap, held
                                                                            // indefinitely once hit
                                                                            // (e.g. a router that
                                                                            // stays down)

    // Doubles from RECONNECT_BACKOFF_BASE_MS each consecutive give-up (stage 1 = 30s, 2 = 60s,
    // 3 = 120s, ...), capped at RECONNECT_BACKOFF_MAX_MS. stage 0 (no give-up yet) also returns
    // the base interval - this is what drives the ~30s retry cadence during the initial
    // AP-fallback grace window too (see loop()), where the stage never advances at all.
    // Bounds the shift itself (rather than relying on the ms-vs-cap comparison alone) so an
    // arbitrarily large stage count over a very long-running outage can never shift a uint64_t
    // by more than 63 bits - undefined behavior otherwise.
    static uint64_t _wifiReconnectBackoffMs(uint32_t stage) {
        if (stage == 0) return RECONNECT_BACKOFF_BASE_MS;
        uint32_t shift = stage - 1;
        if (shift >= 6) return RECONNECT_BACKOFF_MAX_MS;   // 30s << 6 = 1920s already > 30 min cap
        uint64_t ms = RECONNECT_BACKOFF_BASE_MS << shift;
        return (ms > RECONNECT_BACKOFF_MAX_MS) ? RECONNECT_BACKOFF_MAX_MS : ms;
    }

    // How long to keep retrying saved credentials station-only after boot before giving up and
    // falling back to AP+STA mode. Long enough to ride out a router reboot (e.g. a shared power
    // blip resets both the device and the router; routers commonly take 30-90s to come back up),
    // short enough that a genuinely wrong/missing password doesn't leave the device unreachable
    // for provisioning for too long.
    static const uint64_t AP_FALLBACK_GRACE_MS = 120000; // 2 min

    // for AP mode. Open network (no password) unless the consumer opts into one via
    // SocketClient::setPasswordAP() - see _initAPMode().
    String _ap_ssid     = "";
    String _ap_password = "";
    uint64_t _ap_time   = 0;
    bool _apStaFinal    = false; // true once AP+STA fallback has been entered this boot; latched
                                  // until reboot, per _initAPMode()

    void _wifiConnected();
    void _connectingToWifi(String ssid, String password);
    void _initAPMode();
    void _scanNetworks();

    std::function<void()> _onInternetRestored;
    std::function<void()> _onInternetLost;
public:
    WifiManager(NVSManager *nvsManager, const String& ap_ssid, std::function<void()> onInternetRestored = nullptr, std::function<void()> onInternetLost = nullptr);

    // Opts the AP into WPA2 instead of the open-network default. Must be called before the AP
    // is (re)started (i.e. before init(), or before whatever later triggers _initAPMode()) to
    // take effect. password must be 8-63 chars (WPA2-PSK requirement) or it is rejected and the
    // AP stays/remains open - see _initAPMode().
    void setApPassword(const String& password) { _ap_password = password; }

    void init();
    void loop();

    String getIP();
    String getMacAddress();
    bool isConnecting() { return _connecting_time != 0; }
    bool isManaged() { return _managed; } // true if loop() is actively driving the connection

    // True once AP+STA fallback has been entered this boot. It is final until reboot: loop()
    // will not automatically retry the saved credentials while this is set, so a client must
    // either submit new credentials (tryNewCredentials()) or reboot the device to try again.
    bool isApStaFinal() { return _apStaFinal; }

    // True if remoteIp is on the same subnet as one of this device's own interfaces (its AP
    // subnet and/or its STA subnet). Used to restrict sensitive actions (reboot, WiFi connect,
    // WiFi scan) to clients on the device's own local network, rather than anything that can
    // merely route a request to it (e.g. a port-forwarded or proxied remote client).
    bool isLocalAddress(const IPAddress& remoteIp);

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
