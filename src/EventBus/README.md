# EventBus

Pub/sub event bus for SocketClient. Runs a dedicated FreeRTOS task on ESP32/LibreTuya — safe to publish from any task.

---

## Quick start

```cpp
#include <SocketClient.h>  // EventBus is included automatically

void setup() {
    EventBus::init();

    EventBus::subscribe(EventCategory::WIFI, EventSub::CONNECTED, [](const EventData& d) {
        Serial.printf("WiFi up — IP: %s\n", d.wifi.ip);
    });
}
```

---

## Subscribing

```cpp
SubscribeHandle handle = EventBus::subscribe(category, sub, callback);
```

| Parameter  | Type            | Description                                      |
|------------|-----------------|--------------------------------------------------|
| `category` | `EventCategory` | Coarse topic group (WIFI, WS, OTA, SERVER)       |
| `sub`      | `EventSub`      | Fine topic. Use `EventSub::ALL` to match any sub |
| `callback` | `EventCallback` | `void(const EventData&)`                         |

Returns a `SubscribeHandle` you can pass to `unsubscribe()` later.

### Wildcard — all events in a category

```cpp
EventBus::subscribe(EventCategory::WIFI, EventSub::ALL, [](const EventData& d) {
    // fires for CONNECTED, LOST, AP_STARTED — any WIFI sub
});
```

### Unsubscribe

```cpp
auto handle = EventBus::subscribe(EventCategory::OTA, EventSub::STARTED, cb);
// ...
EventBus::unsubscribe(handle);
```

---

## Publishing

```cpp
EventBus::publish(category, sub);              // no payload
EventBus::publish(category, sub, eventData);   // with payload
```

`publish()` is non-blocking (`xQueueSend` with zero timeout). If the queue is full the event is dropped and a warning is logged.

---

## EventData — callback payload

`EventData` is a tagged union. Which field is valid depends on the category of the event.

```cpp
struct EventData {
    union {
        struct { char ssid[32]; char ip[16]; } wifi;   // EventCategory::WIFI
        struct { char url[128]; }             ota;    // EventCategory::OTA
        struct { int32_t value; }             generic; // general use
    };
};
```

### WIFI events → use `d.wifi`

```cpp
EventBus::subscribe(EventCategory::WIFI, EventSub::CONNECTED, [](const EventData& d) {
    Serial.printf("Connected to %s — IP %s\n", d.wifi.ssid, d.wifi.ip);
});

EventBus::subscribe(EventCategory::WIFI, EventSub::LOST, [](const EventData& d) {
    // d.wifi fields are empty for LOST — no payload
});

EventBus::subscribe(EventCategory::WIFI, EventSub::AP_STARTED, [](const EventData& d) {
    // no payload
});
```

### WS / SERVER events → no payload fields used

```cpp
EventBus::subscribe(EventCategory::WS, EventSub::CONNECTED, [](const EventData& d) {
    Serial.println("WebSocket connected");
});

EventBus::subscribe(EventCategory::WS, EventSub::DISCONNECTED, [](const EventData& d) {
    Serial.println("WebSocket disconnected");
});

EventBus::subscribe(EventCategory::SERVER, EventSub::CONNECTED, [](const EventData& d) {
    Serial.println("Server handshake complete");
});
```

### OTA events → use `d.ota`

```cpp
EventBus::subscribe(EventCategory::OTA, EventSub::STARTED, [](const EventData& d) {
    Serial.printf("OTA starting from %s\n", d.ota.url);
});

EventBus::subscribe(EventCategory::OTA, EventSub::FINISHED, [](const EventData& d) {
    Serial.println("OTA done");
});
```

### Publishing with payload

```cpp
EventData d = {};
strncpy(d.wifi.ssid, WiFi.SSID().c_str(), sizeof(d.wifi.ssid));
strncpy(d.wifi.ip,   WiFi.localIP().toString().c_str(), sizeof(d.wifi.ip));
EventBus::publish(EventCategory::WIFI, EventSub::CONNECTED, d);
```

---

## Event table

| Category          | Sub                    | Payload field  |
|-------------------|------------------------|----------------|
| `WIFI`            | `CONNECTED`            | `d.wifi`       |
| `WIFI`            | `LOST`                 | —              |
| `WIFI`            | `AP_STARTED`           | —              |
| `WS`              | `CONNECTED`            | —              |
| `WS`              | `DISCONNECTED`         | —              |
| `SERVER`          | `CONNECTED`            | —              |
| `OTA`             | `STARTED`              | `d.ota`        |
| `OTA`             | `FINISHED`             | —              |

---

## Configuration (override before including SocketClient.h)

```cpp
#define EB_QUEUE_SIZE      32   // max buffered events before publish() drops
#define EB_MAX_SUBSCRIBERS 32   // total subscriber slots across all events
#define EB_TASK_STACK_SIZE 4096 // eventBusTask stack in bytes
#define EB_TASK_PRIORITY   5    // FreeRTOS task priority
```

---

## Notes

- Callbacks fire in the `eventBusTask` context — keep them short and non-blocking.
- The subscriber list is snapshotted before dispatch, so it is safe to `subscribe()` or `unsubscribe()` from inside a callback.
- `EventBus::init()` is idempotent — safe to call multiple times.
- ESP32 / LibreTuya only. Will not compile on ESP8266.
