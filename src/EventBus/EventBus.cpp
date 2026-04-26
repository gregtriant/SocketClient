#include "EventBus.h"

#if defined(ESP32) || defined(LIBRETUYA)

#include "../Log/Log.h"

#define EB_TAG "EVTB"

QueueHandle_t              EventBus::_queue      = nullptr;
SemaphoreHandle_t          EventBus::_mutex      = nullptr;
EventBus::Subscriber       EventBus::_subscribers[EB_MAX_SUBSCRIBERS] = {};
uint8_t                    EventBus::_nextHandle = 0;
TaskHandle_t               EventBus::_taskHandle = nullptr;

void EventBus::init() {
    if (_queue != nullptr) return; // idempotent

    _queue = xQueueCreate(EB_QUEUE_SIZE, sizeof(EventMessage));
    _mutex = xSemaphoreCreateMutex();

    for (auto& s : _subscribers) s.active = false;

    xTaskCreate(eventBusTask, "eventBusTask", EB_TASK_STACK_SIZE, nullptr, EB_TASK_PRIORITY, &_taskHandle);
    MY_LOGI(EB_TAG, "init (queue=%d, subs=%d)", EB_QUEUE_SIZE, EB_MAX_SUBSCRIBERS);
}

void EventBus::reset() {
    if (_taskHandle) {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }
    if (_queue) {
        vQueueDelete(_queue);
        _queue = nullptr;
    }
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
    for (auto& s : _subscribers) s.active = false;
    _nextHandle = 0;
    init();
}

void EventBus::publish(EventCategory cat, EventSub sub, const EventData& data) {
    if (!_queue) return;
    EventMessage msg = { cat, sub, data };
    if (xQueueSend(_queue, &msg, 0) != pdTRUE) {
        MY_LOGW(EB_TAG, "queue full, event dropped");
    }
}

SubscribeHandle EventBus::subscribe(EventCategory cat, EventSub sub, EventCallback cb) {
    if (!_mutex) return EB_INVALID_HANDLE;

    xSemaphoreTake(_mutex, portMAX_DELAY);
    SubscribeHandle handle = EB_INVALID_HANDLE;
    for (auto& s : _subscribers) {
        if (!s.active) {
            s.category = cat;
            s.sub      = sub;
            s.cb       = cb;
            s.handle   = _nextHandle++;
            s.active   = true;
            handle     = s.handle;
            break;
        }
    }
    xSemaphoreGive(_mutex);

    if (handle == EB_INVALID_HANDLE) {
        MY_LOGW(EB_TAG, "subscriber pool full");
    }
    return handle;
}

void EventBus::unsubscribe(SubscribeHandle handle) {
    if (!_mutex || handle == EB_INVALID_HANDLE) return;

    xSemaphoreTake(_mutex, portMAX_DELAY);
    for (auto& s : _subscribers) {
        if (s.active && s.handle == handle) {
            s.active = false;
            s.cb     = nullptr;
            break;
        }
    }
    xSemaphoreGive(_mutex);
}

void EventBus::eventBusTask(void*) {
    EventMessage msg;

    while (true) {
        if (xQueueReceive(_queue, &msg, portMAX_DELAY) != pdTRUE) continue;

        // Snapshot subscriber list under mutex, then dispatch without holding it
        // (std::function is not trivially copyable — use assignment, not memcpy)
        Subscriber snapshot[EB_MAX_SUBSCRIBERS];
        xSemaphoreTake(_mutex, portMAX_DELAY);
        for (int i = 0; i < EB_MAX_SUBSCRIBERS; i++) snapshot[i] = _subscribers[i];
        xSemaphoreGive(_mutex);

        for (auto& s : snapshot) {
            if (!s.active) continue;
            if (s.category != msg.category) continue;
            if (s.sub != EventSub::ALL && s.sub != msg.sub) continue;
            s.cb(msg.data);
        }
    }
}

#endif
