#include "OTAManager.h"
#include "../System/System.h"

OTAManager::OTAManager() {}

OTAManager::~OTAManager() {}

// ESPhttpUpdate takes plain C callbacks with no user pointer, so the ESP8266 progress and
// error hooks below have to be static. _otaManager is a single object that lives as long as
// the sketch does, so a file-static pointer to it, set when an update starts, is enough to
// get back to the instance.
static OTAManager *s_self = nullptr;

void OTAManager::setCallbacks(OtaProgressFn onProgress, OtaErrorFn onError)
{
  _onProgress = onProgress;
  _onError = onError;
}

void OTAManager::_report(uint32_t written, uint32_t total)
{
  if (!_onProgress)
    return;

  const bool finished = (total > 0 && written >= total);
  if (!finished) {
    const uint32_t now = millis();
    const int percent = (total > 0) ? (int)((written * 100ULL) / total) : -1;
    const bool lateEnough = (now - _lastReportMs) >= OTA_PROGRESS_MIN_INTERVAL_MS;
    const bool bigEnough = (percent >= 0) && (percent - _lastReportPercent >= OTA_PROGRESS_MIN_PERCENT);
    if (!lateEnough && !bigEnough)
      return;
    _lastReportMs = now;
    _lastReportPercent = percent;
  }

  _onProgress(written, total);
}

void OTAManager::_fail(const char *code, const char *detail)
{
  SC_LOGD(OTA_TAG, "OTA failed: %s (%s)", code, detail ? detail : "");
  if (_onError)
    _onError(code, detail ? detail : "");
}

void OTAManager::startOTA(String updateURL)
{
  s_self = this;
  // Each attempt reports from scratch: these persist across attempts on this one long-lived
  // object, and a stale percent would swallow the first reports of the next download.
  _lastReportMs = 0;
  _lastReportPercent = -1;
#if defined(ESP32) || defined(LIBRETUYA)
  _checkUpdate(updateURL);
#elif defined(ESP8266)
  updatingMode(updateURL);
#endif
}


#if defined(ESP32) || defined(LIBRETUYA)
void OTAManager::_checkUpdate(String host)
{
  // _otaManager is a single object reused for every "update" command over the device's
  // whole uptime (see SocketClient::_init()/_otaManager->startOTA()) -- _currentLength's
  // in-class initializer only runs once, at construction, so a retry after a prior attempt
  // was interrupted partway through (e.g. connection dropped mid-transfer) would otherwise
  // start this attempt with a stale, non-zero count already left over. That made
  // _updateFirmware()'s completion check trip early, calling Update.end(true) (which
  // finalizes without verifying the size) on a truncated image and rebooting into it.
  _currentLength = 0;

  HTTPClient client;

  // The scheme decides the client, and there is deliberately no fallback from one to the
  // other: the server only hands out an https:// URL to a chip and a library version it
  // knows can fetch it, so TLS failing here means something is actually wrong and should
  // be visible, not quietly downgraded to an unencrypted download.
  const bool secure = host.startsWith("https://");
#if defined(ESP32)
  WiFiClientSecure secureClient;
  WiFiClient plainClient;
  if (secure) {
    // Not a pinned root: a CA rotation would brick OTA on every device already in the
    // field, and the only way to fix that would be the OTA that no longer works. The
    // download token is what authorizes this request; TLS is here for confidentiality.
    secureClient.setInsecure();
    client.begin(secureClient, host);
  } else {
    client.begin(plainClient, host);
  }
#else
  if (secure) {
    _fail("downloadFailed", "https not supported on this chip");
    return;
  }
  client.begin(host);
#endif

  // Get file, just to check if each reachable
  int resp = client.GET();
  SC_LOGD(OTA_TAG, "Response: %d", resp);
  // If file is reachable, start downloading
  if (resp == 200) {
    // get length of document (is -1 when Server sends no Content-Length header)
    _totalLength = client.getSize();

    // Without a size there is nothing to check the image against, and Update.begin would
    // have to be told the partition size instead -- which is exactly how an oversized
    // image gets half-written before anything notices.
    if (_totalLength <= 0) {
      _fail("downloadFailed", "no Content-Length");
      client.end();
      return;
    }

    // transfer to local variable
    int len = _totalLength;
    // The real size, not UPDATE_SIZE_UNKNOWN: this is what makes an image too big for the
    // partition fail here, before the first byte is written, with the running firmware
    // still intact.
    if (!Update.begin(_totalLength)) {
      _fail("tooLarge", "image does not fit the update partition");
      client.end();
      return;
    }
    SC_LOGD(OTA_TAG, "FW Size: %u\n", _totalLength);
    _report(0, _totalLength);
    // create buffer for read
    uint8_t buff[128] = {0};
    // get tcp stream
    WiFiClient *stream = client.getStreamPtr();
    // read all data from server
    SC_LOGD(OTA_TAG, "Updating firmware...");
    uint32_t lastDataMs = millis();
    while (client.connected() && (len > 0 || len == -1)) {
      // get available data size
      size_t size = stream->available();
      if (size) {
        lastDataMs = millis();
        // read up to 128 byte
        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
        // pass to function
        if (!OTAManager::_updateFirmware(buff, c)) {
          Update.abort();
          client.end();
          return;
        }
        if (len > 0) {
          len -= c;
        }
      } else if ((millis() - lastDataMs) >= OTA_STALL_TIMEOUT_MS) {
        // Nothing for OTA_STALL_TIMEOUT_MS. client.connected() can stay true forever on a
        // half-open connection, so this is the only way out of the loop that doesn't need
        // someone to walk over and pull the power.
        Update.abort();
        _fail("downloadAborted", "no data for 30s");
        client.end();
        return;
      }
      delay(1);
    }

    // Falling out of the loop still owed bytes means the connection went away mid-download.
    // _updateFirmware only reboots on a complete image, so reaching here with len > 0 is a
    // failure the user should be told about rather than a silent nothing.
    if (len > 0) {
      Update.abort();
      _fail("downloadAborted", "connection closed before the whole image arrived");
    }
  } else {
    SC_LOGD(OTA_TAG, "Cannot download firmware file. Only HTTP response 200: OK is supported. Double check firmware location #defined in HOST.");
    _fail("downloadFailed", String(resp).c_str());
  }
  client.end();
}

