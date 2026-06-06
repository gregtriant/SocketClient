#include "SocketClient.h"

#include "WebserverManager/WebserverManager.h"
#include "WifiManager/WifiManager.h"

#include "Log/Log.h"
#include "SocketClientDefs.h"

#include "TimeClient/TimeClient.h"
#include <stdarg.h>

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
//         SC_LOGD(WS_TAG, "%u", x);
//         SC_LOGD(WS_TAG, "* reconnect *\n");
//         last_reconnect = millis();
//         reconnect_time += 60000;
//         if (reconnect_time > max_reconnect_time)
//             reconnect_time = max_reconnect_time;
//         sc->reconnect();
//         return true;
//     }

//     if (wsc.isConnected()) {
//         if (wsc.sendPing()) {
//             SC_LOGD(WS_TAG, "*");
//             // last_dog = millis();
//         } else {
//             // wsc.disconnect();
//             SC_LOGD(WS_TAG, "@");
//         }
//     }

//     if (last_dog > 0 && millis() - last_dog > watchdog_time) {
//         SC_LOGD(WS_TAG, "* watchdog time *\n");
//         wsc.disconnect();
//         return true;
//     }
//     if (last_png > 0 && millis() - last_png > watchdog_time) {
//         SC_LOGD(WS_TAG, "* png watchdog time *\n");
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
    SC_LOGD(WS_TAG, "%s", stringData.c_str());
}

void SocketClient_entityChanged(JsonDoc doc) {
    String stringData = "";
    serializeJson(doc, stringData);
    SC_LOGD(WS_TAG, "%s", stringData.c_str());
}

void SocketClient_connected(JsonDoc doc) {
    String stringData = "";
    serializeJson(doc, stringData);
    SC_LOGD(WS_TAG, "%s", stringData.c_str());
}

SocketClient *globalSC = nullptr;

