# SocketClient

A PlatformIO library for ESP32/ESP8266 that connects your device to the [sensordata.space](https://sensordata.space) server over an SSL WebSocket. Handles WiFi, OTA updates, a built-in web server, NTP time sync, remote debug logging, and optional Home Assistant MQTT autodiscovery — so your firmware can focus on application logic.

## Features

- Bidirectional WebSocket protocol with JSON messaging
- Managed WiFi with captive portal and NVS credential persistence (`handleWifi: true`)
- Built-in async web server (WiFi config, OTA upload, device info, reboot)
- NTP time sync with server-provided timezone
- Remote debug logging with server-side level filtering
- OTA firmware updates triggered by the server
- Home Assistant MQTT autodiscovery (optional)
- ESP32 and ESP8266 support

## Installation

Add to `lib_deps` in your `platformio.ini`:

```ini
lib_deps =
    https://github.com/gregtriant/SocketClient.git
```

See [`library.json`](library.json) for all transitive dependencies.

## Quick Start

```cpp
#include <SocketClient.h>

SocketClient sc;

SocketClientConfig_t config = {
    .name        = "MyDevice",
    .version     = 1.0,
    .type        = "ESP32",
    .ledPin      = -1,
    .host        = "api.sensordata.space",
    .port        = 443,
    .isSSL       = true,
    .token       = "your-token-here",
    .handleWifi  = true,
    .sendStatus  = [](JsonDoc doc) { doc["state"] = "ok"; },
    .receivedCommand = [](JsonDoc doc) { /* handle commands */ },
    .entityChanged   = [](JsonDoc doc) { },
    .connected       = [](JsonDoc doc) { },
};

void setup() {
    sc.init(&config);
    sc.initWebserver(80);   // optional — serves WiFi config, OTA, reboot pages
}

void loop() {
    sc.loop();
}
```

## Configuration

All options are set via `SocketClientConfig_t`:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `name` | `const char *` | `"SocketClient"` | Application name shown on the dashboard |
| `version` | `float` | `0.2` | Firmware version |
| `type` | `const char *` | `"ESP32"` | Device type identifier |
| `ledPin` | `int` | `-1` | Status LED pin (active-low); `-1` to disable |
| `host` | `const char *` | `"sensordata.space"` | WebSocket server hostname |
| `port` | `int` | `80` | Server port (`443` for SSL) |
| `isSSL` | `bool` | `true` | Use SSL/TLS |
| `token` | `const char *` | `""` | Authentication token |
| `handleWifi` | `bool` | `false` | Let the library manage WiFi, NVS, and AP mode |
| `sendStatus` | callback | — | Called when server requests device status |
| `receivedCommand` | callback | — | Called when server sends a command |
| `entityChanged` | callback | — | Called when a tracked entity changes on the server |
| `connected` | callback | — | Called after successful server handshake |

## Public API

### Core

```cpp
void init(const SocketClientConfig_t *config);
void loop();                            // call from Arduino loop()
bool isConnected();
void disconnect();
void reconnect();
void stopReconnect();
```

### Sending Data

```cpp
void sendStatusWithSocket(bool save = false);           // push status to server
void sendLog(const String &message);                    // send a raw log string
void sendDebugLog(uint8_t level, const String &message);// send a levelled debug log
void sendNotification(const String &message);
void sendNotification(const String &message, const JsonDoc &data);
```

### Time

```cpp
bool hasTime();
bool getTime(int &hh, int &mm, int &ss);
bool getDate(int &yy, int &mm, int &dd);
```

### Web Server

```cpp
void initWebserver(int port = 80);     // start the built-in async web server
AsyncWebServer* getServer();           // add custom routes to the same server
```

### Setters (alternative to config struct)

```cpp
void setAppAndVersion(const char *app, float version);
void setDeviceType(const char *type);
void setSocketHost(const char *host, int port, bool isSSL);
void setToken(const char *token);
void setSendStatusFunction(SendStatusFunction func);
void setReceivedCommandFunction(ReceivedCommandFunction func);
void setEntityChangedFunction(EntityChangedFunction func);
void setConnectedFunction(ConnectedFunction func);
```

## Logging

Include `Log/Log.h` and use the `SC_LOG*` macros anywhere in your firmware. Each call prints to Serial and — when the server has enabled debug logging — also forwards the message to the server's debug log viewer.

```cpp
#include <Log/Log.h>

SC_LOGE(APP_TAG, "fatal: %s", reason);        // level 0 — error
SC_LOGW(APP_TAG, "retrying (%d)", attempt);   // level 1 — warning
SC_LOGI(APP_TAG, "started, ip: %s", ip);      // level 2 — info
SC_LOGD(APP_TAG, "value: %d", val);           // level 3 — debug
SC_LOGV(APP_TAG, "raw bytes: %02x", b);       // level 4 — verbose
SC_LOGV2(".");                                 // raw Serial.printf, no remote
```

**Predefined tags:** `APP_TAG`, `WIFI_TAG`, `WS_TAG`, `MQTT_TAG`, `OTA_TAG`, `NVS_TAG`, `SERVER_TAG`

**Debug level constants:** `DEBUG_LEVEL_ERROR(0)` → `DEBUG_LEVEL_VERBOSE(4)`, `DEBUG_LEVEL_NONE(255)`

The server controls which levels are forwarded at runtime via the `debugLoggingConfig` message. By default all remote forwarding is off until the server enables it.

## WiFi Management

Set `handleWifi: true` in the config to let the library manage WiFi:

1. On first boot (no saved credentials) the device starts an **AP + STA** hotspot.
   - SSID: `{deviceType}-{appName}` (e.g. `ESP32-MyDevice`)
   - Password: last 10 characters of the token
2. Connect to the AP and open `http://192.168.4.1` to enter your WiFi credentials.
3. Credentials are saved to NVS (flash) and survive reboots.
4. If the connection drops the library automatically reconnects with exponential backoff (30 s → 10 min max).

## Built-in Web Server

Call `sc.initWebserver(80)` to enable. The server starts immediately and is accessible as soon as any network interface (AP or STA) is up.

| Route | Method | Description |
|-------|--------|-------------|
| `/sc/wifi` | GET | WiFi credential form with network scan |
| `/sc/wifi/connect` | POST | Save credentials and connect (`ssid`, `password`) |
| `/sc/wifi/disconnect` | GET | Disconnect from current network |
| `/sc/wifi/scan` | GET | JSON list of visible networks with RSSI |
| `/sc/info` | GET | JSON: `{app, version, type, heap, ssid, rssi}` |
| `/sc/status` | GET | JSON: current device status (your `sendStatus` data) |
| `/sc/reboot` | GET/POST | Reboot confirmation page / trigger |
| `/sc/upload` | GET/POST | OTA firmware upload page / accept binary |
| `/sc/style.css` | GET | Embedded stylesheet |

Unknown URLs redirect to `/sc/wifi`.

Use `sc.getServer()` to add your own routes to the same `AsyncWebServer` instance.

## OTA Updates

OTA can be triggered two ways:

1. **Server-pushed:** The server sends `{message: "update", url: "https://..."}` over the WebSocket and the library streams the binary directly from the URL.
2. **Browser upload:** Navigate to `/sc/upload` on the web server and upload a `.bin` file.

## WebSocket Protocol

### Device → Server

| Message | Trigger | Payload |
|---------|---------|---------|
| `connect` | On WebSocket connect | `{deviceId, deviceApp, deviceType, version, localIP, token}` |
| `returningStatus` | `sendStatusWithSocket()` or server `askStatus` | `{data: {...}, save: bool}` |
| `@log` | `sendLog()` | `{text: "..."}` |
| `@debugLog` | `sendDebugLog()` | `{level, text, timestamp}` |
| `notification` | `sendNotification()` | `{body: "..."}` or `{body, options: {...}}` |

### Server → Device

| Message | Callback | Notes |
|---------|----------|-------|
| `connected` | `connected(doc)` | Includes timezone, default data, debug config |
| `command` | `receivedCommand(doc)` | Application command with `data` field |
| `askStatus` | *(internal)* | Triggers `sendStatusWithSocket()` automatically |
| `entityChanged` | `entityChanged(doc)` | Entity name + new value |
| `update` | *(internal)* | Triggers OTA from `url` field |
| `debugLoggingConfig` | *(internal)* | Updates `{enabled, level}` at runtime |

> **Note:** All callbacks share a single internal `JsonDocument`. Copy any values you need before returning from the callback.

## Home Assistant MQTT (Optional)

`HAMqtt` is a standalone class in `src/Mqtt/` that publishes Home Assistant autodiscovery configs and state events over MQTT.

```cpp
#include <Mqtt/HAMqtt.h>

static const HAMqttConfig_t mqttCfg = {
    .server      = "192.168.1.100",
    .port        = 1883,
    .username    = "",
    .password    = "",
    .deviceName  = "my-device",
    .displayName = "My Device",
};

HAMqtt mqtt(&mqttCfg);

// In setup:
mqtt.addEntity({ .id = "button1", .name = "Button 1" });

// In loop:
mqtt.loop();

// On event:
mqtt.publishEvent("button1", "pressed");
```

State is published to `homeassistant/device/{deviceName}/{entityId}/state` as `{action, timestamp}`. Discovery configs are sent to `homeassistant/sensor/{deviceId}_{entityId}/config` on first connect.

## Platform Support

| Platform | Status |
|----------|--------|
| ESP32 | Supported |
| LibreTuya | Supported |
| ESP8266 | Supported |
