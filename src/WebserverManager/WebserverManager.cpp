#include "WebserverManager.h"

WebserverManager::WebserverManager(WifiManager *wifiManager, SendStatusFunction sendStatus, ReceivedCommandFunction receivedCommand, EntityChangedFunction entityChanged)
    : _server(80), _ws("/ws"), _wsMessage() {
    _wifiManager = wifiManager;
    _sendStatus = sendStatus;
    _receivedCommand = receivedCommand;
    _entityChanged = entityChanged;
    _setupWebServer();
    _server.begin();
}

void WebserverManager::_setupWebServer() {
    // WS setup
    // Attach WebSocket handler
    _ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) { _onWsEvent(server, client, type, arg, data, len); });

    _server.addHandler(&_ws);

    // Serve a simple HTML form
    _server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) { this->_handleRoot(request); });

    // Handle form submission
    _server.on("/connect", HTTP_POST, [this](AsyncWebServerRequest *request) { this->_handleWifiConnect(request); });

    _server.on("/wsdemo", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(
            200, "text/html",
            "<!DOCTYPE html><html><head><title>ESP8266 WebSocket Demo</title><meta name='viewport' content='width=device-width, "
            "initial-scale=1'><style>body{font-family:Arial,sans-serif;background:#f2f2f2;padding:2em}#log{white-space:pre-wrap;background:#fff;padding:1em;border:1px solid "
            "#ccc;height:200px;overflow-y:scroll}input[type=text]{width:80%;padding:10px;font-size:16px}button{padding:10px 16px;font-size:16px;margin-left:8px}</style></head><body><h2>ESP8266 "
            "WebSocket Test</h2><div id='log'>Connecting to WebSocket...</div><br><input id='msg' type='text' placeholder='Type a message'><button onclick='sendMessage()'>Send</button><script>let "
            "log=document.getElementById('log');let ws=new WebSocket(`ws://${window.location.hostname}/ws`);ws.onopen=()=>logMessage('WebSocket connected');ws.onmessage=e=>logMessage('Received: "
            "'+e.data);ws.onclose=()=>logMessage('WebSocket disconnected');ws.onerror=e=>logMessage('WebSocket error');function "
            "logMessage(m){log.textContent+=m+'\\n';log.scrollTop=log.scrollHeight}function sendMessage(){let "
            "i=document.getElementById('msg');if(ws.readyState===WebSocket.OPEN){ws.send(i.value);logMessage('Sent: '+i.value);i.value=''}else{logMessage('WebSocket not "
            "connected')}}</script></body></html>");
    });
    // _server.on("/leave", HTTP_GET, [this]() {
    //     _server.send(200, "text/plain", "Leaving Wi-Fi...");
    //     Serial.println("Leaving Wi-Fi...");
    //     WiFi.disconnect(true);
    //     //  _initAPMode();
    // });

    // _server.on("/restart", HTTP_GET, [this]() {
    //     _server.send(200, "text/plain", "Restarting...");
    //     Serial.println("Restarting...");
    //     ESP.restart();
    // });

    // _server.on("/status", HTTP_GET, [this]() { _server.send(200, "text/plain", "Status: OK"); });

    // _server.on("/scan", HTTP_GET, [this]() {
    //     _server.send(200, "text/plain", "Scanning Wi-Fi networks...");
    //     Serial.println("Scanning Wi-Fi networks...");
    //     int n = WiFi.scanNetworks();
    //     String networks = "Found " + String(n) + " networks:\n";
    //     for (int i = 0; i < n; ++i) {
    //         networks += WiFi.SSID(i) + "\n";
    //     }
    //     _server.send(200, "text/plain", networks);
    // });

    // Serve the form for any URL
    _server.onNotFound([this](AsyncWebServerRequest *request) { this->_handleRoot(request); });
}

