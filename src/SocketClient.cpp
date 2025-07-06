#include "SocketClient.h"

#include "Log/Log.h"
#include "SocketClientDefs.h"

#ifndef UPDATE_SIZE_UNKNOWN
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#endif

unsigned long SocketClient::last_dog = 0;
unsigned long SocketClient::last_png = 0;
unsigned long SocketClient::last_reconnect = 0;
unsigned long SocketClient::reconnect_time = 30000;  //- 30 sec

bool SocketClient::watchdog(void *vv) {
    SocketClient *sc = (SocketClient *)vv;
    if (!sc)
        return true;
    WebSocketsClient &wsc = sc->webSocket;
    if (!wsc.isConnected() && (last_reconnect == 0 || (millis() - last_reconnect) > reconnect_time)) {
        unsigned int x = millis() / (60000);
        MY_LOGD(WS_TAG, "%ld", x);
        MY_LOGD(WS_TAG, "* reconnect *\n");
        last_reconnect = millis();
        reconnect_time += 60000;
        if (reconnect_time > max_reconnect_time)
            reconnect_time = max_reconnect_time;
        sc->reconnect();
        return true;
    }

    if (wsc.isConnected()) {
        if (wsc.sendPing()) {
            MY_LOGD(WS_TAG, "*");
            // last_dog = millis();
        } else {
            // wsc.disconnect();
            MY_LOGD(WS_TAG, "@");
        }
    }

    if (last_dog > 0 && millis() - last_dog > watchdog_time) {
        MY_LOGD(WS_TAG, "* watchdog time *\n");
        wsc.disconnect();
        return true;
    }
    if (last_png > 0 && millis() - last_png > watchdog_time) {
        MY_LOGD(WS_TAG, "* png watchdog time *\n");
        wsc.disconnect();
        return true;
    }
    return true;
}

// Initialize default functions for the user
void SocketClient_sendStatus(JsonDoc &status) {
    status.clear();
    status["message"] = "hello";
}

void SocketClient_receivedCommand(JsonDoc &doc) {
    String stringData = "";
    serializeJson(doc, stringData);
    MY_LOGD(WS_TAG, "%s", stringData.c_str());
}

void SocketClient_entityChanged(JsonDoc &doc) {
    String stringData = "";
    serializeJson(doc, stringData);
    MY_LOGD(WS_TAG, "%s", stringData.c_str());
}

void SocketClient_connected(JsonDoc &doc) {
    String stringData = "";
    serializeJson(doc, stringData);
    MY_LOGD(WS_TAG, "%s", stringData.c_str());
}

SocketClient *globalSC = nullptr;

SocketClient::SocketClient() : _nvsManager(nullptr), _wifiManager(nullptr), _webserverManager(nullptr), _otaManager(nullptr) {
    static int count = 0;
    count++;
    if (count > 1) {
        MY_LOGE(WS_TAG, "Too many SocketClients created");
        exit(-1);
    }
    _isSSL = true;
    globalSC = this;

    sendStatus = SocketClient_sendStatus;
    receivedCommand = SocketClient_receivedCommand;
    entityChanged = SocketClient_entityChanged;
    connected = SocketClient_connected;
}

SocketClient::~SocketClient() {
    globalSC = nullptr;
    delete _wifiManager;
    _wifiManager = nullptr;
    delete _nvsManager;
    _nvsManager = nullptr;
    delete _webserverManager;
    _webserverManager = nullptr;
}

void SocketClient::sendLog(const String &message) {
    if (!webSocket.isConnected())
        return;

    JsonDoc docToSend;
    docToSend["message"] = "@log";
    docToSend["text"] = message;

    String textToSend = "";
    serializeJson(docToSend, textToSend);
    MY_LOGD(WS_TAG, "Sending Log: %s", textToSend.c_str());
    webSocket.sendTXT(textToSend);
}

void SocketClient::sendNotification(const String &message) {
    if (!webSocket.isConnected())
        return;

    JsonDoc docToSend;
    docToSend["message"] = "notification";
    docToSend["body"] = message;

    String textToSend = "";
    serializeJson(docToSend, textToSend);
    MY_LOGD(WS_TAG, "Sending notification: %s", textToSend.c_str());
    webSocket.sendTXT(textToSend);
}

void SocketClient::sendNotification(const String &message, const JsonDoc &doc) {
    if (!webSocket.isConnected())
        return;

    JsonDoc docToSend;
    docToSend["message"] = "notification";
    docToSend["body"] = message;
    docToSend["options"] = doc;

    String textToSend = "";
    serializeJson(docToSend, textToSend);
    MY_LOGD(WS_TAG, "Sending notification: %s", textToSend.c_str());
    webSocket.sendTXT(textToSend);
}