SocketClient::SocketClient() :
    _webSocket(nullptr),
    _nvsManager(nullptr),
    _diagnostics(nullptr),
    _wifiManager(nullptr),
    _webserverManager(nullptr),
    _otaManager(nullptr),
    _doc() //-(JSON_SIZE)
{
    static int count = 0;
    count++;
    if (count > 1) {
        SC_LOGE(WS_TAG, "Too many SocketClients created");
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
    delete _diagnostics;
    _diagnostics = nullptr;
    delete _wifiManager;
    _wifiManager = nullptr;
    delete _nvsManager;
    _nvsManager = nullptr;
    delete _webserverManager;
    _webserverManager = nullptr;
	delete _otaManager;
	_otaManager = nullptr;
}

static const char *DEBUG_LEVEL_STRINGS[] = { "error", "warning", "info", "debug", "verbose" };
static const uint16_t DEBUG_LOG_MAX_TEXT_LENGTH = 256;

// Strong override of the weak no-op in Log.cpp — routes SC_LOG* macros to the server.
void _scRemoteLog(uint8_t level, const char *tag, const char *fmt, ...) {
    if (!globalSC) return;
    char buf[DEBUG_LOG_MAX_TEXT_LENGTH];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    globalSC->sendDebugLog(level, String(buf));
}

uint8_t SocketClient::_levelStringToIndex(const char *level) {
    for (uint8_t i = 0; i <= DEBUG_LEVEL_VERBOSE; i++) {
        if (strcmp(level, DEBUG_LEVEL_STRINGS[i]) == 0) return i;
    }
    return DEBUG_LEVEL_NONE;
}

void SocketClient::_applyDebugConfig(JsonObject debug) {
    if (debug["logs"].is<JsonObject>()) {
        JsonObject logs = debug["logs"].as<JsonObject>();
        _debugLoggingEnabled = logs["enabled"] | false;
        const char *lvl = logs["level"];
        if (lvl) _debugLogLevelIndex = _levelStringToIndex(lvl);
    }
    SC_LOGD(WS_TAG, "Debug config: logs=%s level=%d",
            _debugLoggingEnabled ? "on" : "off",
            _debugLogLevelIndex);
}

void SocketClient::sendDebugLog(uint8_t level, const String &message) {
    if (!_debugLoggingEnabled) return;
    if (level > _debugLogLevelIndex) return;
    if (!_webSocket || !_webSocket->isConnected()) return;

    // Use a local doc to avoid clobbering _doc when called from within gotMessageSocket.
    JsonDocument logDoc;
    logDoc["message"] = "@debugLog";
    logDoc["level"] = DEBUG_LEVEL_STRINGS[level];
    logDoc["text"] = message.length() <= DEBUG_LOG_MAX_TEXT_LENGTH
                         ? message
                         : message.substring(0, DEBUG_LOG_MAX_TEXT_LENGTH);

    if (_tc.hasTime()) {
        int hh, mm, ss;
        _tc.getTime(hh, mm, ss);
        char tsBuf[10];
        snprintf(tsBuf, sizeof(tsBuf), "%02d:%02d:%02d", hh, mm, ss);
        logDoc["timestamp"] = tsBuf;
    }

    String textToSend = "";
    serializeJson(logDoc, textToSend);
    _webSocket->sendTXT(textToSend);
}

void SocketClient::sendLog(const String &message) {
    if (!_webSocket || !_webSocket->isConnected())
        return;

    _doc.clear();
    _doc["message"] = "@log";
    _doc["text"] = message;

    String textToSend = "";
    serializeJson(_doc, textToSend);
    SC_LOGD(WS_TAG, "Sending Log: %s", textToSend.c_str());
    _webSocket->sendTXT(textToSend);
}

void SocketClient::sendNotification(const String &message) {
    if (!_webSocket || !_webSocket->isConnected()) {
        return;
    }

	_doc.clear();
    _doc["message"] = "notification";
    _doc["body"] = message;

    String textToSend = "";
    serializeJson(_doc, textToSend);
    SC_LOGD(WS_TAG, "Sending notification: %s", textToSend.c_str());
    _webSocket->sendTXT(textToSend);
}

void SocketClient::sendNotification(const String &message, const JsonDoc &doc) {
    if (!_webSocket || !_webSocket->isConnected()) {
        return;
    }

	_doc.clear();
    _doc["message"] = "notification";
    _doc["body"] = message;
    _doc["options"] = doc;

    String textToSend = "";
    serializeJson(_doc, textToSend);
    SC_LOGD(WS_TAG, "Sending notification: %s", textToSend.c_str());
    _webSocket->sendTXT(textToSend);
}

bool SocketClient::hasTime(){
    return _tc.hasTime();
}

void SocketClient::gotMessageSocket(uint8_t *payload) {
    SC_LOGD(WS_TAG, "Got data: %s", payload);
    deserializeJson(_doc, payload);
    if (strcmp(_doc["message"], "connected") == 0) {
        // Get the Time first before the JSON gets cleared.
        if (!_doc["time"].isNull()) {
            String tz = _doc["time"]["timezone"];
            if (!tz.isEmpty()) {
                _local_time_zone = tz;
                _tc.begin(_local_time_zone.c_str());
            } else {
                SC_LOGE(WS_TAG, "Timezone missing or invalid!");
            }
        }

        // Read debug config sent by the server on connect.
        if (_doc["debug"].is<JsonObject>()) {
            JsonObject debug = _doc["debug"].as<JsonObject>();
            _applyDebugConfig(debug);
            bool diagEnabled = debug["diagnostics"]["enabled"] | false;
            _diagnostics->onConnected(diagEnabled);
        }

        if (!_doc["data"].isNull()) {
            // check if _doc["data"] is a string
            if (_doc["data"].is<const char *>()) {
                SC_LOGD(WS_TAG, "data is string");
                // TODO: remove this when the server is fixed and all the devices are updated
                JsonDocument tempDoc;//-(2024);
                deserializeJson(tempDoc, _doc["data"]);
                connected(tempDoc);
            } else {
                SC_LOGD(WS_TAG, "data is json");
                connected(_doc["data"]);
            }
        }
    } else if (strcmp(_doc["message"], "debugConfig") == 0) {
        if (_doc["debug"].is<JsonObject>()) {
            JsonObject debug = _doc["debug"].as<JsonObject>();
            _applyDebugConfig(debug);
            bool diagEnabled = debug["diagnostics"]["enabled"] | false;
            _diagnostics->onDebugConfig(diagEnabled);
        }
    } else if (strcmp(_doc["message"], "command") == 0) {
		receivedCommand(_doc);
    } else if (strcmp(_doc["message"], "askStatus") == 0) {
        sendStatusWithSocket();
    } else if (strcmp(_doc["message"], "entityChanged") == 0) {
        entityChanged(_doc);
    } else if (strcmp(_doc["message"], "update") == 0) {
        String updateURL = _doc["url"];
        SC_LOGD(WS_TAG, "Update URL: %s", updateURL.c_str());
        _otaManager->startOTA(updateURL);
    } else if (strcmp(_doc["message"], "fileReady") == 0) {
        String fileUrl = _doc["url"].as<String>();
        if (fileUrl.isEmpty()) { SC_LOGE(WS_TAG, "fileReady: missing url"); return; }
        _downloadFile(fileUrl, _doc["filename"].as<String>(), _doc["size"].as<size_t>());
    } else if (strcmp(_doc["message"], "requestFile") == 0) {
        String fileUrl = _doc["url"].as<String>();
        if (fileUrl.isEmpty()) { SC_LOGE(WS_TAG, "requestFile: missing url"); return; }
        _uploadFile(fileUrl, _doc["filename"].as<String>());
    }
}

void SocketClient::sendStatusWithSocket(bool save /*=false*/) {
    if (!_webSocket || !_webSocket->isConnected()) {
        return;
    }
	_doc.clear();
    _doc["message"] = "returningStatus";
    _doc["data"].to<JsonObject>(); // Create a new JsonObject for "data" field.
    sendStatus(_doc["data"]);
    _doc["save"] = save;

    String JsonToSend = "";
    serializeJson(_doc, JsonToSend);
    SC_LOGD(WS_TAG, "Returning status: %s", JsonToSend.c_str());
    _webSocket->sendTXT(JsonToSend);
}

void SocketClient_webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_ERROR:
            SC_LOGD(WS_TAG, "Error! : %s", payload);
            break;
        case WStype_DISCONNECTED:
            SC_LOGD(WS_TAG, "Disconnected!");
            globalSC->last_dog = 0;
            globalSC->last_png = 0;
            break;
        case WStype_CONNECTED: {
            globalSC->_doc.clear();
            globalSC->last_dog = millis();
            globalSC->last_png = millis();
            // SC_LOGD(WS_TAG, "Connected to url: %s", payload);
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
                SC_LOGD(WS_TAG, "Connected to the server!");
            } else {
                SC_LOGE(WS_TAG, "Failed to send connection message");
            }
        } break;
        case WStype_TEXT: {
            globalSC->last_dog = millis();
            globalSC->gotMessageSocket(payload);
        } break;
        case WStype_BIN:
            globalSC->last_dog = millis();
            SC_LOGD(WS_TAG, "Get binary length: %u", length);
            break;
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            // Not handled
            break;
        case WStype_PING:
            SC_LOGV2(".");
            globalSC->last_png = millis();
            break;
        case WStype_PONG:
            SC_LOGV2("-");
            globalSC->last_dog = millis();
            break;
    }
}

