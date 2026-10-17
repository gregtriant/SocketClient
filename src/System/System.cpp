// src/System/System.cpp
#include "System.h"
#include "../NVS/NVSManager.h"
#include "../Diagnostics/Diagnostics.h"   // NVS_DIAG_NAMESPACE

#if defined(ESP32) || defined(LIBRETUYA)
#include <esp_system.h>
#endif

// Saved by reboot(), read back by rebootReason() on the next boot. Kept in the diagnostics
// namespace, since diagnostics are what report it.
#define NVS_DIAG_REASON_KEY "rebootReason"

bool System::_rebootReasonRead = false;
System::RebootReason System::_rebootReason = System::RebootReason::Unknown;

const char *System::rebootReasonCode(RebootReason reason) {
    switch (reason) {
        case RebootReason::PowerOn:      return "power-on";
        case RebootReason::External:     return "external";
        case RebootReason::Software:     return "software";
        case RebootReason::Panic:        return "panic";
        case RebootReason::IntWatchdog:  return "int-watchdog";
        case RebootReason::TaskWatchdog: return "task-watchdog";
        case RebootReason::Watchdog:     return "watchdog";
        case RebootReason::DeepSleep:    return "deep-sleep";
        case RebootReason::Brownout:     return "brownout";
        case RebootReason::Sdio:         return "sdio";
        case RebootReason::Usb:          return "usb";
        case RebootReason::Jtag:         return "jtag";
        case RebootReason::Efuse:        return "efuse";
        case RebootReason::PowerGlitch:  return "power-glitch";
        case RebootReason::CpuLockup:    return "cpu-lockup";
        case RebootReason::Ota:          return "ota";
        case RebootReason::Cmd:          return "cmd";
        default:                        return "unknown";
    }
}

System::RebootReason System::_hardwareRebootReason() {
#if defined(ESP32) || defined(LIBRETUYA)
    // Numbers rather than names past ESP_RST_SDIO: ESP-IDF 5 appended USB, JTAG, EFUSE,
    // PWR_GLITCH and CPU_LOCKUP in that order, and the 4.4 cores this still builds on don't
    // define them.
    switch ((int)esp_reset_reason()) {
        case ESP_RST_POWERON:   return RebootReason::PowerOn;
        case ESP_RST_EXT:       return RebootReason::External;
        case ESP_RST_SW:        return RebootReason::Software;
        case ESP_RST_PANIC:     return RebootReason::Panic;
        case ESP_RST_INT_WDT:   return RebootReason::IntWatchdog;
        case ESP_RST_TASK_WDT:  return RebootReason::TaskWatchdog;
        case ESP_RST_WDT:       return RebootReason::Watchdog;
        case ESP_RST_DEEPSLEEP: return RebootReason::DeepSleep;
        case ESP_RST_BROWNOUT:  return RebootReason::Brownout;
        case ESP_RST_SDIO:      return RebootReason::Sdio;
        case 11:                return RebootReason::Usb;
        case 12:                return RebootReason::Jtag;
        case 13:                return RebootReason::Efuse;
        case 14:                return RebootReason::PowerGlitch;
        case 15:                return RebootReason::CpuLockup;
        default:                return RebootReason::Unknown;
    }
#elif defined(ESP8266)
    switch (ESP.getResetInfoPtr()->reason) {
        case REASON_DEFAULT_RST:      return RebootReason::PowerOn;
        case REASON_WDT_RST:          return RebootReason::Watchdog;
        case REASON_EXCEPTION_RST:    return RebootReason::Panic;
        case REASON_SOFT_WDT_RST:     return RebootReason::TaskWatchdog;
        case REASON_SOFT_RESTART:     return RebootReason::Software;
        case REASON_DEEP_SLEEP_AWAKE: return RebootReason::DeepSleep;
        case REASON_EXT_SYS_RST:      return RebootReason::External;
        default:                      return RebootReason::Unknown;
    }
#else
    return RebootReason::Unknown;
#endif
}

System::RebootReason System::rebootReason() {
    if (_rebootReasonRead) return _rebootReason;
    _rebootReasonRead = true;

    RebootReason reason = _hardwareRebootReason();
    NVSManager nvs;
    uint32_t saved = nvs.getUInt(NVS_DIAG_NAMESPACE, NVS_DIAG_REASON_KEY, 0);
    if (saved) {
        // Saved as the enum + 1, 0 meaning none. It names this boot only if the chip agrees
        // it was a software reboot: after a crash or power cut in between it is stale.
        if (reason == RebootReason::Software && saved - 1 <= (uint32_t)RebootReason::Cmd) {
            reason = (RebootReason)(saved - 1);
        }
        nvs.putUInt(NVS_DIAG_NAMESPACE, NVS_DIAG_REASON_KEY, 0);
    }
    _rebootReason = reason;
    return reason;
}

void System::reboot(RebootReason reason) {
    // Software is what the chip reports anyway, so nothing needs saving (or writing to flash).
    if (reason != RebootReason::Software) {
        NVSManager nvs;
        nvs.putUInt(NVS_DIAG_NAMESPACE, NVS_DIAG_REASON_KEY, (uint32_t)reason + 1);
    }
    ESP.restart();
}
