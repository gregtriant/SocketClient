#include "WifiManager.h"
#include "../Log/Log.h"


WifiManager::WifiManager(NVSManager *nvsManager, const String& ap_ssid, const String& ap_password, std::function<void()> onInternetRestored, std::function<void()> onInternetLost)
{
    _nvsManager = nvsManager;
    _ap_ssid = ap_ssid;
    _ap_password = ap_password;
    _onInternetRestored = onInternetRestored;
    _onInternetLost = onInternetLost;

    _mac_address = WiFi.macAddress();
    _local_ip = WiFi.localIP().toString(); // in case already connected to wifi
    _nvsManager->getWifiCredentials(_wifi_ssid, _wifi_password);
}


void WifiManager::init()
{
    _managed = true;

    // check if we have credentials in NVS
    if (!_wifi_ssid.isEmpty() && !_wifi_password.isEmpty()) {
        SC_LOGI(WIFI_TAG, "WiFi credentials found in NVS.");
        // SC_LOGI(WIFI_TAG, "SSID: %s", _wifi_ssid.c_str());
        // SC_LOGI(WIFI_TAG, "Password: %s", _wifi_password.c_str());
        _connectingToWifi(_wifi_ssid, _wifi_password);
    } else {
        SC_LOGW(WIFI_TAG, "No WiFi credentials found in NVS. Please set them.");
        _initAPMode();
    }
}


void WifiManager::loop()
{
    uint64_t now = millis();
    wl_status_t wifiStatus = WiFi.status();
    /*int wifiMode =*/ (int)WiFi.getMode();

    if ((_wifi_status != WL_CONNECTION_LOST) && // Prev status
        (_wifi_status != WL_DISCONNECTED) &&    // Prev status
        (wifiStatus == WL_CONNECTION_LOST || wifiStatus == WL_DISCONNECTED)) {

        SC_LOGI(WIFI_TAG, "WiFi connection lost or disconnected. Trying to reconnect...");
        _wifi_status = wifiStatus;
        _ap_time = now; // Reset AP time.
        if (_onInternetLost) {
            _onInternetLost();
        }
    } else if ((_wifi_status != WL_CONNECTED) &&
               (wifiStatus == WL_CONNECTED)) {
        _wifiConnected();
    }

    // Don't fight a user-initiated scan for the (single) WiFi radio: WiFi.begin()/WiFi.mode()
    // calls below would abort an in-progress WiFi.scanNetworks(), and vice versa. Defer our
    // own background reconnect activity until the scan finishes.
    if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
        return;
    }

    if (now - _ap_time > 30000 && wifiStatus != WL_CONNECTED) { // if in AP mode for more than 30 sec, try to connect with old credentials
        _ap_time = now;
        SC_LOGI(WIFI_TAG, "30 seconds in ap mode... Connecting...");
        init(); // connect with old credentials
    }

    if (_connecting_time != 0) { // trying to connect to wifi
        if (now - _connecting_time > 1000) {
            _connecting_time = now;
            _connecting_attempts++;
            // TODO: Here toggle connection led
            SC_LOGI(WIFI_TAG, "%d...", _connecting_attempts);
            if (_connecting_attempts >= 15) {
                _connecting_time = 0;
                _connecting_attempts = 0;
                _wifi_status = WL_CONNECTION_LOST;
                if (_pending_save) {
                    // New candidate credentials didn't work; NVS was never touched, so just
                    // reload what's actually saved and retry with that instead.
                    _pending_save = false;
                    SC_LOGI(WIFI_TAG, "Could not connect with new credentials, reverting to saved ones.");
                    _nvsManager->getWifiCredentials(_wifi_ssid, _wifi_password);
                }
                _initAPMode();
                return;
            }
        }
    }

}


void WifiManager::_connectingToWifi(String ssid, String password)
{
    _wifi_ssid = ssid;
    _wifi_password = password;
    _connecting_time = millis();
    _connecting_attempts = 0;
    _wifi_status = WiFi.status();
    SC_LOGI(WIFI_TAG, "Connecting to WiFi: %s", _wifi_ssid.c_str());
    if (WiFi.getMode() != CONST_MODE_AP_STA && WiFi.getMode() != CONST_MODE_STA) {
        WiFi.mode(WIFI_STA);
    }
    WiFi.begin(ssid.c_str(), password.c_str());
}


