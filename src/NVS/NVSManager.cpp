#include "NVSManager.h"


NVSManager::NVSManager()
{
}


void NVSManager::saveWifiCredentials(String ssid, String password)
{
    _wifi_preferences.begin("WIFIPrefs", RW_MODE);
    _wifi_preferences.putString(NVS_WIFI_SSID_TOKEN, ssid);
    _wifi_preferences.putString(NVS_WIFI_PASSWORD_TOKEN, password);
    _wifi_preferences.end();
}


void NVSManager::getWifiCredentials(String &ssid, String &password)
{
    _wifi_preferences.begin("WIFIPrefs", RW_MODE);
    
    if (true) {
        Serial.println("Initializing NVS for the first time.");
        _wifi_preferences.putString(NVS_WIFI_SSID_TOKEN, "");
        _wifi_preferences.putString(NVS_WIFI_PASSWORD_TOKEN, "");
    }

    ssid = _wifi_preferences.getString(NVS_WIFI_SSID_TOKEN);
    password = _wifi_preferences.getString(NVS_WIFI_PASSWORD_TOKEN);
    _wifi_preferences.end();
}
