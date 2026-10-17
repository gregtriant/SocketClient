// src/Diagnostics/Diagnostics.cpp
#include "Diagnostics.h"
#include "../System/System.h"

Diagnostics::Diagnostics(NVSManager* nvsManager, SendFn sendFn)
    : _nvsManager(nvsManager), _sendFn(sendFn) {}

void Diagnostics::begin() {
    _rebootReason = System::rebootReasonCode(System::rebootReason());
    // Before 1.9.0 the count was kept under "resetCount". Its value moves into rebootCount once,
    // then the old key is deleted. Added rather than copied: 1.9.0 itself counted afresh under
    // the new key without deleting the old one, so a device that ran it holds part of its
    // count in each.
    uint32_t oldCount = _nvsManager->getUInt(NVS_DIAG_NAMESPACE, NVS_DIAG_OLD_RESET_KEY, 0);
    _rebootCount = _nvsManager->getUInt(NVS_DIAG_NAMESPACE, NVS_DIAG_REBOOT_KEY, 0) + oldCount + 1;
    _nvsManager->putUInt(NVS_DIAG_NAMESPACE, NVS_DIAG_REBOOT_KEY, _rebootCount);
    if (oldCount) _nvsManager->remove(NVS_DIAG_NAMESPACE, NVS_DIAG_OLD_RESET_KEY);
}

// Called from the WStype_CONNECTED event.
void Diagnostics::onSocketConnected() {
    _reconnectCount++;
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
    doc["rebootReason"]   = _rebootReason;
    doc["rebootCount"]    = _rebootCount;
    doc["reconnectCount"] = _reconnectCount;

    String output;
    serializeJson(doc, output);
    _sendFn(output);
    _lastSent = millis();
    _hasSent = true;
}