void WifiManager::_wifiConnected()
{
    if (WiFi.getMode() == CONST_MODE_AP_STA) {
        SC_LOGI(WIFI_TAG, "Stopping AP+STA mode...");
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
    }
    _connecting_time = 0;     // Means connected.
    _connecting_attempts = 0; // Reset connecting attempts.
    if (_pending_save) {
        _pending_save = false;
        _nvsManager->saveWifiCredentials(_wifi_ssid, _wifi_password);
        SC_LOGI(WIFI_TAG, "New WiFi credentials saved.");
    }
    SC_LOGI(WIFI_TAG, "Connected to %s! IP address: %s", _wifi_ssid.c_str(), WiFi.localIP().toString().c_str());
    _local_ip = WiFi.localIP().toString();
    _wifi_status = WiFi.status();
    if (_onInternetRestored) {
        _onInternetRestored();
    }
}


void WifiManager::_initAPMode()
{
    if (WiFi.getMode() == CONST_MODE_AP_STA) {
        SC_LOGI(WIFI_TAG, "Already in AP+STA mode. Skipping...");
        return;
    }
    // stop STA mode
    WiFi.disconnect(true);
    // Set WiFi to AP+STA mode
    WiFi.mode(WIFI_AP_STA);

    // if in AP_STA mode make sure in same channel
    IPAddress apIP(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, subnet);

    SC_LOGI(WIFI_TAG, "AP ssid: %s", _ap_ssid.c_str());
    SC_LOGI(WIFI_TAG, "AP pass: %s", _ap_password.c_str());
    WiFi.softAP(_ap_ssid, _ap_password); // AP name and password

    SC_LOGI(WIFI_TAG, "Starting AP+STA mode... IP: %s", WiFi.softAPIP().toString().c_str());

    _ap_time = millis();
}


void WifiManager::_scanNetworks()
{
    // scan wifi
    int n = WiFi.scanNetworks();
    if (n == 0) {
        Serial.println("No networks found.");
    } else {
        Serial.printf("%d networks found:\n\n", n);

        // Store SSIDs for duplicate detection
        for (int i = 0; i < n; ++i) {
        Serial.printf("Network %d: %s | RSSI: %d | Channel: %d | BSSID: %s\n",
            i + 1,
            WiFi.SSID(i).c_str(),
            WiFi.RSSI(i),
            WiFi.channel(i),
            WiFi.BSSIDstr(i).c_str()
        );
        }

        Serial.println("\nChecking for duplicate SSIDs...");
        for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (WiFi.SSID(i) == WiFi.SSID(j)) {
            Serial.printf("⚠️ Duplicate SSID found: %s (Channels %d & %d, BSSIDs %s and %s)\n",
                WiFi.SSID(i).c_str(),
                WiFi.channel(i), WiFi.channel(j),
                WiFi.BSSIDstr(i).c_str(),
                WiFi.BSSIDstr(j).c_str()
            );
            }
        }
        }
    }
}


bool WifiManager::tryAndSaveCredentials(String ssid, String password, unsigned long timeoutMs)
{
    String prevSsid = WiFi.SSID();
    String prevPassword = WiFi.psk();
    bool wasConnected = (WiFi.status() == WL_CONNECTED);

    SC_LOGI(WIFI_TAG, "Testing WiFi credentials: %s", ssid.c_str());
    if (WiFi.getMode() != CONST_MODE_AP_STA && WiFi.getMode() != CONST_MODE_STA) {
        WiFi.mode(WIFI_STA);
    }
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(250);
    }

    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected) {
        _wifi_ssid = ssid;
        _wifi_password = password;
        _nvsManager->saveWifiCredentials(ssid, password);
        SC_LOGI(WIFI_TAG, "Credentials verified and saved. IP: %s", WiFi.localIP().toString().c_str());
    } else {
        SC_LOGI(WIFI_TAG, "Could not connect with the given credentials; nothing saved.");
        if (wasConnected && prevSsid.length() > 0) {
            SC_LOGI(WIFI_TAG, "Reconnecting to previous network: %s", prevSsid.c_str());
            WiFi.begin(prevSsid.c_str(), prevPassword.c_str());
        }
    }
    return connected;
}


String WifiManager::getIP()
{
    return _local_ip;
}

String WifiManager::getMacAddress()
{
    return _mac_address;
}
