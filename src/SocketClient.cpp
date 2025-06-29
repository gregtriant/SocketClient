#include "SocketClient.h"
#include "SocketClientDefs.h"
#ifndef UPDATE_SIZE_UNKNOWN
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#endif

unsigned long SocketClient::last_dog = 0;
unsigned long SocketClient::last_png = 0;
unsigned long SocketClient::last_reconnect = 0;
unsigned long SocketClient::reconnect_time = 30000; //- 30 sec
bool SocketClient::watchdog(void *vv)
{
  SocketClient *sc = (SocketClient *)vv;
  if (!sc)
    return true;
  WebSocketsClient &wsc = sc->webSocket;
  if (!wsc.isConnected() && (last_reconnect == 0 || (millis() - last_reconnect) > reconnect_time))
  {
    unsigned int x = millis() / (60000);
    USE_SERIAL.print(x);
    USE_SERIAL.printf("* reconnect *\n");
    last_reconnect = millis();
    reconnect_time += 60000;
    if (reconnect_time > max_reconnect_time)
      reconnect_time = max_reconnect_time;
    sc->reconnect();
    return true;
  }

  if (wsc.isConnected())
  {
    if (wsc.sendPing())
    {
      USE_SERIAL.printf("*");
      // last_dog = millis();
    }
    else
    {
      // USE_SERIAL.printf("* watchdog ping:disconnect *\n");
      // wsc.disconnect();
      USE_SERIAL.printf("@");
      //- let the watchdog take care...
    }
  }

  if (last_dog > 0 && millis() - last_dog > watchdog_time)
  {
    USE_SERIAL.printf("* watchdog time *\n");
    wsc.disconnect();
    return true;
  }
  if (last_png > 0 && millis() - last_png > watchdog_time)
  {
    USE_SERIAL.printf("* png watchdog time *\n");
    wsc.disconnect();
    return true;
  }
  return true;
}

// Initialize default functions for the user
void SocketClient_sendStatus(JsonDoc &status)
{
  status.clear();
  status["message"] = "hello";
}

void SocketClient_receivedCommand(JsonDoc &doc)
{
  String stringData = "";
  serializeJson(doc, stringData);
  USE_SERIAL.println(stringData);
}

void SocketClient_entityChanged(JsonDoc &doc)
{
  String stringData = "";
  serializeJson(doc, stringData);
  USE_SERIAL.println(stringData);
}

void SocketClient_connected(JsonDoc &doc)
{
  String stringData = "";
  serializeJson(doc, stringData);
  USE_SERIAL.println(stringData);
}

SocketClient *globalSC = nullptr;

SocketClient::SocketClient()
{
  static int count = 0;
  count++;
  if (count > 1)
  {
    Serial.println("Too many SocketClients created");
    exit(-1);
  }
  _isSSL = true;
  globalSC = this;

  sendStatus = SocketClient_sendStatus;
  receivedCommand = SocketClient_receivedCommand;
  entityChanged = SocketClient_entityChanged;
  connected = SocketClient_connected;
}

SocketClient::~SocketClient()
{
  globalSC = nullptr;
  if (_wifiManager) {
    delete _wifiManager;
    _wifiManager = nullptr;
  }
  if (_nvsManager) {
    delete _nvsManager;
    _nvsManager = nullptr;
  }
  if (_webserverManager) {
    delete _webserverManager;
    _webserverManager = nullptr;
  }
}

void SocketClient::sendLog(const String &message)
{
  if (!webSocket.isConnected())
    return;

  JsonDoc docToSend;
  docToSend["message"] = "@log";
  docToSend["text"] = message;

  String textToSend = "";
  serializeJson(docToSend, textToSend);
  Serial.println("\nSending Log:");
  Serial.println(textToSend);
  webSocket.sendTXT(textToSend);
}

void SocketClient::sendNotification(const String &message)
{
  if (!webSocket.isConnected())
    return;

  JsonDoc docToSend;
  docToSend["message"] = "notification";
  docToSend["body"] = message;

  String textToSend = "";
  serializeJson(docToSend, textToSend);
  Serial.println("\nSending notification:");
  Serial.println(textToSend);
  webSocket.sendTXT(textToSend);
}

