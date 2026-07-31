#pragma once

#if !defined(ESP32) && !defined(LIBRETUYA)
#error EventHandler requires ESP32 or LibreTuya (FreeRTOS task/queue/semaphore support); it is not available on ESP8266.
#endif

#include <Arduino.h>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "../Log/Log.h"

/**
 * @brief Single-task publish/subscribe event bus for inter-task communication.
 *
 * @tparam CategoryT The event category type each app injects, typically a
 * small `enum class` (e.g. `enum class EventCategory : uint8_t { Nfc, Button };`).
 * Event ids within a category are a plain `uint16_t`; apps typically define
 * their own scoped id enum per category and cast to `uint16_t` at call sites.
 *
 * Subscribers register a callback for a `CategoryT`, either for every event
 * in that category or for one specific event id. Publishers hand an event
 * (with an optional heap-allocated payload) to a queue; a dedicated FreeRTOS
 * task owned by this class dequeues events and invokes matching callbacks
 * one at a time, so all callbacks run serialized on that single task
 * regardless of which task published the event.
 *
 * Subscriptions are stored in a mutex-protected linked list. Re-subscribing
 * with the same (category, eventId) - or the same category for an
 * all-events subscription - updates the existing entry's callback in place
 * rather than adding a duplicate. There is no unsubscribe(); subscriptions
 * live for the lifetime of the program.
 *
 * @note ESP32 / LibreTuya only - relies on FreeRTOS task/queue/semaphore
 * APIs not exposed the same way on ESP8266's Arduino core.
 *
 * ### Example
 * @code
 * enum class EventCategory : uint8_t { Nfc, Button };
 * enum class NfcEventId : uint16_t { CardDetected };
 *
 * EventHandler<EventCategory> eventHandler;
 *
 * void setup() {
 *     eventHandler.init();
 *
 *     eventHandler.subscribe(EventCategory::Nfc, (uint16_t)NfcEventId::CardDetected,
 *         [](EventCategory category, uint16_t eventId, const void *data, size_t dataLen) {
 *             uint32_t cardId = *static_cast<const uint32_t *>(data);
 *             Serial.println(cardId);
 *         });
 *
 *     uint32_t *cardId = new uint32_t(0x1A2B3C4D);
 *     eventHandler.publish(EventCategory::Nfc, (uint16_t)NfcEventId::CardDetected, cardId, sizeof(uint32_t));
 * }
 * @endcode
 */
template <typename CategoryT>
class EventHandler {
public:
    /**
     * @brief Signature for a subscription callback.
     *
     * Invoked on the EventHandler's own task, never on the publisher's task.
     *
     * @param category Category the event was published under.
     * @param eventId  Event id within that category.
     * @param data     Pointer to the published payload. Still owned by
     *                 EventHandler - do not retain this pointer past the
     *                 call and do not free it; EventHandler frees it right
     *                 after every matching callback has returned.
     * @param dataLen  Number of valid bytes at @p data (0 if none was published).
     *
     * @warning Do not call subscribe() from within a callback - it would
     *          self-deadlock on the subscription mutex. Calling publish()
     *          from within a callback is safe.
     */
    using EventCallback = std::function<void(CategoryT category, uint16_t eventId, const void *data, size_t dataLen)>;

    /// Creates the subscription mutex, event queue, and dispatch task. Call once from setup().
    void init() {
        subscriptionsMutex = xSemaphoreCreateMutex();
        eventQueue = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(EventMessage));

