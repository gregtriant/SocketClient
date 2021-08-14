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
    const char* url = doc["url"];
    updateURL = url;
    // Serial.println(updateURL);
    updatingMode();
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
      doc["updateURL"] = globalSC->updateURL;
      doc["version"] = globalSC->version;
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
    hexdump(payload, length);
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


// --------------------------------------------------------  OTA functions  -----------------------------------------
void update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
}

void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

void SocketClient::updatingMode() {
  // wait for WiFi connection
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    WiFiClient client;
    // callbacks
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    ESPhttpUpdate.onStart(update_started);
    ESPhttpUpdate.onEnd(update_finished);
    ESPhttpUpdate.onProgress(update_progress);
    ESPhttpUpdate.onError(update_error);

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
}

