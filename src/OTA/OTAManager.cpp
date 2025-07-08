#include "OTAManager.h"

OTAManager::OTAManager() {}

OTAManager::~OTAManager() {}

void OTAManager::startOTA(String updateURL)
{
#if defined(ESP32) || defined(LIBRETUYA)
  _checkUpdate(updateURL);
#elif defined(ESP8266)
  updatingMode(updateURL);
#endif
}


#if defined(ESP32) || defined(LIBRETUYA)
void OTAManager::_checkUpdate(String host)
{
  HTTPClient client;
  // Connect to external web server
  // WiFiClient wificlient;
  client.begin(host); // wificlient,
  // Get file, just to check if each reachable
  int resp = client.GET();
  MY_LOGD(OTA_TAG, "Response: %d", resp);
  // If file is reachable, start downloading
  if (resp == 200) {
    // get length of document (is -1 when Server sends no Content-Length header)
    _totalLength = client.getSize();
    // transfer to local variable
    int len = _totalLength;
    // this is required to start firmware update process
    Update.begin(UPDATE_SIZE_UNKNOWN);
    MY_LOGD(OTA_TAG, "FW Size: %u\n", _totalLength);
    // create buffer for read
    uint8_t buff[128] = {0};
    // get tcp stream
    WiFiClient *stream = client.getStreamPtr();
    // read all data from server
    MY_LOGD(OTA_TAG, "Updating firmware...");
    while (client.connected() && (len > 0 || len == -1)) {
      // get available data size
      size_t size = stream->available();
      if (size) {
        // read up to 128 byte
        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
        // pass to function
        OTAManager::_updateFirmware(buff, c);
        if (len > 0) {
          len -= c;
        }
      }
      delay(1);
    }
  } else {
    MY_LOGD(OTA_TAG, "Cannot download firmware file. Only HTTP response 200: OK is supported. Double check firmware location #defined in HOST.");
  }
  client.end();
}

// for ESP32
// Function to update firmware incrementally
// Buffer is declared to be 128 so chunks of 128 bytes
// from firmware is written to device until server closes
void OTAManager::_updateFirmware(uint8_t *data, size_t len)
{
  Update.write(data, len);
  _currentLength += len;
  // Print dots while waiting for update to finish
  MY_LOGV2(".");
  // if current length of written firmware is not equal to total firmware size, repeat
  if (_currentLength != _totalLength)
    return;
  Update.end(true);
  MY_LOGD(OTA_TAG, "Update Success, Total Size: %u\nRebooting...", _currentLength);
  // Restart ESP32 to see changes
  ESP.restart();
}
#endif

#ifdef ESP8266
// for ESP8266
void OTAManager::update_started()
{
  MY_LOGD(OTA_TAG, "CALLBACK:  HTTP update process started");
}

void OTAManager::update_finished()
{
  MY_LOGD(OTA_TAG, "CALLBACK:  HTTP update process finished");
}

void OTAManager::update_progress(int cur, int total)
{
  MY_LOGD(OTA_TAG, "CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void OTAManager::update_error(int err)
{
  MY_LOGD(OTA_TAG, "CALLBACK:  HTTP update fatal error code %d\n", err);
}

void OTAManager::updatingMode(String updateURL)
{
  // wait for WiFi connection
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClient client;
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    ESPhttpUpdate.onStart(OTAManager::update_started);
    ESPhttpUpdate.onEnd(OTAManager::update_finished);
    // ESPhttpUpdate.onProgress(OTAManager::update_progress);
    ESPhttpUpdate.onError(OTAManager::update_error);

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, updateURL); // t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 80, "file.bin");
    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
      MY_LOGD(OTA_TAG, "HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      MY_LOGD(OTA_TAG, "HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      MY_LOGD(OTA_TAG, "HTTP_UPDATE_OK");
      break;
    }
  }
}
#endif
