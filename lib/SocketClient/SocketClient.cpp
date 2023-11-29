#include "SocketClient.h"

#ifndef UPDATE_SIZE_UNKNOWN
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#endif

unsigned long SocketClient::last_dog = 0;
unsigned long SocketClient::last_png = 0;
unsigned long SocketClient::last_reconnect = 0;
unsigned long SocketClient::reconnect_time = 30000; //- 30 sec
bool SocketClient::watchdog(void *vv){
  SocketClient *sc = (SocketClient*)vv;
  if(!sc)
    return true;
  WebSocketsClient &wsc = sc->webSocket;
  if(!wsc.isConnected() && (last_reconnect==0 || (millis()-last_reconnect)>reconnect_time)){
    unsigned int x = millis() / (60000);
    USE_SERIAL.print(x);
    USE_SERIAL.printf("* reconnect *\n");
    last_reconnect = millis();
    reconnect_time += 60000;
    if(reconnect_time>max_reconnect_time)
      reconnect_time = max_reconnect_time;
    sc->reconnect();
    return true;
  }

  if(wsc.isConnected()){
    if(wsc.sendPing()){
      USE_SERIAL.printf("*");
      // last_dog = millis();    
    }else{
      // USE_SERIAL.printf("* watchdog ping:disconnect *\n");
      // wsc.disconnect();
      USE_SERIAL.printf("@");
      //- let the watchdog take care...
    }
  }

  if(last_dog>0 && millis() - last_dog > watchdog_time){
    USE_SERIAL.printf("* watchdog time *\n");
    wsc.disconnect();
    return true;
  }
  if(last_png>0 && millis() - last_png > watchdog_time){
    USE_SERIAL.printf("* png watchdog time *\n");
    wsc.disconnect();
    return true;
  }
  return true;
}

// Initialize default functions for the user
void SocketClient_sendStatus(JsonDoc& status) {
  status.clear();
  status["message"] = "hello";
}

void SocketClient_receivedCommand(JsonDoc &doc) {
  String stringData = "";
  serializeJson(doc, stringData);
  USE_SERIAL.println(stringData);
}

void SocketClient_entityChanged(JsonDoc &doc) {
  String stringData = "";
  serializeJson(doc, stringData);
  USE_SERIAL.println(stringData);
}

SocketClient *globalSC = NULL;

SocketClient::SocketClient() {
  static int count = 0;
  count++;
  if (count > 1) {
    Serial.println("Too many SocketClients created");
    exit(-1);
  }
  isSSL = true;
  globalSC = this;

  sendStatus = SocketClient_sendStatus;
  receivedCommand = SocketClient_receivedCommand;
  entityChanged = SocketClient_entityChanged;
}

void SocketClient::gotMessageSocket(uint8_t * payload) {
  JsonDoc doc;
  USE_SERIAL.printf("[WSc] got data: %s\n", payload);
  deserializeJson(doc, payload);
 
  if (strcmp(doc["message"], "command") == 0) {
    receivedCommand(doc);
  }
  else if (strcmp(doc["message"], "askStatus") == 0) {
    sendStatusWithSocket();
  }
  else if (strcmp(doc["message"], "entityChanged") == 0) {
    entityChanged(doc);
  }
  else if (strcmp(doc["message"], "update") == 0) {
    String updateURL = doc["url"];
    Serial.println(updateURL);
    updatingMode(updateURL);
  }
}

void SocketClient::sendStatusWithSocket(bool save /*=false*/) {
  if(!webSocket.isConnected())
    return;

  JsonDoc responseDoc;

  responseDoc["message"] = "returningStatus";
  JsonDoc data;
  sendStatus(data);
  responseDoc["data"] = data;
  responseDoc["save"] = save;

  String JsonToSend = "";
  serializeJson(responseDoc, JsonToSend);
  Serial.println("");
  Serial.println(JsonToSend);
  webSocket.sendTXT(JsonToSend);
}

// void SocketClient::getDataFromSocket(DynamicJsonDocument recievedDoc) {
  // String data = recievedDoc["data"];
  // recievedData(data);
  //
  // TODO - maybe send ok response to server
  // 
// }

void SocketClient_webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  //- WebSocketsClient &wsc = globalSC->webSocket;
  switch (type) {
  case WStype_ERROR:
    USE_SERIAL.printf("[WSc] Error!! : %s\n", payload);
    break;
  case WStype_DISCONNECTED:
    USE_SERIAL.printf("[WSc] Disconnected!\n");
    globalSC->last_dog = 0;
    globalSC->last_png = 0;
    break;
  case WStype_CONNECTED: 
    {
      globalSC->last_dog = millis();
      globalSC->last_png = millis();
      StaticJsonDocument<512> doc;
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
      globalSC->last_reconnect=0;
      globalSC->reconnect_time=30000;
      globalSC->webSocket.sendTXT(JsonToSend);
    }
    break;
  case WStype_TEXT:
    {
      globalSC->last_dog = millis();
      globalSC->gotMessageSocket(payload);
    }
    break;
  case WStype_BIN:
    globalSC->last_dog = millis();
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
    USE_SERIAL.printf(".");
    globalSC->last_png = millis();  //- got ping from server
    //- globalSC->last_dog = millis();
    //- care only of your own pongs...
    break;
  case WStype_PONG:
    USE_SERIAL.printf("-");
    globalSC->last_dog = millis();
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
#if defined(ESP32) || defined(LIBRETUYA)
void SocketClient::checkUpdate(String host) {
  HTTPClient client;
  // Connect to external web server
  // WiFiClient wificlient;
  client.begin(host); //wificlient, 
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
#endif

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

void SocketClient::reconnect() { 
    if(webSocket.isConnected())
      webSocket.disconnect();

    if(!WiFi.isConnected()){
      USE_SERIAL.println("No WiFi.");
      return;
    }

    //- USE_SERIAL.println("SC <reconnect>");

    if(isSSL)
      webSocket.beginSSL(socketHostURL, port, "/"); // server address, port and URL
    else
      webSocket.begin(socketHostURL, port, "/"); // server address, port and URL
  }

  void SocketClient::init(){
     String mac = WiFi.macAddress();
    mac.toCharArray(macAddress, 50);
    // init local IP
    localIP = WiFi.localIP().toString();
    webSocket.onEvent(SocketClient_webSocketEvent);   // initialte our event handler
    // webSocket.setAuthorization("user", "Password"); // use HTTP Basic Authorization this is optional remove if not needed
    webSocket.setReconnectInterval(5000); // try ever 5000 again if connection has failed
    webSocket.enableHeartbeat(5000,12000,2);
    reconnect();
    //- notimer this->timer.every(tick_time,watchdog,this);
  }

  void SocketClient::loop() {
    this->webSocket.loop();
    //- notimer this->timer.tick();
  }
