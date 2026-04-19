#include "WebserverManager.h"

#include "WebserverManager/WebserverManager.h"
#include "WifiManager/WifiManager.h"

#include "../Log/Log.h"


WebserverManager::WebserverManager(int port, WifiManager *wifiManager, DeviceInfo_t *deviceInfo, std::function<String()> getCurrentStatus)
    : _server(port)
{
  _wifiManager = wifiManager;
  _getCurrentStatus = getCurrentStatus;
  _deviceInfo = deviceInfo;
  _setupWebServer();
}


#include "pages/common_css.h"
#include "pages/main_html.h"
#include "pages/reboot_html.h"
#include "pages/upload_html.h"
#include "pages/wifi_html.h"

void WebserverManager::_setupWebServer()
{
    // Connect to Wifi form submission.
    _server.on("/sc/wifi/connect", HTTP_POST, [this](AsyncWebServerRequest *request)
        {
            this->_handleWifiConnect(request);
        });

    _server.on("/sc/wifi/disconnect", HTTP_GET, [this](AsyncWebServerRequest *request)
        {
            request->send(200, "text/plain", "Disconnecting from Wi-Fi...");
            MY_LOGD(SERVER_TAG, "Disconnecting from Wi-Fi...");
            WiFi.disconnect(true);
            //  _initAPMode();
        });

    _server.on("/sc/reboot", HTTP_GET, [this](AsyncWebServerRequest *request)
        {
            this->_sendRebootPage(request);
        });

    _server.on("/sc/reboot", HTTP_POST, [this](AsyncWebServerRequest *request)
        {
            request->send(200, "text/plain", "Rebooting...");
            MY_LOGD(SERVER_TAG, "Rebooting...");
            ESP.restart();
        });

    _server.on("/sc/status", HTTP_GET, [this](AsyncWebServerRequest *request)
        {
            if (this->_getCurrentStatus == nullptr) {
                request->send(200, "text/plain", "No status available");
                return;
            }
            request->send(200, "application/json", this->_getCurrentStatus());
        });

    /**
     * @brief Endpoint that gives general useful info about the device.
     *
     * @param[out] deviceName
     * @param[out] deviceType
     * @param[out] version
     * @param[out] freeHeap
     * @param[out] SSID
     * @param[out] RSSI
     * @param[out] status
     * @return json
     */
    _server.on("/sc/info", HTTP_GET, [this](AsyncWebServerRequest *request)
        {
            String status = "";
            if (this->_getCurrentStatus != nullptr) {
                status = this->_getCurrentStatus();
            }
            String res = "{";
            res += "\"app\":\"" + String(_deviceInfo && _deviceInfo->app ? _deviceInfo->app : "") + "\",";
            res += "\"version\":\"" + String(_deviceInfo->version) + "\",";
            res += "\"type\":\"" + String(_deviceInfo && _deviceInfo->type ? _deviceInfo->type : "") + "\",";
            res += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
            res += "\"ssid\":\"" + String(WiFi.SSID()) + "\",";
            res += "\"rssi\":" + String(WiFi.RSSI()) ;//-+ ",";
            res += "}";
            //- res += "\"status\":" + status + "}";
            request->send(200, "application/json", res);
        });

    _server.on("/sc/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request)
        {
            int16_t n = WiFi.scanComplete();
            if (n == WIFI_SCAN_FAILED) {
                MY_LOGD(SERVER_TAG, "Starting async WiFi scan...");
                WiFi.scanNetworks(true);
                request->send(202, "application/json", "[]");
                return;
            }
            if (n == WIFI_SCAN_RUNNING) {
                request->send(202, "application/json", "[]");
                return;
            }
            String json = "[";
            for (int i = 0; i < n; ++i) {
                if (i > 0) json += ",";
                json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
            }
            json += "]";
            WiFi.scanDelete();
            request->send(200, "application/json", json);
        });

    _server.on("/sc/wifi", HTTP_GET, [this](AsyncWebServerRequest *request)
        {
            this->_sendWifiPage(request);
        });

    _server.on("/sc/upload", HTTP_GET, [this](AsyncWebServerRequest *request)
        {
            this->_sendUploadPage(request);
        });

    _server.on("/sc/upload", HTTP_POST,
        [this](AsyncWebServerRequest *request) {
            bool error = Update.hasError();
            request->send(error ? 500 : 200, "text/plain", error ? "OTA Update Failed!" : "Update Successful! Rebooting...");
            if (!error) {
                delay(1000);
                ESP.restart();
            }
        },
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!index) {
                MY_LOGD(SERVER_TAG, "Update Start: %s", filename.c_str());
                // Use UPDATE_SIZE_UNKNOWN to tell the library we don't know the size yet
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                    Update.printError(Serial);
                }
            }
            if (!Update.hasError()) {
                if (Update.write(data, len) != len) {
                    Update.printError(Serial);
                }
            }
            if (final) {
                if (Update.end(true)) { // true = sets the size to the current index
                    MY_LOGD(SERVER_TAG, "Update Success: %u bytes", (uint32_t)(index + len));
                } else {
                    Update.printError(Serial);
                }
            }
        });

    _server.on("/sc/style.css", HTTP_GET, [this](AsyncWebServerRequest *request)
        {
            AsyncWebServerResponse *response = request->beginResponse(200, "text/css", FPSTR(COMMON_CSS));
            response->addHeader("Cache-Control", "max-age=3600");
            request->send(response);
        });

    // Register index last so specific /sc/... routes match first
    _server.on("/sc/", HTTP_GET, [this](AsyncWebServerRequest *request) { this->_handleRoot(request); });
    _server.on("/sc", HTTP_GET, [this](AsyncWebServerRequest *request) { this->_handleRoot(request); });

    // Serve the form for any URL
    _server.onNotFound([this](AsyncWebServerRequest *request)
        {
            //- this->_handleRoot(request);
            request->send(404, "text/plain", "404 Not Found");
        });
}




