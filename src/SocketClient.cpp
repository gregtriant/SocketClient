#include "SocketClient.h"

#include "WebserverManager/WebserverManager.h"
#include "WifiManager/WifiManager.h"

#include "Log/Log.h"
#include "SocketClientDefs.h"

#include "TimeClient/TimeClient.h"

#ifndef UPDATE_SIZE_UNKNOWN
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#endif

unsigned long SocketClient::last_dog = 0;
unsigned long SocketClient::last_png = 0;
unsigned long SocketClient::last_reconnect = 0;
unsigned long SocketClient::reconnect_time = 30000;  //- 30 sec

// bool SocketClient::watchdog(void *vv) {
//     SocketClient *sc = (SocketClient *)vv;
//     if (!sc)
//         return true;
//     WebSocketsClient &wsc = sc->webSocket;
//     if (!wsc.isConnected() && (last_reconnect == 0 || (millis() - last_reconnect) > reconnect_time)) {
//         unsigned int x = millis() / (60000);
//         MY_LOGD(WS_TAG, "%u", x);
//         MY_LOGD(WS_TAG, "* reconnect *\n");
//         last_reconnect = millis();
//         reconnect_time += 60000;
//         if (reconnect_time > max_reconnect_time)
//             reconnect_time = max_reconnect_time;
//         sc->reconnect();
//         return true;
//     }

//     if (wsc.isConnected()) {
//         if (wsc.sendPing()) {
//             MY_LOGD(WS_TAG, "*");
//             // last_dog = millis();
//         } else {
//             // wsc.disconnect();
//             MY_LOGD(WS_TAG, "@");
//         }
//     }

//     if (last_dog > 0 && millis() - last_dog > watchdog_time) {
//         MY_LOGD(WS_TAG, "* watchdog time *\n");
//         wsc.disconnect();
//         return true;
//     }
//     if (last_png > 0 && millis() - last_png > watchdog_time) {
//         MY_LOGD(WS_TAG, "* png watchdog time *\n");
//         wsc.disconnect();
//         return true;
//     }
//     return true;
// }

// Initialize default functions for the user
void SocketClient_sendStatus(JsonDoc status) {
    status["message"] = "hello";
}

void SocketClient_receivedCommand(JsonDoc doc) {
    String stringData = "";
    serializeJson(doc, stringData);
    MY_LOGD(WS_TAG, "%s", stringData.c_str());
}

void SocketClient_entityChanged(JsonDoc doc) {
    String stringData = "";
    serializeJson(doc, stringData);
    MY_LOGD(WS_TAG, "%s", stringData.c_str());
}

void SocketClient_connected(JsonDoc doc) {
    String stringData = "";
    serializeJson(doc, stringData);
    MY_LOGD(WS_TAG, "%s", stringData.c_str());
}

SocketClient *globalSC = nullptr;

SocketClient::SocketClient() :
    _webSocket(nullptr),
    _nvsManager(nullptr),
    _wifiManager(nullptr),
    _webserverManager(nullptr),
    _otaManager(nullptr),
    _doc() //-(JSON_SIZE)
{
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
    if (_webSocket) {
        _webSocket->disconnect();
    }
    delete _webSocket;
    _webSocket = nullptr;
    delete _wifiManager;
    _wifiManager = nullptr;
    delete _nvsManager;
    _nvsManager = nullptr;
    delete _webserverManager;
    _webserverManager = nullptr;
	delete _otaManager;
	_otaManager = nullptr;
}

void SocketClient::sendLog(const String &message) {
    if (!_webSocket->isConnected())
        return;

    _doc.clear();
    _doc["message"] = "@log";
    _doc["text"] = message;

    String textToSend = "";
    serializeJson(_doc, textToSend);
    MY_LOGD(WS_TAG, "Sending Log: %s", textToSend.c_str());
    _webSocket->sendTXT(textToSend);
}

void SocketClient::sendNotification(const String &message) {
    if (!_webSocket->isConnected()) {
        return;
    }

	_doc.clear();
    _doc["message"] = "notification";
    _doc["body"] = message;

    String textToSend = "";
    serializeJson(_doc, textToSend);
    MY_LOGD(WS_TAG, "Sending notification: %s", textToSend.c_str());
    _webSocket->sendTXT(textToSend);
}

void SocketClient::sendNotification(const String &message, const JsonDoc &doc) {
    if (!_webSocket->isConnected()) {
        return;
    }

	_doc.clear();
    _doc["message"] = "notification";
    _doc["body"] = message;
    _doc["options"] = doc;

    String textToSend = "";
    serializeJson(_doc, textToSend);
    MY_LOGD(WS_TAG, "Sending notification: %s", textToSend.c_str());
    _webSocket->sendTXT(textToSend);
}

bool SocketClient::hasTime(){
    return _tc.hasTime();
}

void SocketClient::gotMessageSocket(uint8_t *payload) {
    MY_LOGD(WS_TAG, "Got data: %s", payload);
    deserializeJson(_doc, payload);
    if (strcmp(_doc["message"], "connected") == 0) {
        // Get the Time first before the JSON gets cleared.
        if (!_doc["time"].isNull()) {
            String tz = _doc["time"]["timezone"];
            if (!tz.isEmpty()) {
                _local_time_zone = tz;
                _tc.begin(_local_time_zone.c_str());
            } else {
                MY_LOGE(WS_TAG, "Timezone missing or invalid!");
            }
        }

		if (!_doc["data"].isNull()) {
            // check if _doc["data"] is a string
            if (_doc["data"].is<const char *>()) {
                MY_LOGD(WS_TAG, "data is string");
                // TODO: remove this when the server is fixed and all the devices are updated
                JsonDocument tempDoc;//-(2024);
                deserializeJson(tempDoc, _doc["data"]);
                connected(tempDoc);
            } else {
                MY_LOGD(WS_TAG, "data is json");
                connected(_doc["data"]);
            }
		}
    } else if (strcmp(_doc["message"], "command") == 0) {
		receivedCommand(_doc);
    } else if (strcmp(_doc["message"], "askStatus") == 0) {
        sendStatusWithSocket();
    } else if (strcmp(_doc["message"], "entityChanged") == 0) {
        entityChanged(_doc);
    } else if (strcmp(_doc["message"], "update") == 0) {
        String updateURL = _doc["url"];
        MY_LOGD(WS_TAG, "Update URL: %s", updateURL.c_str());
        _otaManager->startOTA(updateURL);
    }
}

