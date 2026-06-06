#include "NVSManager.h"

NVSManager::NVSManager() {}

NVSManager::~NVSManager() {}

uint32_t NVSManager::getUInt(const char* ns, const char* key, uint32_t defaultValue) {
    _prefs.begin(ns, RO_MODE);
    uint32_t val = _prefs.getUInt(key, defaultValue);
    _prefs.end();
    return val;
}

void NVSManager::putUInt(const char* ns, const char* key, uint32_t value) {
    _prefs.begin(ns, RW_MODE);
    _prefs.putUInt(key, value);
    _prefs.end();
}

String NVSManager::getString(const char* ns, const char* key, const String& defaultValue) {
    _prefs.begin(ns, RO_MODE);
    String val = _prefs.getString(key, defaultValue.c_str());
    _prefs.end();
    return val;
}

void NVSManager::putString(const char* ns, const char* key, const String& value) {
    _prefs.begin(ns, RW_MODE);
    _prefs.putString(key, value.c_str());
    _prefs.end();
}

void NVSManager::saveWifiCredentials(String ssid, String password) {
    putString(NVS_WIFI_NAMESPACE, NVS_WIFI_SSID_TOKEN, ssid);
    putString(NVS_WIFI_NAMESPACE, NVS_WIFI_PASSWORD_TOKEN, password);
}

void NVSManager::getWifiCredentials(String& ssid, String& password) {
    ssid     = getString(NVS_WIFI_NAMESPACE, NVS_WIFI_SSID_TOKEN, "");
    password = getString(NVS_WIFI_NAMESPACE, NVS_WIFI_PASSWORD_TOKEN, "");
}
