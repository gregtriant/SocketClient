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
    // check if we have credentials in NVS
    if (!_wifi_ssid.isEmpty() && !_wifi_password.isEmpty()) {
        MY_LOGI(WIFI_TAG, "WiFi credentials found in NVS.");
        // MY_LOGI(WIFI_TAG, "SSID: %s", _wifi_ssid.c_str());
        // MY_LOGI(WIFI_TAG, "Password: %s", _wifi_password.c_str());
        _connectingToWifi(_wifi_ssid, _wifi_password);
    } else {
        MY_LOGW(WIFI_TAG, "No WiFi credentials found in NVS. Please set them.");
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

        MY_LOGI(WIFI_TAG, "WiFi connection lost or disconnected. Trying to reconnect...");
        _wifi_status = wifiStatus;
        _ap_time = now; // Reset AP time.
        if (_onInternetLost) {
            _onInternetLost();
        }
    } else if ((_wifi_status != WL_CONNECTED) &&
               (wifiStatus == WL_CONNECTED)) {
        _wifiConnected();
    }

    if (now - _ap_time > 30000 && wifiStatus != WL_CONNECTED) { // if in AP mode for more than 30 sec, try to connect with old credentials
        _ap_time = now;
        MY_LOGI(WIFI_TAG, "30 seconds in ap mode... Connecting...");
        init(); // connect with old credentials
    }

    if (_connecting_time != 0) { // trying to connect to wifi
        if (now - _connecting_time > 1000) {
            _connecting_time = now;
            _connecting_attempts++;
            // TODO: Here toggle connection led
            MY_LOGI(WIFI_TAG, "%d...", _connecting_attempts);
            if (_connecting_attempts >= 15) {
                _connecting_time = 0;
                _connecting_attempts = 0;
                _wifi_status = WL_CONNECTION_LOST;
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
    MY_LOGI(WIFI_TAG, "Connecting to WiFi: %s", _wifi_ssid.c_str());
    if (WiFi.getMode() != CONST_MODE_AP_STA && WiFi.getMode() != CONST_MODE_STA) {
        WiFi.mode(WIFI_STA);
    }
    WiFi.begin(ssid.c_str(), password.c_str());
}


void WifiManager::_wifiConnected()
{
    if (WiFi.getMode() == CONST_MODE_AP_STA) {
        MY_LOGI(WIFI_TAG, "Stopping AP+STA mode...");
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
    }
    _connecting_time = 0;     // Means connected.
    _connecting_attempts = 0; // Reset connecting attempts.
    MY_LOGI(WIFI_TAG, "Connected to %s! IP address: %s", _wifi_ssid.c_str(), WiFi.localIP().toString().c_str());
    _local_ip = WiFi.localIP().toString();
    _wifi_status = WiFi.status();
    if (_onInternetRestored) {
        _onInternetRestored();
    }
}


void WifiManager::_initAPMode()
{
    if (WiFi.getMode() == CONST_MODE_AP_STA) {
        MY_LOGI(WIFI_TAG, "Already in AP+STA mode. Skipping...");
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

    MY_LOGI(WIFI_TAG, "AP ssid: %s", _ap_ssid.c_str());
    MY_LOGI(WIFI_TAG, "AP pass: %s", _ap_password.c_str());
    WiFi.softAP(_ap_ssid, _ap_password); // AP name and password

    MY_LOGI(WIFI_TAG, "Starting AP+STA mode... IP: %s", WiFi.softAPIP().toString().c_str());

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


String WifiManager::getIP()
{
    return _local_ip;
}

String WifiManager::getMacAddress()
{
    return _mac_address;
}
