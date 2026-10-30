# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SocketClient is a PlatformIO library (Arduino framework) that provides WebSocket connectivity for ESP32/ESP8266 devices to the `sensordata.space` IoT platform. It handles device registration, bidirectional messaging, status reporting, OTA updates, and optional WiFi management with captive portal.

## Build

This is a PlatformIO **library**, not a standalone project. It cannot be built or tested in isolation. To verify compilation, it must be included as a dependency in a consuming PlatformIO project via `lib_deps` in `platformio.ini`.

There are no tests, linter, or CI configured in this repository. The library version is tracked in `library.json`.

## Architecture

### Singleton Pattern
`SocketClient` enforces a single instance via a static counter in the constructor (exits on second instantiation). A global pointer `globalSC` bridges the C-style WebSocket event callback (`SocketClient_webSocketEvent`) to the instance, since the WebSocketsClient library requires a free function callback.

### Two Init Paths
- **Config struct** (`init(const SocketClientConfig_t*)`) — preferred; sets all options including callback functions in one shot
- **Manual** (`init(host, port, ssl)` + individual setters) — legacy path; requires calling `setToken()`, `setSendStatusFunction()`, etc. before init

### Optional WiFi Management (`handleWifi`)
When `config.handleWifi = true`, SocketClient owns the full WiFi lifecycle:
- `NVSManager` — persists WiFi credentials in flash (Preferences library)
- `WifiManager` — connects to saved WiFi, falls back to AP mode with captive portal for credential entry
- `WebserverManager` — serves the captive portal UI and a status endpoint

When `handleWifi = false`, these managers are not instantiated and the consumer is responsible for WiFi. The library reads `WiFi.macAddress()` and `WiFi.localIP()` directly regardless.

The AP SSID is `deviceType-deviceApp` and the AP is an open network (no password) by default. A consumer can opt into a WPA2 password via `SocketClient::setPasswordAP(const char*)` (called before `init()`, like `setToken()`), which is passed down to `WifiManager::setApPassword()`; passwords outside the WPA2-PSK 8-63 char range are rejected and the AP falls back to open. The WiFi hostname is also set to `deviceType-deviceApp`.

AP+STA fallback is final for the boot: once `WifiManager` falls back to AP+STA mode, it will not automatically retry the saved credentials underneath it (`WifiManager::isApStaFinal()`). The only ways out are a client submitting new credentials via `/sc/wifi/connect`, or a reboot.

