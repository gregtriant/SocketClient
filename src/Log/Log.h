#ifndef LOG_H
#define LOG_H

#define MY_LOGE(tag, fmt, ...) Serial.printf("E (%4s) " fmt "\n", tag, ##__VA_ARGS__)
#define MY_LOGW(tag, fmt, ...) Serial.printf("W (%4s) " fmt "\n", tag, ##__VA_ARGS__)
#define MY_LOGI(tag, fmt, ...) Serial.printf("I (%4s) " fmt "\n", tag, ##__VA_ARGS__)
#define MY_LOGD(tag, fmt, ...) Serial.printf("D (%4s) " fmt "\n", tag, ##__VA_ARGS__)
#define MY_LOGV(tag, fmt, ...) Serial.printf("V (%4s) " fmt "\n", tag, ##__VA_ARGS__)
#define MY_LOGV2(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)

#define WIFI_TAG   "WIFI"
#define SERVER_TAG "WEBS"
#define WS_TAG     " WS "
#define OTA_TAG    " OTA"
#define NVS_TAG    " NVS"
#define APP_TAG    " APP"

#endif // LOG_H
