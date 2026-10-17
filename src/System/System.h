// src/System/System.h
#pragma once
#include <Arduino.h>

// The device itself: rebooting it, and why it last booted. "Reboot" is the one word for it
// here, as in /sc/reboot and @reboot; the ESP's own reset reasons map onto RebootReason.
class System {
public:
    // Why the device last booted, as reported in @diagnostics' `rebootReason`. The codes from rebootReasonCode() are the wire format: the server stores
    // them as they are and the web-ui shows them as they come, so a code may be added but never
    // renamed. The chip's own reasons come first, in ESP-IDF's order; Ota and Cmd only exist
    // because reboot() saved them, since the chip reports both as Software.
    enum class RebootReason : uint8_t {
        Unknown,       // "unknown"
        PowerOn,       // "power-on"
        External,      // "external"       reset pin
        Software,      // "software"       a reboot with no more specific reason
        Panic,         // "panic"          crash / exception
        IntWatchdog,   // "int-watchdog"
        TaskWatchdog,  // "task-watchdog"  (ESP8266: its software watchdog)
        Watchdog,      // "watchdog"       any other watchdog
        DeepSleep,     // "deep-sleep"
        Brownout,      // "brownout"
        Sdio,          // "sdio"
        Usb,           // "usb"            ESP-IDF 5+
        Jtag,          // "jtag"           ESP-IDF 5+
        Efuse,         // "efuse"          ESP-IDF 5+
        PowerGlitch,   // "power-glitch"   ESP-IDF 5+
        CpuLockup,     // "cpu-lockup"     ESP-IDF 5+
        Ota,           // "ota"            reboot into new firmware
        Cmd,           // "cmd"            asked for: the @reboot message or /sc/reboot
    };

    // Reboots the device, first saving `reason` so the next boot's rebootReason() reports it.
    // Use it instead of ESP.restart() anywhere in the library.
    static void reboot(RebootReason reason = RebootReason::Software);

    // The reason this boot started with: the chip's, or the one reboot() saved when the chip
    // reports a plain software reboot. The first call reads and clears the saved one, later
    // calls return the same answer. Diagnostics::begin() makes that first call at init().
    static RebootReason rebootReason();

    static const char *rebootReasonCode(RebootReason reason);

private:
    static RebootReason _hardwareRebootReason();
    static bool _rebootReasonRead;
    static RebootReason _rebootReason;
};
