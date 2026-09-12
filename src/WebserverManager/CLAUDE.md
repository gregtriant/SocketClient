# CLAUDE.md — WebserverManager

## Routes

| Path | Method | Handler | Notes |
|------|--------|---------|-------|
| `/sc/` | GET | `_sendPage` | Main nav hub |
| `/sc/reboot` | GET | `_sendRebootPage` | Reboot confirm page |
| `/sc/reboot` | POST | inline lambda | Calls `ESP.restart()` |
| `/sc/wifi` | GET | `_sendWifiPage` | WiFi config page |
| `/sc/wifi/connect` | POST | `_handleWifiConnect` | Saves credentials + calls `_wifiManager->init()` |
| `/sc/wifi/disconnect` | GET | inline lambda | Calls `WiFi.disconnect(true)` |
| `/sc/upload` | GET | `_sendUploadPage` | OTA firmware upload page |
| `/sc/upload` | POST | inline lambdas | Streams firmware via `Update` API; reboots on success |
| `/sc/info` | GET | inline lambda | Returns JSON: product, version, device, heap, ssid, rssi |
| `/sc/status` | GET | inline lambda | Returns JSON from `_getCurrentStatus` callback |
| `/sc/wifi/scan` | GET | inline lambda | Async WiFi scan — see below |
| `/sc/style.css` | GET | inline lambda | Shared CSS for all pages; served with `Cache-Control: max-age=3600` |

## IMPORTANT: Route Registration Order

ESPAsyncWebServer does **prefix matching** — a handler for `/sc/wifi` will intercept requests to `/sc/wifi/scan`, `/sc/wifi/connect`, and `/sc/wifi/disconnect` before they reach their own handlers. The same applies to `/sc` vs all `/sc/...` routes.

**Rule:** always register more-specific (longer) routes before shorter prefix routes. In `_setupWebServer()`:
1. All `/sc/wifi/connect`, `/sc/wifi/disconnect`, `/sc/wifi/scan` — registered first
2. `/sc/wifi` — registered after its sub-routes
3. All other `/sc/...` routes
4. `/sc/` and `/sc` — registered last, just before `onNotFound`

Breaking this order silently causes all sub-routes to serve the parent page instead.

## HTML Page Architecture

All CSS lives in the single `COMMON_CSS` PROGMEM string, served at `/sc/style.css`. Each page HTML is a minimal `const char XXX_HTML[] PROGMEM` raw string that references it with `<link rel="stylesheet" href="/sc/style.css">`. The only dynamic substitution in page HTML is `%APP_TITLE%` (replaced with `product + " " + version` from `_deviceInfo`).

Pages:
- `PAGE_HTML_PART1` — main nav hub, served by `_sendPage()`
- `REBOOT_HTML` — reboot confirm + spinner, served by `_sendRebootPage()`
- `UPLOAD_HTML` — OTA file upload + progress %, served by `_sendUploadPage()`
- `WIFI_HTML` — WiFi config form + inline scan results, served by `_sendWifiPage()`

To add a new page: add a PROGMEM string with the `<link>` tag, add `_sendXxxPage()` declaration to `WebserverManager.h`, implement it following the same `FPSTR` + `html.replace("%APP_TITLE%", title)` pattern, register the route in `_setupWebServer()`. Add any new CSS classes to `COMMON_CSS`.

## Async WiFi Scan (`/sc/wifi/scan`)

`WiFi.scanNetworks()` is blocking and crashes the device inside an AsyncWebServer handler (watchdog). The scan is always async:
- `WiFi.scanComplete()` returns `WIFI_SCAN_FAILED (-2)` → start scan with `WiFi.scanNetworks(true)`, respond `202 []`
- Returns `WIFI_SCAN_RUNNING (-1)` → respond `202 []`
- Returns `n >= 0` → respond `200` with JSON array, then call `WiFi.scanDelete()`

The WiFi page polls `/sc/wifi/scan` every 1500 ms until it receives a `200`.

## Spinner + Reset Pattern

Reboot and upload pages share the same UX pattern:
1. Button click → disable + hide button, show inline `.spinner` div, fire the action (POST)
2. `checkOnline()` polls `HEAD /sc/` every 3 s
3. On success → hide spinner, re-show and re-enable button (no redirect)

## WiFi Page Connect

`doConnect()` sends a `fetch` POST to `/sc/wifi/connect` with `Content-Type: application/x-www-form-urlencoded` and URL-encoded `ssid=...&password=...` body. `_handleWifiConnect` reads params with `request->getParam("ssid", true)` (the `true` flag means POST body param).