void SocketClient::gotMessageSocket(uint8_t *payload) {
    JsonDoc doc;
    MY_LOGD(WS_TAG, "Got data: %s\n", payload);
    deserializeJson(doc, payload);
    if (strcmp(doc["message"], "connected") == 0) {
        if (!doc["data"].isNull()) {
            JsonDoc doc2;
            deserializeJson(doc2, doc["data"]);
            connected(doc2);
        }
    } else if (strcmp(doc["message"], "command") == 0) {
        receivedCommand(doc);
    } else if (strcmp(doc["message"], "askStatus") == 0) {
        sendStatusWithSocket();
    } else if (strcmp(doc["message"], "entityChanged") == 0) {
        entityChanged(doc);
    } else if (strcmp(doc["message"], "update") == 0) {
        String updateURL = doc["url"];
        MY_LOGD(WS_TAG, "Update URL: %s", updateURL.c_str());
        _otaManager->startOTA(updateURL);
    }
}

void SocketClient::sendStatusWithSocket(bool save /*=false*/) {
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
    MY_LOGD(WS_TAG, "Returning status: %s", JsonToSend.c_str());
    webSocket.sendTXT(JsonToSend);
}

void SocketClient_webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
    //- WebSocketsClient &wsc = globalSC->webSocket;
    switch (type) {
        case WStype_ERROR:
            MY_LOGD(WS_TAG, "Error! : %s", payload);
            break;
        case WStype_DISCONNECTED:
            MY_LOGD(WS_TAG, "Disconnected!");
            globalSC->last_dog = 0;
            globalSC->last_png = 0;
            break;
        case WStype_CONNECTED: {
            globalSC->last_dog = millis();
            globalSC->last_png = millis();
            JsonDoc doc;
            MY_LOGD(WS_TAG, "Connected to url: %s", payload);
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
        } break;
        case WStype_TEXT: {
            globalSC->last_dog = millis();
            globalSC->gotMessageSocket(payload);
        } break;
        case WStype_BIN:
            globalSC->last_dog = millis();
            MY_LOGD(WS_TAG, "Get binary length: %u", length);
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
            MY_LOGV2(".");
            globalSC->last_png = millis();  //- got ping from server
            //- globalSC->last_dog = millis();
            //- care only of your own pongs...
            break;
        case WStype_PONG:
            MY_LOGV2("-");
            globalSC->last_dog = millis();
            break;
    }
}

void SocketClient::reconnect() {
    if (!WiFi.isConnected()) {
        MY_LOGD(WS_TAG, "<No WiFi>");
        return;
    }

    if (webSocket.isConnected()) {
        webSocket.disconnect();
    }
    MY_LOGD(WS_TAG, "<reconnect>");
    // Clean up any lingering resources
    webSocket.~WebSocketsClient();        // Explicitly call the destructor
    new (&webSocket) WebSocketsClient();  // Reconstruct the object in place
    // Reinitialize the WebSocket connection
    webSocket.onEvent(SocketClient_webSocketEvent);  // Reattach the event handler
    webSocket.setReconnectInterval(5000);            // Set reconnect interval
    webSocket.enableHeartbeat(5000, 12000, 2);       // Enable heartbeat

    // Attempt to reconnect
    if (_isSSL) {
        webSocket.beginSSL(_socketHostURL, _port, "/");  // Use SSL connection
    } else {
        webSocket.begin(_socketHostURL, _port, "/");  // Use non-SSL connection
    }
}

void SocketClient::stopReconnect() {
    webSocket.setReconnectInterval(MAX_ULONG);
    webSocket.enableHeartbeat(MAX_ULONG, MAX_ULONG, 255);
}

void SocketClient::_init() {
    _nvsManager = new NVSManager();

    String ap_ssid = String(_deviceType) + "-" + String(_deviceApp);
    String ap_password = String(_token).substring(String(_token).length() - 10);
    _wifiManager = new WifiManager(_nvsManager, ap_ssid, ap_password, [this]() { this->reconnect(); }, [this]() { this->stopReconnect(); });
    if (_handleWifi) {
        _wifiManager->init();
        _webserverManager = new WebserverManager(_wifiManager);
    }

    _otaManager = new OTAManager();

    reconnect();
}

void SocketClient::init(const SocketClientConfig *config) {
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

void SocketClient::init(const char *socketHostURL, int port, bool _isSSL) {
    setSocketHost(socketHostURL, port, _isSSL);
    _init();
}

void SocketClient::loop() {
    if (_handleWifi) {
        _wifiManager->loop();
    }

    _webserverManager->loop();
    webSocket.loop();
}
