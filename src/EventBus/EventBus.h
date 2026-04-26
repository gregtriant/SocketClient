#pragma once

#if defined(ESP32) || defined(LIBRETUYA)

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <functional>
#include <stdint.h>

#ifndef EB_QUEUE_SIZE
#define EB_QUEUE_SIZE      32
#endif
#ifndef EB_MAX_SUBSCRIBERS
#define EB_MAX_SUBSCRIBERS 32
#endif
#ifndef EB_TASK_STACK_SIZE
#define EB_TASK_STACK_SIZE 4096
#endif
#ifndef EB_TASK_PRIORITY
#define EB_TASK_PRIORITY   5
#endif

enum class EventCategory : uint8_t {
    WIFI,
    WS,
    OTA,
    SERVER,
    _COUNT
};

// EventSub::ALL is a wildcard — valid for subscribe() only, not publish()
enum class EventSub : uint8_t {
    ALL,
    CONNECTED,
    LOST,
    DISCONNECTED,
    AP_STARTED,
    STARTED,
    FINISHED,
};

struct EventData {
    union {
        struct { char ssid[32]; char ip[16]; } wifi;
        struct { char url[128]; }             ota;
        struct { int32_t value; }             generic;
    };
};

struct EventMessage {
    EventCategory category;
    EventSub      sub;
    EventData     data;
};

using SubscribeHandle = uint8_t;
using EventCallback   = std::function<void(const EventData&)>;

static const SubscribeHandle EB_INVALID_HANDLE = 0xFF;

class EventBus {
public:
    static void            init();
    static void            reset(); // clears all subscribers and recreates queue (for tests)
    static void            publish(EventCategory cat, EventSub sub, const EventData& data = {});
    static SubscribeHandle subscribe(EventCategory cat, EventSub sub, EventCallback cb);
    static void            unsubscribe(SubscribeHandle handle);

private:
    struct Subscriber {
        EventCallback    cb;
        SubscribeHandle  handle;
        EventCategory    category;
        EventSub         sub;
        bool             active;
    };

    static void eventBusTask(void* param);

    static QueueHandle_t     _queue;
    static SemaphoreHandle_t _mutex;
    static Subscriber        _subscribers[EB_MAX_SUBSCRIBERS];
    static uint8_t           _nextHandle;
    static TaskHandle_t      _taskHandle;
};

#elif defined(ESP8266)
#error "EventBus requires ESP32 or LibreTuya (FreeRTOS is not available on ESP8266)"
#endif
