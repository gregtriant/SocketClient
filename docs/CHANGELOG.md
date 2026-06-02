# Changelog for Socket Client library

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