void SocketClient::sendNotification(const String &message, const JsonDoc &doc)
{
  if (!webSocket.isConnected())
    return;

  JsonDoc docToSend;
  docToSend["message"] = "notification";
  docToSend["body"] = message;
  docToSend["options"] = doc;

  String textToSend = "";
  serializeJson(docToSend, textToSend);
  Serial.println("\nSending notification:");
  Serial.println(textToSend);
  webSocket.sendTXT(textToSend);
}

void SocketClient::gotMessageSocket(uint8_t *payload)
{
  JsonDoc doc;
  USE_SERIAL.printf("[WSc] got data: %s\n", payload);
  deserializeJson(doc, payload);
  if (strcmp(doc["message"], "connected") == 0)
  {
    if (!doc["data"].isNull())
    {
      JsonDoc doc2;
      deserializeJson(doc2, doc["data"]);
      connected(doc2);
    }
  }
  else if (strcmp(doc["message"], "command") == 0)
  {
    receivedCommand(doc);
  }
  else if (strcmp(doc["message"], "askStatus") == 0)
  {
    sendStatusWithSocket();
  }
  else if (strcmp(doc["message"], "entityChanged") == 0)
  {
    entityChanged(doc);
  }
  else if (strcmp(doc["message"], "update") == 0)
  {
    String updateURL = doc["url"];
    Serial.println(updateURL);
    updatingMode(updateURL);
  }
}