        xTaskCreatePinnedToCore(taskEntry, "EventHandler", TASK_STACK_SIZE, this, TASK_PRIORITY, &taskHandle, TASK_CORE);
    }

    /**
     * @brief Subscribe to every event published under @p category.
     * @param category Category to receive all events for.
     * @param callback Callback invoked for each matching event.
     */
    void subscribe(CategoryT category, EventCallback callback) {
        subscribeLocked(category, true, 0, callback);
    }

    /**
     * @brief Subscribe to one specific @p eventId within @p category.
     * @param category Category the event is published under.
     * @param eventId  Specific event id to match.
     * @param callback Callback invoked when that event is published.
     */
    void subscribe(CategoryT category, uint16_t eventId, EventCallback callback) {
        subscribeLocked(category, false, eventId, callback);
    }

    /**
     * @brief Publish an event, transferring ownership of its payload to EventHandler.
     *
     * @p data must be heap-allocated with `new uint8_t[dataLen]` (or be
     * `nullptr`). Once this call is made, EventHandler owns that memory no
     * matter what: on success it is freed with `delete[]` right after every
     * matching subscriber callback has returned; on failure (queue full) it
     * is freed immediately, before publish() returns. The caller must never
     * touch or free @p data after calling this.
     *
     * @param category Category to publish under.
     * @param eventId  Event id within that category.
     * @param data     Heap-allocated payload (`new uint8_t[dataLen]`) that
     *                 EventHandler takes ownership of, or `nullptr` for no payload.
     * @param dataLen  Number of bytes at @p data (0 if @p data is `nullptr`).
     * @return `true` if the event was queued; `false` if the queue was full
     *         (in which case @p data has already been freed).
     */
    bool publish(CategoryT category, uint16_t eventId, void *data = nullptr, size_t dataLen = 0) {
        EventMessage msg;
        msg.category = category;
        msg.eventId = eventId;
        msg.data = data;
        msg.dataLen = data != nullptr ? dataLen : 0;

        if (xQueueSend(eventQueue, &msg, 0) == pdTRUE) {
            return true;
        }

        SC_LOGW(EVENT_TAG, "publish() queue full, dropping event id=%u", eventId);
        delete[] static_cast<uint8_t *>(data);
        return false;
    }

private:
    static constexpr uint8_t EVENT_QUEUE_LENGTH = 16;
    static constexpr uint32_t TASK_STACK_SIZE = 4096;
    static constexpr UBaseType_t TASK_PRIORITY = 1;
    static constexpr BaseType_t TASK_CORE = 1;

    /// Internal queue item: an event category/id plus its owned payload pointer.
    struct EventMessage {
        CategoryT category;
        uint16_t eventId;
        void *data;
        size_t dataLen;
    };

    /// One linked-list node: a single subscriber's registration.
    struct Subscription {
        CategoryT category;
        bool subscribeToAll;
        uint16_t eventId;
        EventCallback callback;
        Subscription *next;
    };

    /// FreeRTOS task trampoline; casts @p param back to `this` and calls taskLoop().
    static void taskEntry(void *param) {
        static_cast<EventHandler *>(param)->taskLoop();
    }

    /// Body of the dispatch task: blocks on the queue and dispatches each event as it arrives.
    void taskLoop() {
        EventMessage msg;
        for (;;) {
            if (xQueueReceive(eventQueue, &msg, portMAX_DELAY) == pdTRUE) {
                dispatch(msg);
            }
        }
    }

    /// Looks up matching subscriptions for @p msg, invokes their callbacks, then frees msg.data.
    void dispatch(const EventMessage &msg) {
        xSemaphoreTake(subscriptionsMutex, portMAX_DELAY);

        for (Subscription *sub = subscriptions; sub != nullptr; sub = sub->next) {
            if (sub->category == msg.category && (sub->subscribeToAll || sub->eventId == msg.eventId)) {
                sub->callback(msg.category, msg.eventId, msg.data, msg.dataLen);
            }
        }

        xSemaphoreGive(subscriptionsMutex);

        delete[] static_cast<uint8_t *>(msg.data);
    }

    /// Shared implementation for both subscribe() overloads.
    void subscribeLocked(CategoryT category, bool subscribeToAll, uint16_t eventId, EventCallback callback) {
        xSemaphoreTake(subscriptionsMutex, portMAX_DELAY);

        Subscription *sub = findSubscriptionLocked(category, subscribeToAll, eventId);
        if (sub == nullptr) {
            sub = new Subscription();
            sub->category = category;
            sub->subscribeToAll = subscribeToAll;
            sub->eventId = eventId;
            sub->next = subscriptions;
            subscriptions = sub;
        }
        sub->callback = callback;

        xSemaphoreGive(subscriptionsMutex);
    }

    /// Finds an existing subscription matching the given key. Caller must hold subscriptionsMutex.
    Subscription *findSubscriptionLocked(CategoryT category, bool subscribeToAll, uint16_t eventId) {
        for (Subscription *sub = subscriptions; sub != nullptr; sub = sub->next) {
            if (sub->category == category && sub->subscribeToAll == subscribeToAll &&
                (subscribeToAll || sub->eventId == eventId)) {
                return sub;
            }
        }
        return nullptr;
    }

    Subscription *subscriptions = nullptr;
    SemaphoreHandle_t subscriptionsMutex = nullptr;
    QueueHandle_t eventQueue = nullptr;
    TaskHandle_t taskHandle = nullptr;
};
