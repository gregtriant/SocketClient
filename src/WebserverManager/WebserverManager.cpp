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

    _server.on("/leave", HTTP_GET, [this]()
                {
                    _server.send(200, "text/plain", "Leaving Wi-Fi...");
                    MY_LOGD(SERVER_TAG, "Leaving Wi-Fi...");
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
    *{box-sizing:border-box;margin:0;padding:0}
    html,body{font-family:Arial,sans-serif;height:100%;width:100%;background:#f2f2f2}
    body{display:flex;justify-content:center;padding: 24px;}
    .container{width:100%;max-width:400px; margin-top: 40px;}
    nav{display:flex;flex-wrap:wrap;gap:8px;justify-content:center;margin:20px 0}
    nav a{flex:1;text-align:center;padding:10px;border:2px solid #4CAF50;color:#4CAF50;background:#fff;text-decoration:none;border-radius:6px;font-size:16px;cursor:pointer;transition:all 0.2s ease}
    nav a:hover{background:#4CAF50;color:#fff}
    label{display:block;font-size:16px;font-weight:bold}
    input[type='text'],input[type='password']{width:100%;padding:12px;margin-bottom:16px;margin-top:5px;border:1px solid #ccc;border-radius:6px;font-size:16px}
    input[type='submit']{width:100%;padding:14px;background:#4CAF50;color:#fff;border:none;border-radius:6px;font-size:18px;cursor:pointer;transition:all 0.2s ease}
    input[type='submit']:hover{background:#45a049}
    .toggle{display:flex;align-items:center;gap:8px;margin-bottom:16px}
    .overlay{position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(255,255,255,0.9);display:flex;flex-direction:column;justify-content:center;align-items:center;font-size:20px;display:none;z-index:1000}
    .spinner{border:6px solid #f3f3f3;border-top:6px solid #4CAF50;border-radius:50%;width:50px;height:50px;animation:spin 1s linear infinite;margin-bottom:20px}
    @keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}
    #popup{position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);display:none;align-items:center;justify-content:center;z-index:2000}
    #popup .content{background:#fff;padding:20px;border-radius:10px;max-width:500px;width:95%;max-height:80%;overflow-y:auto;box-shadow:0 4px 12px rgba(0,0,0,0.2)}
    #popup h2{margin-bottom:12px;font-size:18px;color:#333}
    #networkList{list-style:none;padding:0;margin:0;max-height:350px;overflow-y:auto}
    #networkList li{padding:10px;border:1px solid #ddd;border-radius:6px;margin-bottom:8px;cursor:pointer;transition:all 0.2s}
    #networkList li:hover{background:#f0f0f0}
    #popup .content .button-container{text-align:right;margin-top:10px;}
    #popup button{padding:10px 20px;border:none;border-radius:6px;background:#4CAF50;color:#fff;font-size:16px;cursor:pointer}
    #popup button:hover{background:#45a049}
    .device-name{font-size:24px;font-weight:bold;margin-bottom:8px;}
    .device-info{margin-bottom:8px;}
    
    #wifiForm, #fileForm {
        display: none;
        width: 100%;
        border-radius: 12px;
        background: #fff;
        box-shadow: 0 4px 12px rgba(0,0,0,0.1);
        height: 0;
        padding: 0 24px;
        opacity: 0;
        overflow: hidden;
        transition: height 0.4s ease, opacity 0.3s ease;
    }
    #wifiForm.show, #fileForm.show {
        height: 300px;
        opacity: 1;
        display: block;
        padding: 24px;
    }
    .file-input-wrapper {
        margin-bottom: 16px;
        margin-top: 5px;
        position: relative;
        display: flex;
        align-items: center;
        gap: 10px;
        border: 1px solid #ccc;
        border-radius: 6px;
        padding: 10px;
        cursor: pointer;
        background: #f9f9f9;
    }
    .file-input-wrapper input[type="file"] {
        opacity: 0;
        position: absolute;
        left: 0;
        top: 0;
        height: 100%;
        width: 100%;
        cursor: pointer;
    }
    #fileName {
        font-size: 14px;
        color: #555;
    }
    .highlightButton {background:#4CAF50;color:#fff}
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
    });
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
)rawliteral";


const char PAGE_HTML_PART2[] PROGMEM = R"rawliteral(
        <nav>
            <a href='/status'>Status</a>
            <a href='#' onclick='rebootAndWait()'>Reboot</a>
            <a href='#' onclick='scanNetworks()'>Scan</a>
            <a href='/leave'>Leave</a>
        </nav>
        <nav>
            <a id="toggleWifiFormBtn" href='#' onclick='toggleForm("wifiForm","toggleWifiFormBtn")'>Configure WiFi</a>
            <a id="toggleFileFormBtn" href='#' onclick='toggleForm("fileForm","toggleFileFormBtn")'>Update</a>
        </nav>
        <form id="wifiForm" action='/connect' method='POST'>
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
                <input type='file' id='file' name='file'>
                <span id="fileName">No file chosen</span>
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

    _server.setContentLength(CONTENT_LENGTH_UNKNOWN); // chunked transfer
    _server.send(200, "text/html", "");
    _server.sendContent_P(PAGE_HTML_PART1);  

    _server.sendContent("<h1 class='device-name'>");
    _server.sendContent(_deviceInfo->name);
    _server.sendContent("</h1>");

    _server.sendContent("<p class='device-info'>Type: ");
    _server.sendContent(_deviceInfo->type);
    _server.sendContent("</p>");

    _server.sendContent("<p class='device-info'>Version: ");
    _server.sendContent(String(_deviceInfo->version, 2));
    _server.sendContent("</p>");

    _server.sendContent("<p class='device-info'>Status: ");
    _server.sendContent(status);
    _server.sendContent("</p>");

    _server.sendContent("<p class='device-info'>Free heap: ");
    _server.sendContent(String(ESP.getFreeHeap()));
    _server.sendContent("</p>");

    // Send the rest of the HTML
    _server.sendContent_P(PAGE_HTML_PART2);

    _server.sendContent("");
    // _server.client().stop();
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