void SocketClient::sendStatusWithSocket(bool save /*=false*/)
{
  if (!webSocket.isConnected())
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


void SocketClient_webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  //- WebSocketsClient &wsc = globalSC->webSocket;
  switch (type)
  {
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
    JsonDoc doc;
    USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);
    // send message to server when Connected
    doc["message"] = "connect";
    doc["deviceId"] = globalSC->_wifiManager->getMacAddress();
    doc["deviceApp"] = globalSC->_deviceApp;
    doc["deviceType"] = globalSC->_deviceType;
    doc["version"] = globalSC->_version;
    doc["localIP"] = globalSC->_wifiManager->getIP();
    doc["token"] = globalSC->_token;
    String JsonToSend = "";
    serializeJson(doc, JsonToSend);
    globalSC->last_reconnect = 0;
    globalSC->reconnect_time = 30000;
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
    globalSC->last_png = millis(); //- got ping from server
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
void SocketClient::update_started()
{
  Serial.println("CALLBACK:  HTTP update process started");
}

void SocketClient::update_finished()
{
  Serial.println("CALLBACK:  HTTP update process finished");
}

void SocketClient::update_progress(int cur, int total)
{
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", cur, total);
}

void SocketClient::update_error(int err)
{
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
}

// for ESP32
#if defined(ESP32) || defined(LIBRETUYA)
void SocketClient::checkUpdate(String host)
{
  HTTPClient client;
  // Connect to external web server
  // WiFiClient wificlient;
  client.begin(host); // wificlient,
  // Get file, just to check if each reachable
  int resp = client.GET();
  Serial.print("Response: ");
  Serial.println(resp);
  // If file is reachable, start downloading
  if (resp == 200)
  {
    // get length of document (is -1 when Server sends no Content-Length header)
    totalLength = client.getSize();
    // transfer to local variable
    int len = totalLength;
    // this is required to start firmware update process
    Update.begin(UPDATE_SIZE_UNKNOWN);
    Serial.printf("FW Size: %u\n", totalLength);
    // create buffer for read
    uint8_t buff[128] = {0};
    // get tcp stream
    WiFiClient *stream = client.getStreamPtr();
    // read all data from server
    Serial.println("Updating firmware...");
    while (client.connected() && (len > 0 || len == -1))
    {
      // get available data size
      size_t size = stream->available();
      if (size)
      {
        // read up to 128 byte
        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
        // pass to function
        SocketClient::updateFirmware(buff, c);
        if (len > 0)
        {
          len -= c;
        }
      }
      delay(1);
    }
  }
  else
  {
    Serial.println("Cannot download firmware file. Only HTTP response 200: OK is supported. Double check firmware location #defined in HOST.");
  }
  client.end();
}
#endif

// for ESP32
// Function to update firmware incrementally
// Buffer is declared to be 128 so chunks of 128 bytes
// from firmware is written to device until server closes
void SocketClient::updateFirmware(uint8_t *data, size_t len)
{
  Update.write(data, len);
  currentLength += len;
  // Print dots while waiting for update to finish
  Serial.print('.');
  // if current length of written firmware is not equal to total firmware size, repeat
  if (currentLength != totalLength)
    return;
  Update.end(true);
  Serial.printf("\nUpdate Success, Total Size: %u\nRebooting...\n", currentLength);
  // Restart ESP32 to see changes
  ESP.restart();
}

void SocketClient::updatingMode(String updateURL)
{

#if defined(ESP32) || defined(LIBRETUYA)
  SocketClient::checkUpdate(updateURL);

#elif defined(ESP8266)
  // wait for WiFi connection
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClient client;
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    ESPhttpUpdate.onStart(SocketClient::update_started);
    ESPhttpUpdate.onEnd(SocketClient::update_finished);
    // ESPhttpUpdate.onProgress(SocketClient::update_progress);
    ESPhttpUpdate.onError(SocketClient::update_error);

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, updateURL); // t_httpUpdate_return ret = ESPhttpUpdate.update(client, "server", 80, "file.bin");
    switch (ret)
    {
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


void SocketClient::reconnect()
{
  if (!WiFi.isConnected()) {
    USE_SERIAL.println("SC <No WiFi>");
    return;
  }

  if (webSocket.isConnected()) {
    webSocket.disconnect();
  }


  USE_SERIAL.println("SC <reconnect>");

  // Clean up any lingering resources
  webSocket.~WebSocketsClient();       // Explicitly call the destructor
  new (&webSocket) WebSocketsClient(); // Reconstruct the object in place
  // Reinitialize the WebSocket connection
  webSocket.onEvent(SocketClient_webSocketEvent); // Reattach the event handler
  webSocket.setReconnectInterval(5000);           // Set reconnect interval
  webSocket.enableHeartbeat(5000, 12000, 2);      // Enable heartbeat

  // Attempt to reconnect
  if (_isSSL) {
    webSocket.beginSSL(_socketHostURL, _port, "/"); // Use SSL connection
  } else {
    webSocket.begin(_socketHostURL, _port, "/"); // Use non-SSL connection
  }
}


void SocketClient::_init()
{
  _nvsManager = new NVSManager();

  String ap_ssid = String(_deviceType) + "-" + String(_deviceApp);
  String ap_password = String(_token).substring(String(_token).length() - 10);
  _wifiManager = new WifiManager(_nvsManager, ap_ssid, ap_password, [this]() { this->reconnect(); });
  if (_handleWifi) {
    _wifiManager->init();
  }

  _webserverManager = new WebserverManager(_wifiManager);

  webSocket.onEvent(SocketClient_webSocketEvent); // initialte our event handler
  webSocket.setReconnectInterval(5000); // try ever 5000 again if connection has failed
  webSocket.enableHeartbeat(5000, 12000, 2);
  reconnect();
}


void SocketClient::init(const SocketClientConfig *config)
{
  RETURN_IF_NULLPTR(config);
  ASSIGN_IF_NOT_NULLPTR(_deviceApp, config->name);
  ASSIGN_IF_NOT_NULLPTR(_deviceType, config->type);
  ASSIGN_IF_NOT_NULLPTR(_socketHostURL, config->host);
  ASSIGN_IF_NOT_NULLPTR(_token, config->token);
  
  ASSIGN_IF_NOT_NULLPTR(sendStatus, config->sendStatus);
  ASSIGN_IF_NOT_NULLPTR(receivedCommand, config->receivedCommand);
  ASSIGN_IF_NOT_NULLPTR(entityChanged, config->entityChanged);
  ASSIGN_IF_NOT_NULLPTR(connected, config->connected);

  
  _version = config->version;
  _port = config->port;
  _isSSL = config->isSSL;
  _handleWifi = config->handleWifi;

  _init();
}


void SocketClient::init(const char *socketHostURL, int port, bool _isSSL)
{
  setSocketHost(socketHostURL, port, _isSSL);
  _init();
}


// void SocketClient::initWifi(const char *ssid, const char *password)
// {
//   _handleWifi = true;
//   _wifi_connecting = true;
//   _connection_retries = 0;
//   _wifi_ssid = String(ssid);
//   _wifi_password = String(password);
//   webSocket.setReconnectInterval(MAX_ULONG); // stop ws reconnect
 
//   Serial.println("Connecting to WiFi: " + _wifi_ssid);
//   if(WiFi.getMode() == CONST_MODE_AP) {
//     WiFi.softAPdisconnect(true); // stop AP mode
//   }
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(ssid, password);

//   _led_blink_time = millis();
//   setLedState(true); // turn on led
// }


// void SocketClient::_wifiConnected()
// {
//   Serial.print("Connected to ");
//   Serial.print(_wifi_ssid);
//   Serial.print("!  IP address:  ");
//   Serial.println(WiFi.localIP());
//   _localIP = WiFi.localIP().toString();
//   // handle the websocket connection
//   webSocket.setReconnectInterval(5000); // try ever 5000 again if connection has failed
//   webSocket.enableHeartbeat(5000, 12000, 2);
//   this->reconnect();
// }


// void SocketClient::_initAPMode()
// {
//   // stop STA mode
//   WiFi.disconnect(true);
//   // WiFi.mode(WIFI_AP); // set to AP mode
//   // if in AP_STA mode make sure in same channel
//   IPAddress apIP(192, 168, 4, 1);
//   IPAddress subnet(255, 255, 255, 0);
//   WiFi.softAPConfig(apIP, apIP, subnet);

//   String ap_name = String(deviceType) + "-" + String(deviceApp);
//   String ap_password = String(token).substring(String(token).length() - 10);
//   Serial.println("AP name: " + ap_name);
//   Serial.println("AP password: " + ap_password);
//   WiFi.softAP(ap_name, ap_password); // AP name and password

//   Serial.print("Starting AP mode... ");
//   Serial.println(WiFi.softAPIP());

//   // stop ws reconnect
//   webSocket.setReconnectInterval(MAX_ULONG);
//   _ap_time = millis();
// }


// void SocketClient::initWifi()
// {
//   _handleWifi = true;
//   _getWifiCredentialsFromNVS();
//   // check if we have credentials in NVS
//   if (_wifi_ssid.isEmpty() || _wifi_password.isEmpty()) {
//     Serial.println("No WiFi credentials found in NVS. Please set them.");
//     _initAPMode();
//     return;
//   } else {
//     Serial.println("\nWiFi credentials found in NVS.");
//     Serial.println("SSID: " + _wifi_ssid);
//     Serial.println("Password: " + _wifi_password);
//     this->initWifi(_wifi_ssid.c_str(), _wifi_password.c_str());
//   }
// }


void SocketClient::loop()
{
  if (_handleWifi) {
    _wifiManager->loop();
  } else if (!_handleWifi) {
  }
  _webserverManager->loop();
  webSocket.loop();
  return;

  // Wifi Access Point mode
  uint64_t now = millis();
  wl_status_t wifiStatus = WiFi.status();
  int wifiMode = (int)WiFi.getMode();
  if (WiFi.getMode() == CONST_MODE_AP) // case AP mode
  {
    // _server.handleClient();
    // toggle_led_time(now, LED_TIME_1, false, false);
    // if (now - _ap_time > MAX_TIME_IN_AP_MODE) // if in AP mode for more than 60 sec, turn off led
    // {
    //   this->initWifi(_wifi_ssid.c_str(), _wifi_password.c_str());
    // }
    return; // if in ap moode, do not handle ws
  }

  // Wifi STA mode
  if (wifiStatus != WL_CONNECTED && wifiMode == CONST_MODE_STA) // Not connected to wifi
  {
    // if (_wifi_connecting)
    // {
    //   toggle_led_time(now, LED_TIME_2, true, true);
    //   if (_connection_retries > 20)
    //   {
    //     Serial.println("Failed to connect to WiFi. Returning to AP mode.");
    //     _initAPMode();
    //   }
    //   return;
    // } else {
    //   this->initWifi(_wifi_ssid.c_str(), _wifi_password.c_str());
    // }
  }
  else if (wifiStatus == WL_CONNECTED && wifiMode == CONST_MODE_STA /*&& _wifi_connecting*/) // Just conneted to wifi
  {
    // _wifi_connecting = false;
    // _wifiConnected();
    // setLedState(false);
  }

  // _server.handleClient();
  // this->webSocket.loop();
}
