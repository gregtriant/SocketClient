# Changelog for Socket Client library

## [1.5.0] - 2026-07-31

### Added

- `EventHandler<CategoryT>` (`src/EventHandler/EventHandler.h`) — a standalone, header-only publish/subscribe event bus for inter-task communication
  - Templated on an app-supplied category type (e.g. `enum class EventCategory : uint8_t { Nfc, Button };`), so each consuming app injects its own categories/event ids without touching the library
  - Subscribe to every event in a category, or to one specific event id within a category; re-subscribing the same key updates the callback in place instead of duplicating
  - Dedicated FreeRTOS task dispatches all callbacks serially, regardless of which task published the event
  - `publish()` transfers ownership of a heap-allocated (`new uint8_t[]`) payload to `EventHandler`, which frees it right after every matching callback returns
  - ESP32 / LibreTuya only (relies on FreeRTOS task/queue/semaphore APIs not available on ESP8266, same constraint as `HAMqtt`)
  - Not yet wired into `SocketClient`'s own callbacks (`connected`, `receivedCommand`, etc.) — those are unchanged; this is a standalone module for now

## [1.3.0] - 2026-06-03

### Added

- Bidirectional file transfer over HTTPS
  - `fileReady` WebSocket message triggers an HTTPS GET; downloaded bytes are passed to the `fileReceived` callback as `const std::vector<uint8_t> &buf`
  - `requestFile` WebSocket message triggers an HTTPS POST; the `fileRequested` callback fills a `std::vector<uint8_t> &buf` with bytes to upload as multipart/form-data
  - Both callbacks are optional fields in `SocketClientConfig_t` (default `nullptr`)
  - Auth via `x-mac-address` header; file size capped at 4096 bytes
- `FileReceivedFunction` and `FileRequestedFunction` typedefs in `SocketClientDefs.h`

### Changed

- `SocketClientConfig_t` callback fields for file transfer use `std::vector<uint8_t>` instead of raw pointers, eliminating manual malloc/free at the application layer

## [1.2.1] - 2025-06-01

### Added to platformio