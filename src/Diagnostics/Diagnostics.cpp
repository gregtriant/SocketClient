// src/Diagnostics/Diagnostics.cpp
#include "Diagnostics.h"

Diagnostics::Diagnostics(NVSManager* nvsManager, SendFn sendFn)
    : _nvsManager(nvsManager), _sendFn(sendFn) {}

#if defined(ESP32) || defined(LIBRETUYA)
String Diagnostics::_resetReasonToString(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:   return "power-on";
        case ESP_RST_EXT:       return "external";
        case ESP_RST_SW:        return "software";
        case ESP_RST_PANIC:     return "panic";
        case ESP_RST_INT_WDT:   return "int-watchdog";
        case ESP_RST_TASK_WDT:  return "task-watchdog";
        case ESP_RST_WDT:       return "watchdog";
        case ESP_RST_DEEPSLEEP: return "deep-sleep";
        case ESP_RST_BROWNOUT:  return "brownout";
        case ESP_RST_SDIO:      return "sdio";
        default:                return "unknown";
    }
}
#endif

void Diagnostics::begin() {
#if defined(ESP32) || defined(LIBRETUYA)
    _resetReason = _resetReasonToString(esp_reset_reason());
#else
    _resetReason = ESP.getResetReason();
#endif
    _resetCount = _nvsManager->getUInt(NVS_DIAG_NAMESPACE, NVS_DIAG_RESET_KEY, 0) + 1;
    _nvsManager->putUInt(NVS_DIAG_NAMESPACE, NVS_DIAG_RESET_KEY, _resetCount);
}

void Diagnostics::onConnected(bool enabled) {
    _enabled = enabled;
    if (_enabled) _send();
}

void Diagnostics::onDebugConfig(bool enabled) {
    bool wasEnabled = _enabled;
    _enabled = enabled;
    if (_enabled && !wasEnabled) _send();
}

void Diagnostics::loop() {
    if (!_enabled) return;
    if (!_hasSent || millis() - _lastSent >= INTERVAL) {
        _send();
    }
}

void Diagnostics::_send() {
    JsonDocument doc;
    doc["message"]     = "@diagnostics";
    doc["rssi"]        = WiFi.RSSI();
    doc["freeHeap"]    = (uint32_t)ESP.getFreeHeap();
#if defined(ESP32) || defined(LIBRETUYA)
    doc["minFreeHeap"] = (uint32_t)ESP.getMinFreeHeap();
#endif
    doc["resetReason"] = _resetReason;
    doc["resetCount"]  = _resetCount;

    String output;
    serializeJson(doc, output);
    _sendFn(output);
    _lastSent = millis();
    _hasSent = true;
}