### Message Protocol
All messages are JSON over WebSocket. Key message types:
- `connect` — sent on WebSocket connect (includes deviceId, token, version, IP)
- `connected` — received from server with timezone and persisted device data
- `askStatus` / `returningStatus` — server requests status, client responds via the user's `sendStatus` callback. `askStatus` is always answered, `checkSubs` or not: the server only forwards it from a browser already in this device's subscriber list, so receiving one is itself proof somebody is watching
- `@subs` — how many browsers have this device open (`{ message: "@subs", count: N }`). Sent by the server on every change and once right after `connected`, count 0 included. See **Subscriber count** below
- `command` — server-initiated command dispatched to `receivedCommand` callback; carries `user: { idx, role }` (`idx` = the sender's number on this device, 1 for its first user and never reused; `role` = `"admin"` / `"user"`), set by the server from the sender's session and absent if unknown; the library passes it through untouched in `doc`
- `@reboot` — sent by the browser's Reboot button, forwarded by the server like `command` (server needs `'@reboot': forwardMessageToDevice` in its handler map). Never reaches the app: it calls `reboot(System::RebootReason::Cmd)`, and the next `loop()` closes the socket and calls `System::reboot()` (named after the local `/sc/reboot`). A reconnect needs no device message: the browser's `closeConnection` has the server close the socket, and the device reconnects on its own
- `entityChanged` — server notifies of entity value change; carries `user` like `command`
- `requestFile` — the browser asks the device for a file; carries `user` like `command` (the
  server adds it in `handleRequestFile` via `messageForDevice`, so a browser cannot claim to
  be someone else). See **File transfer** below
- `update` — triggers OTA firmware update from a URL
- `@log` / `notification` — client-to-server log and notification messages

### Subscriber count

`sendStatusWithSocket(bool save = false, bool checkSubs = true)` skips the *send* when no
browser has the device open, so a device with nobody watching stops paying for live status.

What it does **not** skip is building the status: `sendStatus()` — the consumer's callback —
runs either way, and the JSON is serialized as before. Only `sendTXT` is skipped. That is
deliberate: a callback that silently stops firing is a trap for the consumer, who may be doing
their own bookkeeping in there. A sketch that wants to skip the work too (reading sensors is
usually the expensive part, not the bytes) checks `getSubscriberCount()` first and doesn't
call at all.

Two statuses are never gated:
- `save = true` — that status is for the database, and the server saves it regardless of who
  is subscribed (see `statusDisposition` on the server). Gating it would silently lose
  recorded data whenever nobody happened to be looking.
- `checkSubs = false` — the explicit opt-out, for a sketch that wants the status on the wire
  whatever the count.

The count is `0` until the server says otherwise, and resets to `0` on disconnect. **This
makes the server a hard dependency:** a server that never sends `@subs` leaves a `checkSubs`
device permanently silent on live status. Deploy the server before shipping firmware 1.12.0,
the same ordering the 1.10.0 file-transfer change needed. The reset on disconnect matters
because the server sends `@subs` right after `connected`, while a sketch's `connected()`
callback runs before that message is processed — without the reset, the first status of a new
connection would be judged by the old socket's number.

### File transfer

One binary WebSocket message per file, in either direction: `[uint16 big-endian header
length][JSON header][raw bytes]`. The header carries `message: "file"`, `filename`, `size`,
and for uploads `requestId` and an optional `contentType`. Files are capped at 4096 bytes
(`FileWire::MAX_SIZE`) and the server enforces the same limit.

Both directions tell the device **who** did it, as the same `user: { idx, role }` object
`command` and `entityChanged` carry: the server puts it on the forwarded `requestFile`
message, and in the header of a file a browser uploads (`POST /:id/files/send`, where
`deviceAccess()` both checks access and says who the user is). A header with no `user` means
the server could not say — an older server, or a request with no session behind it.

So since 1.11.0 the sketch's two callbacks take the **header**, not just the filename:

```cpp
void fileReceived(JsonDoc header, const std::vector<uint8_t> &buf);  // header as it arrived
void fileRequested(JsonDoc header, std::vector<uint8_t> &buf);       // header it will be sent with
```

`fileRequested` gets the outgoing header, so a sketch renames the file with
`header["filename"] = "cfg.txt"` and sets `header["contentType"]` directly — which is why the
old `"cfg.txt|text/plain"` smuggling convention is gone. `message` and `size` are set by the
library at send time. `requestId` is read before the callback and written back after, so a
sketch that rewrites or clears the header cannot leave the browser unable to match the reply
to its request. The header must not be stored past the callback's return, like every other
`JsonDoc`.

The code is deliberately split in two:

- **`src/FileTransfer/FileWire.{h,cpp}`** — the wire format alone, as free functions in a
  `FileWire` namespace: `build()`, `parse()`, `MAX_SIZE`, plus `reserve()`/`assign()`, which
  are `std::vector` calls that report an allocation failure instead of aborting the device
  (arduino-esp32 builds with exceptions, ESP8266 without; the macro in the `.cpp` covers
  both). Nothing here knows about `SocketClient`, the socket or the sketch's callbacks.
  The same format is implemented in sensordata2-server's `utils/fileWire.js` (tested in
  `test/fileWire.test.js`) and web-ui's `utils/fileWire.js` — **three implementations of one
  contract**, so a change to the header or the length prefix needs the same change in all three.
- **The orchestration stays in `SocketClient`** — who asked for a file, when it is sent, and
  on which thread. A `requestFile` only records the request in `s_uploadReq`; `loop()` sends
  it on its next pass, after `_webSocket->loop()` has returned, so the socket read, the event
  dispatch and the parsed message are not all sitting under the sketch's file callback on
  `loop()`'s stack. One at a time: a second request while one is waiting gets `fileError`
  with `"busy"`. `s_uploadReq` copies the `user` out of the message rather than referencing
  it, because `_doc` is cleared as soon as the message is dispatched — long before `loop()`
  builds the header. `_sendFile`/`_sendFileError` use a local `JsonDocument`, never the shared
  `_doc`, which still holds the inbound message. Inbound frames are parsed in the
  `WStype_BIN` case and handed to the sketch's `fileReceived`.

That boundary is where a future multi-chunk transfer would go: the codec is already out, so a
`FileTransfer` class owning the per-transfer state (sequence numbers, reassembly buffer,
timeouts) would take over the orchestration without touching the format. Keep the
loop()-thread rule with it if it moves.

