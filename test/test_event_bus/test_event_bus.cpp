#include <Arduino.h>
#include <unity.h>
#include "EventBus/EventBus.h"

static SemaphoreHandle_t sem;

void setUp() {
    sem = xSemaphoreCreateBinary();
    EventBus::reset();
}

void tearDown() {
    vSemaphoreDelete(sem);
}

// 1. Subscribe to a specific sub, publish it — callback fires
void test_specific_subscribe_fires() {
    EventBus::subscribe(EventCategory::WIFI, EventSub::CONNECTED, [](const EventData&) {
        xSemaphoreGive(sem);
    });
    EventBus::publish(EventCategory::WIFI, EventSub::CONNECTED);
    TEST_ASSERT_TRUE_MESSAGE(
        xSemaphoreTake(sem, pdMS_TO_TICKS(200)) == pdTRUE,
        "callback did not fire"
    );
}

// 2. Subscribe ALL, publish two different subs in that category — both fire
void test_wildcard_subscribe_fires_for_all_subs() {
    static SemaphoreHandle_t count_sem;
    count_sem = xSemaphoreCreateCounting(10, 0);

    EventBus::subscribe(EventCategory::WIFI, EventSub::ALL, [](const EventData&) {
        xSemaphoreGive(count_sem);
    });

    EventBus::publish(EventCategory::WIFI, EventSub::CONNECTED);
    EventBus::publish(EventCategory::WIFI, EventSub::LOST);

    TEST_ASSERT_TRUE(xSemaphoreTake(count_sem, pdMS_TO_TICKS(200)) == pdTRUE);
    TEST_ASSERT_TRUE(xSemaphoreTake(count_sem, pdMS_TO_TICKS(200)) == pdTRUE);
    vSemaphoreDelete(count_sem);
}

// 3. Subscribe to sub X, publish sub Y (same category) — callback does NOT fire
void test_different_sub_does_not_fire() {
    EventBus::subscribe(EventCategory::WIFI, EventSub::CONNECTED, [](const EventData&) {
        xSemaphoreGive(sem);
    });
    EventBus::publish(EventCategory::WIFI, EventSub::LOST);
    TEST_ASSERT_FALSE_MESSAGE(
        xSemaphoreTake(sem, pdMS_TO_TICKS(100)) == pdTRUE,
        "callback fired for wrong sub"
    );
}

// 4. Unsubscribe, then publish — callback does NOT fire
void test_unsubscribe_prevents_callback() {
    auto handle = EventBus::subscribe(EventCategory::WS, EventSub::CONNECTED, [](const EventData&) {
        xSemaphoreGive(sem);
    });
    EventBus::unsubscribe(handle);
    EventBus::publish(EventCategory::WS, EventSub::CONNECTED);
    TEST_ASSERT_FALSE_MESSAGE(
        xSemaphoreTake(sem, pdMS_TO_TICKS(100)) == pdTRUE,
        "callback fired after unsubscribe"
    );
}

// 5. Two subscribers to the same event — both callbacks fire
void test_multiple_subscribers_both_fire() {
    static SemaphoreHandle_t count_sem;
    count_sem = xSemaphoreCreateCounting(10, 0);

    EventBus::subscribe(EventCategory::OTA, EventSub::STARTED, [](const EventData&) {
        xSemaphoreGive(count_sem);
    });
    EventBus::subscribe(EventCategory::OTA, EventSub::STARTED, [](const EventData&) {
        xSemaphoreGive(count_sem);
    });

    EventBus::publish(EventCategory::OTA, EventSub::STARTED);

    TEST_ASSERT_TRUE(xSemaphoreTake(count_sem, pdMS_TO_TICKS(200)) == pdTRUE);
    TEST_ASSERT_TRUE(xSemaphoreTake(count_sem, pdMS_TO_TICKS(200)) == pdTRUE);
    vSemaphoreDelete(count_sem);
}

// 6. Subscribe to category A, publish category B — callback does NOT fire
void test_wrong_category_does_not_fire() {
    EventBus::subscribe(EventCategory::WIFI, EventSub::CONNECTED, [](const EventData&) {
        xSemaphoreGive(sem);
    });
    EventBus::publish(EventCategory::WS, EventSub::CONNECTED);
    TEST_ASSERT_FALSE_MESSAGE(
        xSemaphoreTake(sem, pdMS_TO_TICKS(100)) == pdTRUE,
        "callback fired for wrong category"
    );
}

// 7. Publish from a separate FreeRTOS task — callback still fires (thread-safety)
static void publisherTask(void*) {
    EventBus::publish(EventCategory::SERVER, EventSub::CONNECTED);
    vTaskDelete(nullptr);
}

void test_publish_from_another_task() {
    EventBus::subscribe(EventCategory::SERVER, EventSub::CONNECTED, [](const EventData&) {
        xSemaphoreGive(sem);
    });
    xTaskCreate(publisherTask, "pub", 2048, nullptr, 4, nullptr);
    TEST_ASSERT_TRUE_MESSAGE(
        xSemaphoreTake(sem, pdMS_TO_TICKS(300)) == pdTRUE,
        "callback did not fire from another task"
    );
}

// 8. EventData payload is delivered correctly
void test_payload_delivered() {
    static char receivedIp[16] = {};

    EventBus::subscribe(EventCategory::WIFI, EventSub::CONNECTED, [](const EventData& d) {
        strncpy(receivedIp, d.wifi.ip, sizeof(receivedIp));
        xSemaphoreGive(sem);
    });

    EventData d = {};
    strncpy(d.wifi.ssid, "MyNet", sizeof(d.wifi.ssid));
    strncpy(d.wifi.ip, "192.168.1.99", sizeof(d.wifi.ip));
    EventBus::publish(EventCategory::WIFI, EventSub::CONNECTED, d);

    TEST_ASSERT_TRUE(xSemaphoreTake(sem, pdMS_TO_TICKS(200)) == pdTRUE);
    TEST_ASSERT_EQUAL_STRING("192.168.1.99", receivedIp);
}

void setup() {
    Serial.begin(115200);
    vTaskDelay(pdMS_TO_TICKS(2000)); // wait for serial monitor

    UNITY_BEGIN();
    RUN_TEST(test_specific_subscribe_fires);
    RUN_TEST(test_wildcard_subscribe_fires_for_all_subs);
    RUN_TEST(test_different_sub_does_not_fire);
    RUN_TEST(test_unsubscribe_prevents_callback);
    RUN_TEST(test_multiple_subscribers_both_fire);
    RUN_TEST(test_wrong_category_does_not_fire);
    RUN_TEST(test_publish_from_another_task);
    RUN_TEST(test_payload_delivered);
    UNITY_END();
}

void loop() {}
