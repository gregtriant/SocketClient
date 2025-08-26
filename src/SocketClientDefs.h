#pragma once
#include <ArduinoJson.h>

#if defined(ESP32) || defined(LIBRETUYA)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#error Platform not supported
#endif

#ifndef JSON_SIZE
#define JSON_SIZE 4096
#endif

#define MAX_ULONG 4294967295UL
#define LED_TIME_1 500                 // AP mode
#define LED_TIME_2 1000                // Wifi connecting mode
#define MAX_TIME_IN_AP_MODE 60 * 1000  // 1min

#define USE_SERIAL Serial

#if defined(ESP32) || defined(LIBRETUYA)
#define CONST_MODE_AP WIFI_MODE_AP
#define CONST_MODE_STA WIFI_MODE_STA
#define CONST_MODE_AP_STA WIFI_MODE_APSTA
#elif defined(ESP8266)
#define CONST_MODE_AP WIFI_AP
#define CONST_MODE_STA WIFI_STA
#define CONST_MODE_AP_STA WIFI_AP_STA
#else
#error Platform not supported
#endif

#if defined(ESP32) || defined(LIBRETUYA)
#define DEVICE_TYPE "ESP32"
#elif defined(ESP8266)
#define DEVICE_TYPE "ESP8266"
#else
#define DEVICE_TYPE "UNKNOWN"
#endif

#define DEFAULT_PORT 80
#define DEFAULT_HOST "sensordata.space"
#define DEFAULT_APP_NAME "SocketClient"


/**
 * @brief JsonVariant does reference counting, no need to use &.
 * JsonVariant is a reference to a JsonDocument.
 */
typedef JsonVariant JsonDoc;


/**
 * @brief Function type definitions for the SocketClient library.
 * These functions are used to handle various events such as sending status,
 * receiving commands, entity changes, and connection events.
 */
typedef std::function<void(JsonDoc doc)> SendStatusFunction;
typedef std::function<void(JsonDoc doc)> ReceivedCommandFunction;
typedef std::function<void(JsonDoc doc)> EntityChangedFunction;
typedef std::function<void(JsonDoc doc)> ConnectedFunction;


/**
 * @brief Configuration structure for the SocketClient.
 * This structure holds various settings for the SocketClient, including device
 * settings, server settings, and function pointers for handling events.
 */
typedef struct {
    /* device settings */
    const char *name;     // name of the app
    float version;        // version of the app
    const char *type;     // type of the device (e.g. ESP32, ESP8266)
    const int ledPin;     // pin for the LED (optional, can be -1 if not used)

    /* server settings */
    const char *host;       // host of the socket server
    const int port;         // port of the socket server
    const bool isSSL;       // is the socket server using SSL
    const char *token;      // token for authentication
    const bool handleWifi;  // the socket client will handle wifi connection

    /* functions */
    SendStatusFunction sendStatus;
    ReceivedCommandFunction receivedCommand;
    EntityChangedFunction entityChanged;
    ConnectedFunction connected;
} SocketClientConfig_t;

typedef struct {
    const char *name;
    const char *type;
    float version;
} DeviceInfo_t;

/**
 * Macros
 */
#define ASSIGN_IF_NOT_NULLPTR(target, value) \
    do {                                     \
        if ((value) != nullptr) {            \
            (target) = (value);              \
        }                                    \
    } while (0)

#define RETURN_IF_NULLPTR(value)  \
    do {                          \
        if ((value) == nullptr) { \
            return;               \
        }                         \
    } while (0)
