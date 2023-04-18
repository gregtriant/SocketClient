#include "SocketClient.h"

// function for the user
String SocketClient_defineDataToSend() {
  return "hello";
}
void SocketClient_recievedData(String data) {
  Serial.println(data);
}

SocketClient *globalSC = NULL;

SocketClient::SocketClient() {
  static int count = 0;
  count++;
  if (count > 1) {
    Serial.println("Too many SocketClients created");
    exit(-1);
  }
  globalSC = this;

  defineDataToSend = SocketClient_defineDataToSend;
  recievedData = SocketClient_recievedData;
}

void SocketClient::gotMessageSocket(uint8_t * payload) {
  DynamicJsonDocument doc(300);
  USE_SERIAL.printf("[WSc] got data: %s\n", payload);
  deserializeJson(doc, payload);
  const char* serverMessage = doc["message"];
  // update
  if (strcmp(serverMessage, "update") == 0) {
    String updateURL = doc["url"];
    Serial.println(updateURL);
    updatingMode(updateURL);
  }

  if (strcmp(serverMessage, "sendData") == 0) {
    sendDataWithSocket(doc);
  }
  if (strcmp(serverMessage, "getData") == 0) {
    getDataFromSocket(doc);
  }
}

void SocketClient::sendDataWithSocket(DynamicJsonDocument recievedDoc) {
  DynamicJsonDocument responseDoc(1024);
  const char *recieverId = recievedDoc["recieverId"];

  responseDoc["message"] = "returningData";
  responseDoc["recieverId"] = recieverId;
  String data = defineDataToSend(); // call user function to fill the JsonObject
  responseDoc["data"] = data;

  String JsonToSend = "";
  serializeJson(responseDoc, JsonToSend);
  Serial.println("");
  Serial.println(JsonToSend);
  webSocket.sendTXT(JsonToSend);
}

void SocketClient::getDataFromSocket(DynamicJsonDocument recievedDoc) {
  String data = recievedDoc["data"];
  recievedData(data);
  //
  // TODO - maybe send ok response to server
  // 
}

void SocketClient_webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
  case WStype_ERROR:
    USE_SERIAL.printf("[WSc] Error!! : %s\n", payload);
    break;
  case WStype_DISCONNECTED:
    USE_SERIAL.printf("[WSc] Disconnected!\n");
    break;
  case WStype_CONNECTED: 
    {
      DynamicJsonDocument doc(1024);
      USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);
      // send message to server when Connected
      doc["message"] = "connect";
      doc["deviceId"] = globalSC->macAddress;
      doc["deviceApp"] = globalSC->deviceApp;
      doc["deviceType"] = globalSC->deviceType;
      doc["version"] = globalSC->version;
      doc["localIP"] = globalSC->localIP;
      String JsonToSend = "";
      serializeJson(doc, JsonToSend);
      globalSC->webSocket.sendTXT(JsonToSend);
    }
    break;
  case WStype_TEXT:
    {
      globalSC->gotMessageSocket(payload);
    }
    break;
  case WStype_BIN:
    USE_SERIAL.printf("[WSc] get binary length: %u\n", length);
    // hexdump(payload, length);
    // send data to server
    // webSocket.sendBIN(payload, length);
    break;
  case WStype_FRAGMENT_TEXT_START:
    break;
  case WStype_FRAGMENT_BIN_START:
    break;
  case WStype_FRAGMENT:
    break;
  case WStype_FRAGMENT_FIN:
    break;
  case WStype_PING:
    break;
  case WStype_PONG:
    break;
  }
}


// --------------------------------------------------------  OTA functions  ----------------------------------------- //
void SocketClient::update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
}

void SocketClient::update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
}

void SocketClient::update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void SocketClient::update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

// for ESP32
void SocketClient::checkUpdate(String host) {
  HTTPClient client;
  // Connect to external web server
  client.begin(host);
  // Get file, just to check if each reachable
  int resp = client.GET();
  Serial.print("Response: ");
  Serial.println(resp);
  // If file is reachable, start downloading
  if(resp == 200){
    // get length of document (is -1 when Server sends no Content-Length header)
    totalLength = client.getSize();
    // transfer to local variable
    int len = totalLength;
    // this is required to start firmware update process
    Update.begin(UPDATE_SIZE_UNKNOWN);
    Serial.printf("FW Size: %u\n",totalLength);
    // create buffer for read
    uint8_t buff[128] = { 0 };
    // get tcp stream
    WiFiClient * stream = client.getStreamPtr();
    // read all data from server
    Serial.println("Updating firmware...");
    while(client.connected() && (len > 0 || len == -1)) {
      // get available data size
      size_t size = stream->available();
      if(size) {
        // read up to 128 byte
        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
        // pass to function
        SocketClient::updateFirmware(buff, c);
        if(len > 0) {
          len -= c;
        }
      }
      delay(1);
    }
  } else {
    Serial.println("Cannot download firmware file. Only HTTP response 200: OK is supported. Double check firmware location #defined in HOST.");
  }
  client.end();
}

// for ESP32
// Function to update firmware incrementally
// Buffer is declared to be 128 so chunks of 128 bytes
// from firmware is written to device until server closes
void SocketClient::updateFirmware(uint8_t *data, size_t len){
  Update.write(data, len);
  currentLength += len;
  // Print dots while waiting for update to finish
  Serial.print('.');
  // if current length of written firmware is not equal to total firmware size, repeat
  if(currentLength != totalLength) return;
  Update.end(true);
  Serial.printf("\nUpdate Success, Total Size: %u\nRebooting...\n", currentLength);
  // Restart ESP32 to see changes 
  ESP.restart();
}

void SocketClient::updatingMode(String updateURL) {
  
  #if defined(ESP32) || defined(LIBRETUYA)
  SocketClient::checkUpdate(updateURL);

  #elif defined(ESP8266)
  // wait for WiFi connection
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    ESPhttpUpdate.onStart(SocketClient::update_started);
    ESPhttpUpdate.onEnd(SocketClient::update_finished);
    // ESPhttpUpdate.onProgress(SocketClient::update_progress);
    ESPhttpUpdate.onError(SocketClient::update_error);

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, updateURL); // t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 80, "file.bin");
    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    }
  }
  #endif
}