void WebserverManager::_handleRoot(AsyncWebServerRequest *request) {
    this->_sendPage(request);
}

void WebserverManager::_sendPage(AsyncWebServerRequest *request) {
    String html = FPSTR(PAGE_HTML_PART1);
    String title = _deviceInfo && _deviceInfo->app ? _deviceInfo->app : "";
    if (_deviceInfo && _deviceInfo->version) {
        title += " ";
        title += _deviceInfo->version;
    }
    html.replace("%APP_TITLE%", title);
    request->send(200, "text/html", html);
}


void WebserverManager::_sendRebootPage(AsyncWebServerRequest *request) {
    String html = FPSTR(REBOOT_HTML);
    String title = _deviceInfo && _deviceInfo->app ? _deviceInfo->app : "";
    if (_deviceInfo && _deviceInfo->version) {
        title += " ";
        title += _deviceInfo->version;
    }
    html.replace("%APP_TITLE%", title);
    request->send(200, "text/html", html);
}


void WebserverManager::_sendUploadPage(AsyncWebServerRequest *request) {
    String html = FPSTR(UPLOAD_HTML);
    String title = _deviceInfo && _deviceInfo->app ? _deviceInfo->app : "";
    if (_deviceInfo && _deviceInfo->version) {
        title += " ";
        title += _deviceInfo->version;
    }
    html.replace("%APP_TITLE%", title);
    request->send(200, "text/html", html);
}


void WebserverManager::_sendWifiPage(AsyncWebServerRequest *request) {
    String html = FPSTR(WIFI_HTML);
    String title = _deviceInfo && _deviceInfo->app ? _deviceInfo->app : "";
    if (_deviceInfo && _deviceInfo->version) {
        title += " ";
        title += _deviceInfo->version;
    }
    html.replace("%APP_TITLE%", title);
    request->send(200, "text/html", html);
}

void WebserverManager::_handleWifiConnect(AsyncWebServerRequest *request)
{
    if (_wifiManager == nullptr) {
        request->send(400, "application/json", "{\"error\": \"WiFi management not enabled\"}");
        return;
    }

    String ssid = "";
    String password = "";

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
        MY_LOGD(SERVER_TAG, "Received Wi-Fi credentials");

        _wifiManager->saveWifiCredentials(ssid, password);
        _wifiManager->init();
    } else {
        request->send(400, "text/plain", "Invalid SSID or Password");
    }
}


void WebserverManager::loop()
{
    if (!_started && WiFi.isConnected()) {
        _server.begin();
        _started = true;
        MY_LOGD(SERVER_TAG, "Webserver started");
    }
}