void WebserverManager::_handleRoot(AsyncWebServerRequest *request) {
    request->send(
        200, "text/html",
        "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>*{box-sizing:border-box;margin:0;padding:0}html,body{font-family:Arial,sans-serif;height:100%;width:100%;background:#f2f2f2}body{display:flex;justify-content:center;align-items:"
        "center;padding:20px}form{width:100%;max-width:400px;padding:24px;border-radius:12px;box-shadow:0 4px 12px "
        "rgba(0,0,0,0.1)}label{display:block;font-size:16px;font-weight:bold}input[type='text'],input[type='password']{width:100%;padding:12px;margin-bottom:16px;margin-top:5px;border:1px solid "
        "#ccc;border-radius:6px;font-size:16px}input[type='submit']{width:100%;padding:14px;background:#4CAF50;color:#fff;border:none;border-radius:6px;font-size:18px;cursor:pointer}input[type='"
        "submit']:hover{background:#45a049}.toggle{display:flex;align-items:center;gap:8px;margin-bottom:16px}@media(max-height:500px){body{align-items:flex-start;padding-top:40px}}</style>"
        "<script>function togglePassword(){var p=document.getElementById('password');p.type=p.type==='password'?'text':'password';}</script>"
        "</head><body>"
        "<form action='/connect' method='POST'>"
        "<label for='ssid'>SSID:</label>"
        "<input type='text' id='ssid' name='ssid' placeholder='Enter WiFi SSID'>"
        "<label for='password'>Password:</label>"
        "<input type='password' id='password' name='password' placeholder='Enter WiFi Password'>"
        "<div class='toggle'><input type='checkbox' id='show' onclick='togglePassword()'><label for='show'>Show Password</label></div>"
        "<input type='submit' value='Connect'>"
        "</form></body></html>");
}

void WebserverManager::_handleWifiConnect(AsyncWebServerRequest *request) {
    String ssid, password;
    if (request->hasParam("ssid", true)) {
        ssid = request->getParam("ssid", true)->value();
    }
    if (request->hasParam("password", true)) {
        password = request->getParam("password", true)->value();
    }
    ssid.trim();
    password.trim();

    if (ssid.length() > 0 && password.length() > 0) {
        request->send(200, "text/plain", "Connecting to Wi-Fi...");
        MY_LOGI(SERVER_TAG, "Received Wi-Fi credentials: SSID='%s', Password='%s'", ssid.c_str(), password.c_str());

        _wifiManager->saveWifiCredentials(ssid, password);
        _wifiManager->init();
    } else {
        request->send(400, "text/plain", "Invalid SSID or Password");
    }
}

void WebserverManager::_onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected\n", client->id());
            // _wsMessage.clear();
            // _wsMessage["message"] = "Hello Client!";
            // client->text(_wsMessage.dump());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            AwsFrameInfo *info;
            info = (AwsFrameInfo *)arg;
            if (info->final && info->index == 0 && info->len == len) {
                if (info->opcode == WS_TEXT) {
                    data[len] = 0;  // null-terminate text
                    MY_LOGV(SERVER_TAG, "%s", (char *)data);
                    // client->text("Echo: " + String((char *)data));
                    // _ws.textAll(String((char *)data));  // Echo back to all clients
                    _wsMessage.clear();
                    deserializeJson(_wsMessage, (char *)data);
                    if (_wsMessage.containsKey("message")) {
                        if (_wsMessage["message"] == "connect") {
                            _handleWSConnected();
                        } else if (_wsMessage["message"] == "subscribeToDevice") {
                            _handleSubToDevice();
                        } else if (_wsMessage["message"] == "askStatus") {
                            _handleAskStatus();
                        } else if (_wsMessage["message"] == "command") {
                            _handleCommand();
                        } else if (_wsMessage["message"] == "entityChanged") {
                            _handleEntityChanged();
                        } else {
                            MY_LOGI(SERVER_TAG, "Unknown message: %s", _wsMessage["message"].as<String>().c_str());
                        }
                    }
                }
            }
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void WebserverManager::_handleWSConnected() {
    _wsMessage.clear();
    _wsMessage["message"] = "connected";
    _wsMessage["result"] = "success";
    _wsMessage["data"] = "";
    String JsonToSend = "";
    serializeJson(_wsMessage, JsonToSend);
    _ws.textAll(JsonToSend);
}

void WebserverManager::_handleSubToDevice() {
    _wsMessage.clear();
    _wsMessage["message"] = "subscribed";
    _wsMessage["result"] = "success";
    String JsonToSend = "";
    serializeJson(_wsMessage, JsonToSend);
    _ws.textAll(JsonToSend);
}

void WebserverManager::_handleAskStatus() {
    _wsMessage.clear();
    _wsMessage["message"] = "returningStatus";
    _wsMessage.createNestedObject("data");
    _sendStatus(_wsMessage["data"]);
    _wsMessage["save"] = false;

    String JsonToSend = "";
    serializeJson(_wsMessage, JsonToSend);
    MY_LOGD(SERVER_TAG, "Returning status: %s", JsonToSend.c_str());
    _ws.textAll(JsonToSend);
}

void WebserverManager::_handleCommand() { _receivedCommand(_wsMessage); }

void WebserverManager::_handleEntityChanged() { _entityChanged(_wsMessage); }

void WebserverManager::publishStatus(String statusJson)
{
    _ws.textAll(statusJson);
}

void WebserverManager::loop() {}