void SocketClient::sendStatusWithSocket(bool save /*=false*/) {
    if (!_webSocket->isConnected()) {
        return;
    }
	_doc.clear();
    _doc["message"] = "returningStatus";
    _doc["data"].to<JsonObject>(); // Create a new JsonObject for "data" field.
    sendStatus(_doc["data"]);
    _doc["save"] = save;

    String JsonToSend = "";
    serializeJson(_doc, JsonToSend);
    MY_LOGD(WS_TAG, "Returning status: %s", JsonToSend.c_str());
    _webSocket->sendTXT(JsonToSend);
}

void SocketClient_webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
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
            globalSC->_doc.clear();
            globalSC->last_dog = millis();
            globalSC->last_png = millis();
            // MY_LOGD(WS_TAG, "Connected to url: %s", payload);
            // Prepare and send a "connect" message
            globalSC->_doc["message"] = "connect";
            globalSC->_doc["deviceId"] = WiFi.macAddress();//- globalSC->_wifiManager->getMacAddress();
            globalSC->_doc["deviceApp"] = globalSC->_deviceApp;
            globalSC->_doc["deviceType"] = globalSC->_deviceType;
            globalSC->_doc["version"] = globalSC->_version;
            globalSC->_doc["localIP"] = WiFi.localIP().toString(); //-globalSC->_wifiManager->getIP();
            globalSC->_doc["token"] = globalSC->_token;
            String JsonToSend = "";
            serializeJson(globalSC->_doc, JsonToSend);
            globalSC->last_reconnect = 0;
            globalSC->reconnect_time = 30000;
            bool result = globalSC->_webSocket->sendTXT(JsonToSend);
            if (result) {
                MY_LOGD(WS_TAG, "Connected to the server!");
            } else {
                MY_LOGE(WS_TAG, "Failed to send connection message");
            }
        } break;
        case WStype_TEXT: {
            globalSC->last_dog = millis();
            globalSC->gotMessageSocket(payload);
        } break;
        case WStype_BIN:
            globalSC->last_dog = millis();
            MY_LOGD(WS_TAG, "Get binary length: %u", length);
            break;
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            // Not handled
            break;
        case WStype_PING:
            MY_LOGV2(".");
            globalSC->last_png = millis();
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

    if (_webSocket->isConnected()) {
        _webSocket->disconnect();
    }
    MY_LOGD(WS_TAG, "<reconnect>");
    // Clean up any lingering resources
    if (_webSocket) {
        _webSocket->~WebSocketsClient();
        _webSocket = nullptr;
    }
    _webSocket = new WebSocketsClient();
    // Reinitialize the WebSocket connection
    _webSocket->onEvent(SocketClient_webSocketEvent);  // Reattach the event handler
    _webSocket->setReconnectInterval(5000);            // Set reconnect interval
    _webSocket->enableHeartbeat(5000, 12000, 2);       // Enable heartbeat


    // Attempt to reconnect
    if (_isSSL) {
        _webSocket->beginSSL(_socketHostURL, _port, "/");  // Use SSL connection
    } else {
        _webSocket->begin(_socketHostURL, _port, "/");  // Use non-SSL connection
    }
}

void SocketClient::stopReconnect() {
    _webSocket->setReconnectInterval(MAX_ULONG);
    _webSocket->enableHeartbeat(MAX_ULONG, MAX_ULONG, 255);
}

void SocketClient::_init() {
    _webSocket = new WebSocketsClient();
    _nvsManager = new NVSManager();

    String ap_ssid = String(_deviceType) + "-" + String(_deviceApp);
    String ap_password = String(_token).substring(String(_token).length() - 10);
    _wifiManager = new WifiManager(_nvsManager, ap_ssid, ap_password, [this]() { this->reconnect(); }, [this]() { this->stopReconnect(); });
    if (_handleWifi) {
        _wifiManager->init();
        _webserverManager = new WebserverManager(_wifiManager,
                                                &_deviceInfo,
                                                [this]() { return this->getCurrentStatus(); });
    }

    _otaManager = new OTAManager();

    reconnect();
}

void SocketClient::init(const SocketClientConfig_t *config) {
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

    // Copy socket config to _deviceInfo.
    _deviceInfo.name = config->name;
    _deviceInfo.type = config->type;
    _deviceInfo.version = config->version;

    _init();
}

void SocketClient::init(const char *socketHostURL, int port, bool _isSSL) {
    setSocketHost(socketHostURL, port, _isSSL);
    _init();
}

void SocketClient::loop() {
    if (_handleWifi) {
        _wifiManager->loop();
        _webserverManager->loop();
    }

    _webSocket->loop();
    _tc.loop();
}

String SocketClient::getCurrentStatus() {
    _doc.clear();
    sendStatus(_doc);
    String JsonToSend = "";
    serializeJson(_doc, JsonToSend);
    return JsonToSend;
}

String SocketClient::getVersion() {
    _doc.clear();
    _doc["version"] = _version;
    String JsonToSend = "";
    serializeJson(_doc, JsonToSend);
    return JsonToSend;
}
