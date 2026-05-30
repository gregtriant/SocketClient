#ifndef LOG_H
#define LOG_H

#include <stdint.h>

#ifndef DEBUG_LEVEL_ERROR
#define DEBUG_LEVEL_ERROR   0
#define DEBUG_LEVEL_WARNING 1
#define DEBUG_LEVEL_INFO    2
#define DEBUG_LEVEL_DEBUG   3
#define DEBUG_LEVEL_VERBOSE 4
#define DEBUG_LEVEL_NONE    255
#endif

// Implemented strongly in SocketClient.cpp; Log.cpp provides a weak no-op fallback.
void _scRemoteLog(uint8_t level, const char *tag, const char *fmt, ...);

#define SC_LOGE(tag, fmt, ...) do { Serial.printf("E (%4s) " fmt "\n", tag, ##__VA_ARGS__); _scRemoteLog(DEBUG_LEVEL_ERROR,   tag, fmt, ##__VA_ARGS__); } while(0)
#define SC_LOGW(tag, fmt, ...) do { Serial.printf("W (%4s) " fmt "\n", tag, ##__VA_ARGS__); _scRemoteLog(DEBUG_LEVEL_WARNING, tag, fmt, ##__VA_ARGS__); } while(0)
#define SC_LOGI(tag, fmt, ...) do { Serial.printf("I (%4s) " fmt "\n", tag, ##__VA_ARGS__); _scRemoteLog(DEBUG_LEVEL_INFO,    tag, fmt, ##__VA_ARGS__); } while(0)
#define SC_LOGD(tag, fmt, ...) do { Serial.printf("D (%4s) " fmt "\n", tag, ##__VA_ARGS__); _scRemoteLog(DEBUG_LEVEL_DEBUG,   tag, fmt, ##__VA_ARGS__); } while(0)
#define SC_LOGV(tag, fmt, ...) do { Serial.printf("V (%4s) " fmt "\n", tag, ##__VA_ARGS__); _scRemoteLog(DEBUG_LEVEL_VERBOSE, tag, fmt, ##__VA_ARGS__); } while(0)
#define SC_LOGV2(fmt, ...)     Serial.printf(fmt, ##__VA_ARGS__)

#define WIFI_TAG   "WIFI"
#define SERVER_TAG "WEBS"
#define WS_TAG     " WS "
#define OTA_TAG    " OTA"
#define NVS_TAG    " NVS"
#define APP_TAG    " APP"
#define MQTT_TAG   "MQTT"

#endif // LOG_H
