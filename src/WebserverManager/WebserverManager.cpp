#include "WebserverManager.h"

#include "../Log/Log.h"


WebserverManager::WebserverManager(WifiManager *wifiManager, DeviceInfo_t *deviceInfo, std::function<String()> getCurrentStatus)
{
  _wifiManager = wifiManager;
  _getCurrentStatus = getCurrentStatus;
  _setupWebServer();
  _deviceInfo = deviceInfo;
  _server.begin();
}


void WebserverManager::_setupWebServer()
{
    // Connect to Wifi form.
    _server.on("/", HTTP_GET, [this]() { this->_handleRoot(); });

    // Connect to Wifi form submission.
    _server.on("/connect", HTTP_POST, [this]()
        {
            this->_handleWifiConnect();
        });

    _server.on("/disconnect", HTTP_GET, [this]()
        {
            _server.send(200, "text/plain", "Disconnecting from Wi-Fi...");
            MY_LOGD(SERVER_TAG, "Disconnecting from Wi-Fi...");
            WiFi.disconnect(true);
            //  _initAPMode();
        });

    _server.on("/reboot", HTTP_GET, [this]()
        {
            _server.send(200, "text/plain", "Rebooting...");
            MY_LOGD(SERVER_TAG, "Rebooting...");
            ESP.restart();
        });

    _server.on("/status", HTTP_GET, [this]()
        {
            if (this->_getCurrentStatus == nullptr) {
                _server.send(200, "text/plain", "No status available");
                return;
            }
            _server.send(200, "text/plain", this->_getCurrentStatus());
        });

    _server.on("/version", HTTP_GET, [this]()
        {
            _server.send(200, "text/plain", String(_deviceInfo->version));
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
    _server.on("/sys_info", HTTP_GET, [this]()
        {
            String status = "";
            if (this->_getCurrentStatus != nullptr) {
                status = this->_getCurrentStatus();
            }
            String res = "{";
            res += "\"deviceName\":\"" + String(_deviceInfo && _deviceInfo->name ? _deviceInfo->name : "") + "\",";
            res += "\"deviceType\":\"" + String(_deviceInfo && _deviceInfo->type ? _deviceInfo->type : "") + "\",";
            res += "\"version\":\"" + String(_deviceInfo->version) + "\",";
            res += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
            res += "\"SSID\":\"" + String(WiFi.SSID()) + "\",";
            res += "\"RSSI\":" + String(WiFi.RSSI()) + ",";
            res += "\"status\":" + status + "}";
            _server.send(200, "application/json", res);
        });

    _server.on("/scan", HTTP_GET, [this]()
        {
            MY_LOGD(SERVER_TAG, "Scanning Wi-Fi networks...");
            int n = WiFi.scanNetworks();

            String json = "[";
            for (int i = 0; i < n; ++i) {
                if (i > 0) json += ","; // comma between objects

                json += "{";
                json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
                json += "\"bssid\":\"" + WiFi.BSSIDstr(i) + "\",";
                json += "\"rssi\":" + String(WiFi.RSSI(i));
                json += "}";
            }
            json += "]";

            _server.send(200, "application/json", json);
        });

    _server.on("/upload", HTTP_POST,
        [this]() {
            if (Update.hasError()) {
                _server.send(500, "text/plain", "OTA Update Failed!");
            } else {
                _server.send(200, "text/plain", "Update Successful! Rebooting...");
                delay(1000);
                ESP.restart();
            }
        },
        // This handles the file upload in chunks, mimicking ESP8266HTTPUpdateServer logic (no auth)
        [this]() {
            HTTPUpload& upload = _server.upload();
            static bool updateError = false;
            if (upload.status == UPLOAD_FILE_START) {
                updateError = false;
                WiFiUDP::stopAll();
                Serial.printf("Update: %s\n", upload.filename.c_str());
                uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
                if (!Update.begin(maxSketchSpace, U_FLASH)) {
                    Update.printError(Serial);
                    updateError = true;
                }
            } else if (upload.status == UPLOAD_FILE_WRITE && !updateError) {
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Update.printError(Serial);
                    updateError = true;
                }
            } else if (upload.status == UPLOAD_FILE_END && !updateError) {
                if (Update.end(true)) {
                    Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
                } else {
                    Update.printError(Serial);
                    updateError = true;
                }
            } else if (upload.status == UPLOAD_FILE_ABORTED) {
                Update.end();
                Serial.println("Update was aborted");
            }
            // yield(); // keep WiFi alive
        });

    // Serve the form for any URL
    _server.onNotFound([this]()
        {
            this->_handleRoot();
        });
}


const char PAGE_HTML_PART1[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
    :root {--main-color: #1976d2;--main-color-hover:rgb(23, 103, 183);--background-color: #f2f2f2;--container-bg: #fff;--border-color: #ccc;--input-bg: #fff;--input-text: #555;--button-text: #fff;--popup-bg: rgba(0,0,0,0.5);--overlay-bg: rgba(255,255,255,0.9);}
    *{box-sizing:border-box;margin:0;padding:0}
    html,body{font-family:Arial,sans-serif;height:100%;width:100%;background:var(--background-color)}
    body{display:flex;justify-content:center;padding: 0 20px;}
    .container{width:100%;max-width:400px; margin: 40px 0;}
    nav{display:flex;flex-wrap:wrap;gap:8px;justify-content:center;margin:20px 0}
    nav a{flex:1;text-align:center;padding:10px;border:2px solid var(--main-color);color:var(--main-color);background:var(--container-bg);text-decoration:none;border-radius:6px;font-size:16px;cursor:pointer;transition:all 0.2s ease}
    nav a:hover{background:var(--main-color);color:var(--button-text)}
    a.disabled{pointer-events:none;opacity:0.6}
    #wifiActions{display:flex;flex-direction:row;gap:8px; margin: 8px 0;}
    #wifiActions a{text-align:center;padding:10px;border:2px solid var(--main-color);color:var(--main-color);background:var(--container-bg);text-decoration:none;border-radius:6px;font-size:16px;cursor:pointer;transition:all 0.2s ease}
    #wifiActions a:hover{background:var(--main-color);color:var(--button-text)}
    label{display:block;font-size:16px;font-weight:bold}
    input[type='text'],input[type='password']{width:100%;padding:12px;margin-bottom:16px;margin-top:5px;border:1px solid var(--border-color);border-radius:6px;font-size:16px;background:var(--input-bg);color:var(--input-text)}
    input[type='submit']{width:100%;padding:14px;background:var(--main-color);color:var(--button-text);border:none;border-radius:6px;font-size:18px;cursor:pointer;transition:all 0.2s ease}
    input[type='submit']:hover{background:var(--main-color-hover)}
    .toggle{display:flex;align-items:center;gap:8px;margin-bottom:16px}
    .overlay{position:fixed;top:0;left:0;width:100%;height:100%;background:var(--overlay-bg);display:flex;flex-direction:column;justify-content:center;align-items:center;font-size:20px;display:none;z-index:1000}
    .spinner{border:6px solid #f3f3f3;border-top:6px solid var(--main-color);border-radius:50%;width:50px;height:50px;animation:spin 1s linear infinite;margin-bottom:20px}
    @keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}
    #popup{position:fixed;top:0;left:0;width:100%;height:100%;background:var(--popup-bg);display:none;align-items:center;justify-content:center;z-index:2000}
    #popup .content{background:var(--container-bg);padding:20px;border-radius:10px;max-width:500px;width:95%;max-height:80%;overflow-y:auto;box-shadow:0 4px 12px rgba(0,0,0,0.2)}
    #popup h2{margin-bottom:12px;font-size:18px;color:#333}
    #networkList{list-style:none;padding:0;margin:0;max-height:350px;overflow-y:auto}
    #networkList li{padding:10px;border:1px solid #ddd;border-radius:6px;margin-bottom:8px;cursor:pointer;transition:all 0.2s}
    #networkList li:hover{background:#f0f0f0}
    #popup .content .button-container{text-align:right;margin-top:10px;}
    #popup button{padding:10px 20px;border:none;border-radius:6px;background:var(--main-color);color:var(--button-text);font-size:16px;cursor:pointer}
    #popup button:hover{background:var(--main-color-hover)}
    .device-name{font-size:24px;font-weight:bold;margin-bottom:8px;}
    .device-info{margin-bottom:8px;}
    #wifiForm, #fileForm {display: none;width: 100%;border-radius: 12px;background: var(--container-bg);box-shadow: 0 4px 12px rgba(0,0,0,0.1);height: 0;padding: 0 24px;opacity: 0;overflow: hidden;transition: height 0.4s ease, opacity 0.3s ease;}
    #wifiForm.show {height: 380px;opacity: 1;display: block;padding: 24px;}
    #fileForm.show {height: 300px;opacity: 1;display: block;padding: 24px;}
    .file-input-wrapper {margin-bottom: 16px;margin-top: 5px;position: relative;display: flex;align-items: center;gap: 10px;border: 1px solid var(--border-color);border-radius: 6px;padding: 10px;cursor: pointer;background: #f9f9f9;}
    .file-input-wrapper input[type="file"] {opacity: 0;position: absolute;left: 0;top: 0;height: 100%;width: 100%;cursor: pointer;}
    #fileName {font-size: 14px;color: var(--input-text);}
    #uploadProgressContainer {width: 100%;background: #eee;border-radius: 6px;margin-bottom: 16px;display: none;}
    #uploadProgressBar {width: 0%;height: 18px;background: var(--main-color);border-radius: 6px;transition: width 0.2s;}
    #uploadProgressText {text-align: right;font-size: 13px;color: #333;margin-bottom: 8px;display: none;}
    .highlightButton {background:var(--main-color);color:var(--button-text)}
</style>
<script>
    window.addEventListener('DOMContentLoaded', () => {
        const fileInput = document.getElementById('file');
        if (fileInput) {
            fileInput.addEventListener('change', function(){
                const fileName = this.files.length ? this.files[0].name : "No file chosen";
                document.getElementById('fileName').textContent = fileName;
            });
        }
        // Custom OTA upload handler
        const fileForm = document.getElementById('fileForm');
        if (fileForm) {
            fileForm.addEventListener('submit', function(e) {
                e.preventDefault();
                const fileInput = document.getElementById('file');
                if (!fileInput.files.length) return;
                const file = fileInput.files[0];
                const formData = new FormData();
                formData.append('file', file);
                // Show progress bar
                document.getElementById('uploadProgressContainer').style.display = 'block';
                document.getElementById('uploadProgressText').style.display = 'block';
                document.getElementById('uploadProgressBar').style.width = '0%';
                document.getElementById('uploadProgressText').textContent = '0%';
                // Hide overlay spinner
                document.getElementById('overlay').style.display = 'none';
                // Upload with progress
                var xhr = new XMLHttpRequest();
                xhr.open('POST', '/upload', true);
                xhr.upload.onprogress = function(e) {
                    if (e.lengthComputable) {
                        var percent = Math.round((e.loaded / e.total) * 100);
                        document.getElementById('uploadProgressBar').style.width = percent + '%';
                        document.getElementById('uploadProgressText').textContent = percent + '%';
                    }
                };
                xhr.onload = function() {
                    if (xhr.status === 200) {
                        document.getElementById('uploadProgressBar').style.width = '100%';
                        document.getElementById('uploadProgressText').textContent = 'Update Successful! Rebooting...';
                        setTimeout(function(){
                            document.getElementById('uploadProgressContainer').style.display = 'none';
                            document.getElementById('uploadProgressText').style.display = 'none';
							document.getElementById('overlay').style.display='flex';
							try {
								checkOnline();
							} catch (e) {
							 	console.log(e);
								checkOnline(); 
							}
                        }, 3000);
                    } else {
                        document.getElementById('uploadProgressText').textContent = 'OTA Update Failed!';
                    }
                };
                xhr.onerror = function() {
                    document.getElementById('uploadProgressText').textContent = 'Upload error!';
                };
                xhr.send(formData);
            });
        }
        // Fetch and update device info on page load
        updateDeviceInfo();
        // Attach updateDeviceInfo to Status button with disable/enable logic
        var statusBtn = document.getElementById('statusBtn');
        if (statusBtn) {
            statusBtn.addEventListener('click', function(e) {
                e.preventDefault();
                statusBtn.classList.add('disabled');
                statusBtn.classList.add('highlightButton');
                updateDeviceInfo(function() {
                    statusBtn.classList.remove('disabled');
                    statusBtn.classList.remove('highlightButton');
                });
            });
        }
    });

    // Fetch device info from /sys_info and update UI elements
    function updateDeviceInfo() {
        // Accepts optional callback to run after update
        var cb = arguments.length > 0 ? arguments[0] : null;
        fetch('/sys_info')
            .then(res => res.json())
            .then(data => {
                if (data.deviceName !== undefined) {
                    document.getElementById('dname').textContent = data.deviceName;
                }
                if (data.deviceType !== undefined) {
                    document.getElementById('dtype').textContent = 'Type: ' + data.deviceType;
                }
                if (data.version !== undefined) {
                    document.getElementById('dversion').textContent = 'Version: ' + data.version;
                }
                if (data.status !== undefined) {
                    document.getElementById('dstatus').textContent = 'Status: ' + JSON.stringify(data.status);
                }
                if (data.freeHeap !== undefined) {
                    document.getElementById('dheap').textContent = 'Free Heap: ' + data.freeHeap;
                }
                // Optionally update SSID/RSSI in WiFi form if present
                if (data.SSID !== undefined && document.getElementById('ssidLabel')) {
                    document.getElementById('ssidLabel').textContent = data.SSID;
                }
                if (data.RSSI !== undefined && document.getElementById('rssiLabel')) {
                    document.getElementById('rssiLabel').textContent = data.RSSI;
                }
                if (cb) cb();
            })
            .catch(err => {
                console.error('Failed to fetch device info:', err);
                if (cb) cb();
            });
    }
    function toggleForm(formId, btnId) {
        const forms = ["wifiForm", "fileForm"];
        const buttons = ["toggleWifiFormBtn", "toggleFileFormBtn"];

        forms.forEach(id => {
            const form = document.getElementById(id);
            if (id === formId) {
                form.classList.toggle("show");
            } else {
                form.classList.remove("show");
            }
        });

        buttons.forEach(id => {
            const btn = document.getElementById(id);
            if (id === btnId && document.getElementById(formId).classList.contains("show")) {
                btn.classList.add("highlightButton");
            } else {
                btn.classList.remove("highlightButton");
            }
        });
    }
    function togglePassword(){var p=document.getElementById('password');p.type=p.type==='password'?'text':'password';}
    function rebootAndWait(){document.getElementById('overlay').style.display='flex';fetch('/reboot').then(()=>{checkOnline();}).catch(()=>{checkOnline();});}
    function checkOnline(){fetch('/',{method:'HEAD'}).then(()=>{window.location.href='/';}).catch(()=>{setTimeout(checkOnline,2000);});}
    function scanNetworks(){
        document.getElementById('overlay').style.display='flex';
        fetch('/scan')
            .then(res=>res.json())
            .then(data=>{
                data.sort((a,b)=>b.rssi-a.rssi);
                document.getElementById('overlay').style.display='none';
                let list=document.getElementById('networkList');
                list.innerHTML='';
                data.forEach(net=>{
                    let li=document.createElement('li');
                    li.textContent=net.ssid+' (RSSI:'+net.rssi+')';
                    li.onclick=function(){document.getElementById('ssid').value=net.ssid;closePopup();};
                    list.appendChild(li);
                });
                document.getElementById('popup').style.display='flex';
            })
            .catch(err=>{document.getElementById('overlay').style.display='none';alert('Error: '+err);});
    }
    function closePopup(){document.getElementById('popup').style.display='none';}
</script>
</head>
<body>
    <div class='container'>
        <h1 id="dname" class='device-name'></h1>
        <p id="dtype" class='device-info'></p>
        <p id="dversion" class='device-info'></p>
        <p id="dstatus" class='device-info'></p>
        <p id="dheap" class='device-info'></p>
        <nav>
            <a id="statusBtn" href='#'>Status</a>
            <a id="rebootBtn" href='#' onclick='rebootAndWait()'>Reboot</a>
        </nav>
        <nav>
            <a id="toggleWifiFormBtn" href='#' onclick='toggleForm("wifiForm","toggleWifiFormBtn")'>Configure WiFi</a>
            <a id="toggleFileFormBtn" href='#' onclick='toggleForm("fileForm","toggleFileFormBtn")'>OTAUpdate</a>
        </nav>
        <form id="wifiForm" action='/connect' method='POST'>
			<span style="font-weight: bold;font-size: 16px">Connected to: </span><span id="ssidLabel"></span> (<span id="rssiLabel"></span>)
			<div id="wifiActions">
				<a href='#' onclick='scanNetworks()'>Scan</a>
				<a href='/disconnect'>Disconnect</a>
            </div>
            <label for='ssid'>SSID:</label>
            <input type='text' id='ssid' name='ssid' placeholder='Enter WiFi SSID'>
            <label for='password'>Password:</label>
            <input type='password' id='password' name='password' placeholder='Enter WiFi Password'>
            <div class='toggle'><input type='checkbox' id='show' onclick='togglePassword()'><label for='show'>Show Password</label></div>
            <input type='submit' value='Connect'>
        </form>
        <form id="fileForm" action='/upload' method='POST' enctype='multipart/form-data'>
            <label for='file'>Choose a file:</label>
            <div class="file-input-wrapper">
                <input type='file' id='file' name='file' accept=".bin">
                <span id="fileName">No file chosen</span>
            </div>
            <div id="uploadProgressText"></div>
            <div id="uploadProgressContainer">
                <div id="uploadProgressBar"></div>
            </div>
            <input type='submit' value='Upload'>
        </form>
    </div>
    <div id='overlay' class='overlay'>
        <div class='spinner'></div>
            <div>Working...</div>
    </div>
    <div id='popup'>
        <div class='content'>
            <h2>Available Networks</h2>
            <ul id='networkList'></ul>
            <div class='button-container'><button onclick='closePopup()'>Close</button></div>
        </div>
    </div>
</body>
</html>
)rawliteral";


void WebserverManager::_handleRoot() {
    String status = "";
    if (this->_getCurrentStatus != nullptr) {
        status = this->_getCurrentStatus();
    }

    _server.setContentLength(CONTENT_LENGTH_UNKNOWN); // Indicate chunked transfer.
    _server.send(200, "text/html", "");
    _server.sendContent_P(PAGE_HTML_PART1);
    _server.sendContent("");
}


void WebserverManager::_handleWifiConnect()
{
    String ssid = _server.arg("ssid");
    String password = _server.arg("password");
    ssid.trim();
    password.trim();

    if (ssid.length() > 0 && password.length() > 0) {
        _server.send(200, "text/plain", "Connecting to Wi-Fi...");
        MY_LOGD(SERVER_TAG, "Received Wi-Fi credentials");
        // MY_LOGD(SERVER_TAG, "SSID: " + ssid);
        // MY_LOGD(SERVER_TAG, "Password: " + password);

        _wifiManager->saveWifiCredentials(ssid, password);
        _wifiManager->init();
    } else {
        _server.send(400, "text/plain", "Invalid SSID or Password");
    }
}


void WebserverManager::loop()
{
    _server.handleClient();
}