void SocketClient::reconnect() {
    if (!WiFi.isConnected()) {
        SC_LOGD(WS_TAG, "<No WiFi>");
        return;
    }

    WiFi.hostname(String(_deviceType) + "-" + String(_deviceApp));

    SC_LOGD(WS_TAG, "<reconnect>");
    if (_webSocket) {
        if (_webSocket->isConnected()) {
            _webSocket->disconnect();
        }
        delete _webSocket;
        _webSocket = nullptr;
    }
    _webSocket = new WebSocketsClient();
    // Reinitialize the WebSocket connection
    _webSocket->onEvent(SocketClient_webSocketEvent);  // Reattach the event handler
    _webSocket->setReconnectInterval(5000);            // Set reconnect interval
    _webSocket->enableHeartbeat(15000, 30000, 3);       // Enable heartbeat


    // Attempt to reconnect
    if (_isSSL) {
        _webSocket->beginSSL(_socketHostURL, _port, "/");  // Use SSL connection
    } else {
        _webSocket->begin(_socketHostURL, _port, "/");  // Use non-SSL connection
    }
}

void SocketClient::stopReconnect() {
    if (!_webSocket) return;
    _webSocket->setReconnectInterval(MAX_ULONG);
    _webSocket->enableHeartbeat(MAX_ULONG, MAX_ULONG, 255);
}

void SocketClient::_init() {
    _webSocket = nullptr;
    _wifiManager = nullptr;

    _nvsManager = new NVSManager();
    _diagnostics = new Diagnostics(_nvsManager, [this](const String& msg) {
        if (_webSocket && _webSocket->isConnected()) {
            String txt = msg;
            _webSocket->sendTXT(txt);
        }
    });
    _diagnostics->begin();

    if (_handleWifi) {
        String ap_ssid = String(_deviceType) + "-" + String(_deviceApp);
        String ap_password = String(_token).substring(String(_token).length() - 10);
        _wifiManager = new WifiManager(_nvsManager, ap_ssid, ap_password,
            [this]() { this->reconnect(); },
            [this]() { this->stopReconnect(); });
        _wifiManager->init();
    }

    _otaManager = new OTAManager();
    reconnect();
}

