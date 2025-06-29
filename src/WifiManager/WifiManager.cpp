#include "WifiManager.h"


WifiManager::WifiManager(NVSManager *nvsManager, const String& ap_ssid, const String& ap_password, std::function<void()> onInternetRestored)
{
    _nvsManager = nvsManager;
    _ap_ssid = ap_ssid;
    _ap_password = ap_password;
    _onInternetRestored = onInternetRestored;
    
    _mac_address = WiFi.macAddress();
    _local_ip = WiFi.localIP().toString(); // in case already connected to wifi
    _nvsManager->getWifiCredentials(_wifi_ssid, _wifi_password);
}


void WifiManager::init()
{
    // check if we have credentials in NVS
    if (_wifi_ssid.isEmpty() || _wifi_password.isEmpty()) {
        Serial.println("No WiFi credentials found in NVS. Please set them.");
        _initAPMode();
        return;
    } else {
        Serial.println("\nWiFi credentials found in NVS.");
        Serial.println("SSID: " + _wifi_ssid);
        Serial.println("Password: " + _wifi_password);
        _connectingToWifi(_wifi_ssid, _wifi_password);
    }
}


void WifiManager::loop()
{
    uint64_t now = millis();
    wl_status_t wifiStatus = WiFi.status();
    int wifiMode = (int)WiFi.getMode();

    if (WiFi.getMode() == CONST_MODE_AP) {
        
        if (now - _ap_time > 2000) { 
            _ap_time = now;
            Serial.println("Ap mode");
        }

    } else if (WiFi.getMode() == CONST_MODE_STA && _connecting_time != 0) {

        if (now - _connecting_time > 1000) {
            _connecting_time = now;
            Serial.println("STA mode");
        }

        if (wifiStatus == WL_CONNECTED) {
            _wifiConnected();
        }
    }
    // Serial.println("wifi status: " + String(wifiStatus));
}


void WifiManager::_connectingToWifi(String ssid, String password)
{
    _wifi_ssid = ssid;
    _wifi_password = password;
    _connecting_time = millis();
    
    Serial.println("Connecting to WiFi: " + _wifi_ssid);
    if(WiFi.getMode() == CONST_MODE_AP) {
        WiFi.softAPdisconnect(true); // stop AP mode
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
}


void WifiManager::_wifiConnected()
{
    _connecting_time = 0; // means connected
    Serial.print("Connected to ");
    Serial.print(_wifi_ssid);
    Serial.print("!  IP address:  ");
    Serial.println(WiFi.localIP());
    _local_ip = WiFi.localIP().toString();
    if (_onInternetRestored) {
        _onInternetRestored();
    }
}


void WifiManager::_initAPMode()
{
    // stop STA mode
    WiFi.disconnect(true);
    // WiFi.mode(WIFI_AP); // set to AP mode
    // if in AP_STA mode make sure in same channel
    IPAddress apIP(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, subnet);

    Serial.println("AP ssid: " + _ap_ssid);
    Serial.println("AP password: " + _ap_password);
    WiFi.softAP(_ap_ssid, _ap_password); // AP name and password

    Serial.print("Starting AP mode... ");
    Serial.println(WiFi.softAPIP());

    _ap_time = millis();
}


String WifiManager::getIP()
{
    return _local_ip;
}

String WifiManager::getMacAddress()
{
    return _mac_address;
}