// for ESP32
// Function to update firmware incrementally
// Buffer is declared to be 128 so chunks of 128 bytes
// from firmware is written to device until server closes
bool OTAManager::_updateFirmware(uint8_t *data, size_t len)
{
  // A short write means the flash write failed -- out of space, or a bad sector. Carrying
  // on from here would finalize a corrupt image and reboot into it.
  if (Update.write(data, len) != len) {
    _fail("flashFailed", "short write to flash");
    return false;
  }
  _currentLength += len;
  // Print dots while waiting for update to finish
  SC_LOGV2(".");
  _report(_currentLength, _totalLength);
  // if current length of written firmware is not equal to total firmware size, repeat
  if (_currentLength != _totalLength)
    return true;
  if (!Update.end(true)) {
    _fail("flashFailed", "the written image did not verify");
    return false;
  }
  SC_LOGD(OTA_TAG, "Update Success, Total Size: %u\nRebooting...", _currentLength);
  // Reboot into the new firmware
  System::reboot(System::RebootReason::Ota);
  return true;
}
#endif

#ifdef ESP8266
// for ESP8266
void OTAManager::update_started()
{
  SC_LOGD(OTA_TAG, "CALLBACK:  HTTP update process started");
}

void OTAManager::update_finished()
{
  SC_LOGD(OTA_TAG, "CALLBACK:  HTTP update process finished");
}

void OTAManager::update_progress(int cur, int total)
{
  SC_LOGD(OTA_TAG, "CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
  // Static because ESPhttpUpdate takes a plain function pointer; s_self is set in startOTA.
  if (s_self)
    s_self->_report((uint32_t)cur, (uint32_t)total);
}

void OTAManager::update_error(int err)
{
  SC_LOGD(OTA_TAG, "CALLBACK:  HTTP update fatal error code %d\n", err);
  if (s_self)
    s_self->_fail("downloadAborted", ESPhttpUpdate.getLastErrorString().c_str());
}

void OTAManager::updatingMode(String updateURL)
{
  // This chip keeps the plain-HTTP download on purpose: a BearSSL handshake needs ~16-22 kB
  // of heap, and there is not room for a second TLS context beside the socket's during an
  // OTA. The server knows that, and never hands an ESP8266 an https:// URL.
  if (updateURL.startsWith("https://"))
  {
    _fail("downloadFailed", "https not supported on this chip");
    return;
  }

  // wait for WiFi connection
  if (WiFi.status() != WL_CONNECTED)
  {
    // Returning quietly here left the server to work it out from three minutes of silence,
    // which is the exact thing the progress reporting exists to replace.
    _fail("downloadFailed", "wifi not connected");
    return;
  }

  {
    WiFiClient client;
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    ESPhttpUpdate.onStart(OTAManager::update_started);
    ESPhttpUpdate.onEnd(OTAManager::update_finished);
    ESPhttpUpdate.onProgress(OTAManager::update_progress);
    ESPhttpUpdate.onError(OTAManager::update_error);

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, updateURL); // t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 80, "file.bin");
    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
      SC_LOGD(OTA_TAG, "HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      // onError above has usually reported this already. Saying it twice is harmless --
      // the server just re-stamps the same terminal phase -- and this covers the failures
      // that return without ever reaching the error callback.
      _fail("downloadAborted", ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      SC_LOGD(OTA_TAG, "HTTP_UPDATE_NO_UPDATES");
      _fail("downloadFailed", "server had no update to give");
      break;

    case HTTP_UPDATE_OK:
      SC_LOGD(OTA_TAG, "HTTP_UPDATE_OK");
      break;
    }
  }
}
#endif