### Reboot reasons
"Reboot" is the library's one word for it (as in `/sc/reboot` and `@reboot`), never reset or restart; the ESP's reset reasons and `ESP.restart()` are mapped onto it. The only exception is the ESP APIs. Since 1.9.0 `@diagnostics` sends `rebootReason` / `rebootCount` (before: `resetReason` / `resetCount`, which the server still accepts) and the count lives under NVS `sc-diag`/`rebootCount`: from 1.9.1 `Diagnostics::begin()` adds any old `resetCount` value into it and deletes the old key, so counts carry on (1.9.0 started them over without deleting the old key, hence adding rather than copying). `System` (`src/System/System.h`, all static) is the device itself: rebooting it and why it last booted. `@diagnostics` reports `rebootReason` as a code from `System::RebootReason`. The codes are the wire format: the server stores them unchanged and sensordata2's web-ui shows them as they come (Diagnostics tooltip), so a code may be added but never renamed. The chip only knows its own reasons and reports every `ESP.restart()` as `software`, so the library reboots through `System::reboot(reason)` instead: it saves the reason in NVS (`sc-diag`/`rebootReason`, enum + 1, skipped for plain `Software`), and the first `System::rebootReason()` of the boot (from `Diagnostics::begin()` in `init()`) uses it only if the chip agrees it was a software reboot, then clears it; later calls return the cached answer. `ota` = reboot into new firmware (server OTA or `/sc/upload`), `cmd` = asked for (`@reboot` or `/sc/reboot`), new-WiFi-credentials reboots stay `software`. Never call `ESP.restart()` directly in the library. ESP8266 maps its `rst_info` reasons onto the same codes (it used to send `ESP.getResetReason()`'s free text). ESP8266's own `ESPhttpUpdate` reboots by itself, so an OTA there still reads `software`.

### Reconnection Strategy
WebSocketsClient handles basic reconnection (5s interval, heartbeat). The library also has a manual reconnect path (`reconnect()`) with exponential backoff up to 10 minutes, and `stopReconnect()` which sets intervals to `MAX_ULONG` to effectively disable reconnection.

**Live on/off switches.** `socketOn()`/`socketOff()` and `wifiOn()`/`wifiOff()` (getters `isSocketOn()`/`isWifiOn()`) are callable from any task: they only set `_socket_want`/`_wifi_want`, and `_applyOnOff()` at the top of `loop()` does the work. Nothing is saved; both are on at boot. Socket off = `_webSocket->disconnect()`, and while `_socket_on` is false `reconnect()` and `watchdog()` return at once and `_webSocket->loop()` isn't pumped (that loop is what reconnects a closed socket). WiFi off = `WifiManager::setEnabled(false)`: `WiFi.disconnect(true)` (and the AP down), `loop()` idle so nothing retries; `setEnabled(true)` starts over from the saved credentials as at boot (`init()`; without `handleWifi`, `WiFi.begin()` with the stack's own credentials). The socket runs only while both are wanted, so WiFi off closes it first and WiFi on brings it back through the usual `_onInternetRestored` path.

