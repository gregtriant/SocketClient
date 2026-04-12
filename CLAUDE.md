# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SocketClient is a PlatformIO library (Arduino framework) that provides WebSocket connectivity for ESP32/ESP8266 devices to the `sensordata.space` IoT platform. It handles device registration, bidirectional messaging, status reporting, OTA updates, and optional WiFi management with captive portal.

## Build

This is a PlatformIO **library**, not a standalone project. It cannot be built or tested in isolation. To verify compilation, it must be included as a dependency in a consuming PlatformIO project via `lib_deps` in `platformio.ini`.

There are no tests, linter, or CI configured in this repository.

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

### Message Protocol
All messages are JSON over WebSocket. Key message types:
- `connect` — sent on WebSocket connect (includes deviceId, token, version, IP)
- `connected` — received from server with timezone and persisted device data
- `askStatus` / `returningStatus` — server requests status, client responds via the user's `sendStatus` callback
- `command` — server-initiated command dispatched to `receivedCommand` callback
- `entityChanged` — server notifies of entity value change
- `update` — triggers OTA firmware update from a URL
- `@log` / `notification` — client-to-server log and notification messages

### Reconnection Strategy
WebSocketsClient handles basic reconnection (5s interval, heartbeat). The library also has a manual reconnect path (`reconnect()`) with exponential backoff up to 10 minutes, and `stopReconnect()` which sets intervals to `MAX_ULONG` to effectively disable reconnection.

### Platform Abstraction
`SocketClientDefs.h` uses `#if defined(ESP32) || defined(LIBRETUYA)` / `#elif defined(ESP8266)` throughout for platform-specific WiFi, HTTP, and server APIs. LibreTuya boards follow the ESP32 code path.

### Logging
`Log.h` defines `MY_LOG{E,W,I,D,V}(tag, fmt, ...)` macros that write to `Serial.printf` with severity prefix and 4-char tag. Tags: `WIFI`, `WEBS`, ` WS `, ` OTA`, ` NVS`, ` APP`.

### JSON Handling
A single `JsonDocument _doc` member is reused across all message construction and parsing. It is `clear()`'d before each use. `JsonDoc` is a typedef for `JsonVariant` (ArduinoJson v7 reference semantics). `JSON_SIZE` defaults to 4096 but can be overridden by the consumer.

## Key Conventions

- Platform guards use `defined(ESP32) || defined(LIBRETUYA)` for the ESP32 path, `defined(ESP8266)` for the 8266 path, with `#error` fallback.
- Null-safety macros `ASSIGN_IF_NOT_NULLPTR` and `RETURN_IF_NULLPTR` are used throughout init code.
- Forward declarations of `WifiManager` and `WebserverManager` in `SocketClient.h` avoid circular includes; full includes are in the `.cpp`.
