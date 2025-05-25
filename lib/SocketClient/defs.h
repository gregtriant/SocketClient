#ifndef DEFS_H
#define DEFS_H

#if defined(ESP32) || defined(LIBRETUYA)
#include <WiFi.h>
#include <ArduinoJson.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#error Platform not supported
#endif

// defines for the Preferences library
#define RW_MODE false
#define RO_MODE true

#define LED_TIME_1 500  // AP mode
#define LED_TIME_2 1000 // Wifi connecting mode
#define MAX_ULONG 4294967295UL
#define MAX_TIME_IN_AP_MODE 60*1000 // 1min

#define USE_SERIAL Serial

#if defined(ESP32) || defined(LIBRETUYA)
  #define CONST_MODE_AP WIFI_MODE_AP
  #define CONST_MODE_STA WIFI_MODE_STA
  #define CONST_MODE_AP_STA WIFI_MODE_AP_STA
#elif defined(ESP8266)
  #define CONST_MODE_AP WIFI_AP
  #define CONST_MODE_STA WIFI_STA
  #define CONST_MODE_AP_STA WIFI_AP_STA
#else
  #error Platform not supported
#endif


/**
 * @brief An alias for the ArduinoJson DynamicJsonDocument type with 256 bytes capacity.
 */
class JsonDoc : public ArduinoJson::DynamicJsonDocument {
public:
  JsonDoc() : ArduinoJson::DynamicJsonDocument(256) {}
};


/**
 * @brief Function type definitions for the SocketClient library.
 * These functions are used to handle various events such as sending status,
 * receiving commands, entity changes, and connection events.
 */
typedef std::function<void(JsonDoc &doc)> SendStatusFunction;
typedef std::function<void(JsonDoc &doc)> ReceivedCommandFunction;
typedef std::function<void(JsonDoc &doc)> EntityChangedFunction;
typedef std::function<void(JsonDoc &doc)> ConnectedFunction;


#define ASSIGN_IF_NOT_NULLPTR(target, value) \
  do { if ((value) != nullptr) { (target) = (value); } } while (0)

#define RETURN_IF_NULLPTR(value) \
  do { if ((value) == nullptr) { return; } } while (0)

#endif // DEFS_H