`WifiManager::_wifiConnected()` disables WiFi modem sleep (`WiFi.setSleep(false)` on ESP32/LibreTuya, `WiFi.setSleepMode(WIFI_NONE_SLEEP)` on ESP8266) every time WiFi connects - a reasonable reliability trade-off regardless (modem sleep's low-power listen cycle can drop a UDP reply while the radio is asleep), though it turned out not to be the cause of the persistent `hostByName()`/`DNS Failed` errors this was chasing down (reproduced on both old and new arduino-esp32 cores alike).

`WifiManager::_connectingToWifi()` calls `dns_clear_cache()` (lwIP, `<lwip/dns.h>`) immediately before every `WiFi.begin()` - first boot, a periodic retry, or a user submitting new credentials via `/sc/wifi/connect`. Root cause of the DNS failures above: lwIP's resolver cache is keyed by hostname only, with no notion of which network a result was resolved on, so switching to a different WiFi network mid-session left it serving a cached entry (or a cached failure) from the *previous* network. arduino-esp32 3.x's newer `NetworkManager::hostByName()` already clears this cache automatically on an interface IP change; this library still supports the older 2.x cores that don't, so it's done explicitly here instead - unconditionally per connection attempt rather than trying to detect the IP change itself.

### Platform Abstraction
`SocketClientDefs.h` uses `#if defined(ESP32) || defined(LIBRETUYA)` / `#elif defined(ESP8266)` throughout for platform-specific WiFi, HTTP, and server APIs. LibreTuya boards follow the ESP32 code path.

### Logging
`Log.h` defines `MY_LOG{E,W,I,D,V}(tag, fmt, ...)` macros that write to `Serial.printf` with severity prefix and 4-char tag. Tags: `WIFI`, `WEBS`, ` WS `, ` OTA`, ` NVS`, ` APP`, `MQTT`.

### JSON Handling
A single `JsonDocument _doc` member is reused across all message construction and parsing. It is `clear()`'d before each use. `JsonDoc` is a typedef for `JsonVariant` (ArduinoJson v7 reference semantics). `JSON_SIZE` defaults to 4096 but can be overridden by the consumer. **Callbacks must not store the `JsonDoc` reference past their return** — `_doc` is cleared immediately after dispatch.

### TimeClient
`_tc` (a `TimeClient` member) syncs time via NTP after the server sends a `connected` message with timezone. Public API: `hasTime()`, `getTime(hh, mm, ss)`, `getDate(yy, mm, dd)`. Time is only valid after a successful `connected` message is received.

The timezone is owned and persisted by SocketClient, in the `NVS_SC_NAMESPACE` namespace next to the WiFi credentials (the stored name is still `"WIFIPrefs"` — only the constant was renamed, since renaming the namespace itself would make deployed devices lose their credentials). Two POSIX TZ strings in fixed `char[SC_TZ_LEN]` buffers: `_tz_srv` (key `tz_srv`), the last zone the server sent, and `_tz_set` (key `tz_set`), an optional override; the zone in effect is `_tz_set` if present, else `_tz_srv`. `init()` loads both (`_tzLoad()`) and applies the zone right after `NVSManager` is created, so local time is right before or without the server — NTP still only starts on `connected`, where `gotMessageSocket()` records a changed server zone and calls `_tc.begin()` with whichever zone is in effect. `setTZ(tz, keep = true)` validates (`validTZ()`) and stores a kept zone in `_tz_set`; `keep = false` clears the kept one and puts the zone in `_tz_srv` as a stand-in until the server sends its own; `setTZ(nullptr)` returns to the server's zone. `setTZ()`, `getTZ(buf, n, &pinned)` and `isTZPinned()` are callable from any task (spinlock `s_tzMux` on ESP32): they only change/read the buffers and set flags, and `_tzFlush()` in `loop()` does the `setenv`/`tzset` and the NVS writes, since `NVSManager` shares one `Preferences` object and isn't thread-safe. The `const char*` getters `getTZ()` / `getServerTZ()` are loop-thread only.

### EventHandler (standalone pub/sub module)
`src/EventHandler/EventHandler.h` is a header-only, templated publish/subscribe event bus (`EventHandler<CategoryT>`) that is **independent of `SocketClient`** — any ESP32/LibreTuya project can include it for inter-task pub/sub without pulling in WebSocket/WiFi functionality. `CategoryT` is injected by the consuming app as a template parameter (typically its own `enum class`); event ids within a category are a plain `uint16_t`. `init()` starts a dedicated FreeRTOS task that runs all matching callbacks serially, regardless of which task called `publish()`. `publish()` transfers ownership of a `new uint8_t[]`-allocated payload to `EventHandler`, which frees it with `delete[]` right after dispatch (or immediately, if the queue was full). Subscriptions live in a mutex-protected linked list with no `unsubscribe()`; re-subscribing the same (category, eventId) key updates the callback in place. **ESP32/LibreTuya only** — relies on FreeRTOS task/queue/semaphore APIs not available on ESP8266, the same constraint already documented below for `HAMqtt`. Not yet wired into `SocketClient`'s own callbacks (`connected`, `receivedCommand`, etc.) — that integration is a deliberate follow-up, not part of this module.

### Optional HA MQTT Module (`SC_ENABLE_HA_MQTT`)
Enabled by adding `-D SC_ENABLE_HA_MQTT` to `build_flags`. When enabled:
- `HAMqttConfig_t` must be populated and passed to `HAMqtt` directly (no field on `SocketClientConfig_t` yet)
- `SocketClient` instantiates `HAMqtt` and exposes it via `getMqttClient()`
- `HAMqtt` auto-reconnects every 5s, publishes Home Assistant MQTT autodiscovery on connect, and exposes `addEntity()` / `publishEvent()` / `loop()`
- **ESP32/LibreTuya only** — `HAMqtt.cpp` includes `<WiFi.h>` directly without platform guard; not compatible with ESP8266
- `MAX_HA_ENTITIES` is 8 (compile-time fixed array)

## Key Conventions

- Platform guards use `defined(ESP32) || defined(LIBRETUYA)` for the ESP32 path, `defined(ESP8266)` for the 8266 path, with `#error` fallback.
- Null-safety macros `ASSIGN_IF_NOT_NULLPTR` and `RETURN_IF_NULLPTR` are used throughout init code.
- Forward declarations of `WifiManager` and `WebserverManager` in `SocketClient.h` avoid circular includes; full includes are in the `.cpp`.
- `initWebserver(port)` can be called independently of `handleWifi` to start only the webserver (e.g. to add custom routes via `getServer()`).
- `getServer()` returns `AsyncWebServer*` on both platforms (ESPAsyncWebServer is used throughout).
