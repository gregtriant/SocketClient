// src/System/Mutex.h
#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// A FreeRTOS mutex and the scope guard that takes it. Declare one next to the state it guards,
// create it from the owner's init()/begin(), and lock with Mutex::Lock:
//
//     Mutex _mutex;                                                    // member
//     void Thing::init()     { _mutex.create(); }
//     void Thing::set(int v) { Mutex::Lock lock(_mutex); _value = v; }
//
// The guard gives it back on every way out of the scope, so a locked block can't leak the
// mutex by later growing an early return. Before create() (or if it failed) locking is a no-op
// rather than a crash. The wait is portMAX_DELAY: hold it around a few assignments, never
// around a send, a log line or a delay.
//
// create() is plain -- the same task taking it twice deadlocks, so a locked method must not
// call another locked method of the same object; factor the shared body into an unlocked
// helper. createRecursive() allows that nesting, but costs a little more per lock and turns
// lock-order mistakes into silent success instead of a findable deadlock, so prefer plain.
// Which was created decides whether lock()/unlock() use the Recursive FreeRTOS calls: mixing
// them is undefined, which is why the handle is wrapped here rather than passed around bare.
class Mutex {
public:
    Mutex() = default;

    // Once -- a second call is ignored, not a leak. False only if FreeRTOS had no memory.
    // Call it from init()/begin(), before the tasks that use the object are running.
    bool create()          { return _create(false); }
    bool createRecursive() { return _create(true); }

    bool valid() const       { return _handle != nullptr; }
    bool isRecursive() const { return _recursive; }
    SemaphoreHandle_t handle() const { return _handle; }  // for FreeRTOS calls not wrapped here

    // Prefer Mutex::Lock. For a take and give that genuinely can't share a scope -- then every
    // pair must match on every path.
    void lock() {
        if (!_handle) return;
        if (_recursive) xSemaphoreTakeRecursive(_handle, portMAX_DELAY);
        else            xSemaphoreTake(_handle, portMAX_DELAY);
    }
    void unlock() {
        if (!_handle) return;
        if (_recursive) xSemaphoreGiveRecursive(_handle);
        else            xSemaphoreGive(_handle);
    }

    // Holds `mutex` for the enclosing scope.
    class Lock {
    public:
        explicit Lock(Mutex &mutex) : _mutex(mutex) { _mutex.lock(); }
        ~Lock() { _mutex.unlock(); }

        Lock(const Lock &) = delete;   // tied to its scope: a copy would give it back twice
        Lock &operator=(const Lock &) = delete;

    private:
        Mutex &_mutex;
    };

    // The handle is owned, not shared. No destructor either: these live as long as the object
    // they guard, and deleting one a task might hold is worse than keeping it to the end.
    Mutex(const Mutex &) = delete;
    Mutex &operator=(const Mutex &) = delete;

private:
    bool _create(bool recursive) {
        if (_handle) return true;
        _handle = recursive ? xSemaphoreCreateRecursiveMutex() : xSemaphoreCreateMutex();
        _recursive = recursive;
        return _handle != nullptr;
    }

    SemaphoreHandle_t _handle = nullptr;
    bool _recursive = false;
};