void SocketClient::initWebserver(int port) {
    if (_webserverManager) {
        SC_LOGW(WS_TAG, "Webserver already initialized");
        return;
    }
    _webserverManager = new WebserverManager(port, _wifiManager, &_deviceInfo,
                                             [this]() { return this->getCurrentStatus(); });
}

AsyncWebServer* SocketClient::getServer() {
    if (_webserverManager) {
        return _webserverManager->getServer();
    }
    return nullptr;
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
    ASSIGN_IF_NOT_NULLPTR(_fileReceived,  config->fileReceived);
    ASSIGN_IF_NOT_NULLPTR(_fileRequested, config->fileRequested);

    _version = config->version;
    _port = config->port;
    _isSSL = config->isSSL;
    _handleWifi = config->handleWifi;

    // Copy socket config to _deviceInfo.
    _deviceInfo.app = config->name;
    _deviceInfo.type = config->type;
    _deviceInfo.version = config->version;

    _init();
}

void SocketClient::init(const char *socketHostURL, int port, bool _isSSL) {
    setSocketHost(socketHostURL, port, _isSSL);
    _init();
}

void SocketClient::loop() {
    if (_wifiManager) _wifiManager->loop();
    if (_webserverManager) _webserverManager->loop();
    if (_webSocket) _webSocket->loop();
    if (_diagnostics) _diagnostics->loop();
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

void SocketClient::_downloadFile(const String &url, const String &filename, size_t size) {
    WiFiClientSecure secureClient;
    WiFiClient plainClient;
    HTTPClient http;

    bool ok;
    if (url.startsWith("https://")) {
        secureClient.setInsecure();
        ok = http.begin(secureClient, url);
    } else {
        ok = http.begin(plainClient, url);
    }
    if (!ok) {
        SC_LOGE(WS_TAG, "download: http.begin failed: %s", url.c_str());
        return;
    }
    http.addHeader("x-mac-address", WiFi.macAddress());

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        SC_LOGE(WS_TAG, "download: HTTP %d", code);
        http.end();
        return;
    }

    size_t clampedSize = (size < 4096) ? size : 4096;
    std::vector<uint8_t> buf(clampedSize);
    size_t actual = http.getStream().readBytes(buf.data(), clampedSize);
    buf.resize(actual);
    http.end();

    SC_LOGD(WS_TAG, "download: '%s' %u bytes", filename.c_str(), actual);
    if (_fileReceived) {
        _fileReceived(filename, buf);
    }
}

void SocketClient::_uploadFile(const String &url, const String &filename) {
    const String boundary = "ESP32Boundary";
    String fname = filename.isEmpty() ? "upload.bin" : filename;

    std::vector<uint8_t> fileBuf;
    if (_fileRequested) {
        _fileRequested(fname, fileBuf);
    } else {
        const char *p = "Hello from device!";
        fileBuf.assign(p, p + strlen(p));
    }

    if (fileBuf.empty()) {
        SC_LOGE(WS_TAG, "upload: empty buffer");
        return;
    }
    if (fileBuf.size() > 4096) {
        SC_LOGE(WS_TAG, "upload: file too large (%u bytes)", fileBuf.size());
        return;
    }

    String header = "--" + boundary + "\r\n"
                    "Content-Disposition: form-data; name=\"file\"; filename=\"" + fname + "\"\r\n"
                    "Content-Type: application/octet-stream\r\n"
                    "\r\n";
    String footer = "\r\n--" + boundary + "--\r\n";

    std::vector<uint8_t> body;
    body.reserve(header.length() + fileBuf.size() + footer.length());
    body.insert(body.end(), header.c_str(), header.c_str() + header.length());
    body.insert(body.end(), fileBuf.begin(), fileBuf.end());
    body.insert(body.end(), footer.c_str(), footer.c_str() + footer.length());

    WiFiClientSecure secureClient;
    WiFiClient plainClient;
    HTTPClient http;

    bool ok;
    if (url.startsWith("https://")) {
        secureClient.setInsecure();
        ok = http.begin(secureClient, url);
    } else {
        ok = http.begin(plainClient, url);
    }
    if (!ok) {
        SC_LOGE(WS_TAG, "upload: http.begin failed: %s", url.c_str());
        return;
    }
    http.addHeader("x-mac-address", WiFi.macAddress());
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

    int code = http.POST(body.data(), body.size());
    if (code != HTTP_CODE_OK) {
        SC_LOGE(WS_TAG, "upload: HTTP %d", code);
    } else {
        SC_LOGD(WS_TAG, "upload: HTTP %d", code);
    }
    http.end();
}
