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


const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<style>
*{box-sizing:border-box;margin:0;padding:0}
html,body{font-family:Arial,sans-serif;height:100%;width:100%;background:#f2f2f2}
body{display:flex;justify-content:center;align-items:center;padding:20px}
.container{width:100%;max-width:400px}
nav{display:flex;flex-wrap:wrap;gap:8px;justify-content:center;margin:20px 0}
nav a{flex:1;text-align:center;padding:10px;border:2px solid #4CAF50;color:#4CAF50;background:#fff;text-decoration:none;border-radius:6px;font-size:16px;cursor:pointer;transition:all 0.2s ease}
nav a:hover{background:#4CAF50;color:#fff}
form{width:100%;padding:24px;border-radius:12px;background:#fff;box-shadow:0 4px 12px rgba(0,0,0,0.1)}
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
</style>
<script>
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
<h1 class='device-name'>%DEVICE_NAME%</h1>
<p class='device-info'>Type: %DEVICE_TYPE%</p>
<p class='device-info'>Version: %DEVICE_VERSION%</p>
<p class='device-info'>Free heap: %FREE_HEAP%</p>
<nav>
<a href='/status'>Status</a>
<a href='#'>Update</a>
<a href='#' onclick='rebootAndWait()'>Reboot</a>
<a href='#' onclick='scanNetworks()'>Scan</a>
<a href='/leave'>Leave</a>
</nav>
<form action='/connect' method='POST'>
<label for='ssid'>SSID:</label>
<input type='text' id='ssid' name='ssid' placeholder='Enter WiFi SSID'>
<label for='password'>Password:</label>
<input type='password' id='password' name='password' placeholder='Enter WiFi Password'>
<div class='toggle'><input type='checkbox' id='show' onclick='togglePassword()'><label for='show'>Show Password</label></div>
<input type='submit' value='Connect'>
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
    // String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
    //     "<style>"
    //     "*{box-sizing:border-box;margin:0;padding:0}"
    //     "html,body{font-family:Arial,sans-serif;height:100%;width:100%;background:#f2f2f2}"
    //     "body{display:flex;justify-content:center;align-items:center;padding:20px}"
    //     ".container{width:100%;max-width:400px}"
    //     "h1.device-name{font-size:28px;margin-bottom:8px;color:#333}"
    //     "p.device-type, p.device-version{font-size:16px;color:#555;margin:4px 0}"
    //     "nav{display:flex;flex-wrap:wrap;gap:8px;justify-content:center;margin:20px 0}"
    //     "nav a{flex:1;text-align:center;padding:10px;border:2px solid #4CAF50;color:#4CAF50;background:#fff;text-decoration:none;border-radius:6px;font-size:16px;cursor:pointer;transition:all 0.2s ease}"
    //     "nav a:hover{background:#4CAF50;color:#fff}"
    //     "form{width:100%;padding:24px;border-radius:12px;background:#fff;box-shadow:0 4px 12px rgba(0,0,0,0.1)}"
    //     "label{display:block;font-size:16px;font-weight:bold}"
    //     "input[type='text'],input[type='password']{width:100%;padding:12px;margin-bottom:16px;margin-top:5px;border:1px solid #ccc;border-radius:6px;font-size:16px}"
    //     "input[type='submit']{width:100%;padding:14px;background:#4CAF50;color:#fff;border:none;border-radius:6px;font-size:18px;cursor:pointer;transition:all 0.2s ease}"
    //     "input[type='submit']:hover{background:#45a049}"
    //     ".toggle{display:flex;align-items:center;gap:8px;margin-bottom:16px}"
    //     ".overlay{position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(255,255,255,0.9);display:flex;flex-direction:column;justify-content:center;align-items:center;font-size:20px;display:none;z-index:1000}"
    //     ".spinner{border:6px solid #f3f3f3;border-top:6px solid #4CAF50;border-radius:50%;width:50px;height:50px;animation:spin 1s linear infinite;margin-bottom:20px}"
    //     "@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}"
    //     "@media(max-height:500px){body{align-items:flex-start;padding-top:40px}}"
    //     /* Popup styles */
    //     "#popup{position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);display:none;align-items:center;justify-content:center;z-index:2000}"
    //     "#popup .content{background:#fff;padding:20px;border-radius:10px;max-width:500px;width:95%;max-height:80%;overflow-y:auto;box-shadow:0 4px 12px rgba(0,0,0,0.2)}"
    //     "#popup h2{margin-bottom:12px;font-size:18px;color:#333}"
    //     "#networkList{list-style:none;padding:0;margin:0;max-height:350px;overflow-y:auto}"
    //     "#networkList li{padding:10px;border:1px solid #ddd;border-radius:6px;margin-bottom:8px;cursor:pointer;transition:all 0.2s}"
    //     "#networkList li:hover{background:#f0f0f0}"
    //     "#popup .content .button-container{text-align:right;margin-top:10px;}"
    //     "#popup button{padding:10px 20px;border:none;border-radius:6px;background:#4CAF50;color:#fff;font-size:16px;cursor:pointer}"
    //     "#popup button:hover{background:#45a049}"
    //     "</style>"
    //     "<script>"
    //     "function togglePassword(){var p=document.getElementById('password');p.type=p.type==='password'?'text':'password';}"
    //     "function rebootAndWait(){document.getElementById('overlay').style.display='flex';fetch('/reboot').then(()=>{checkOnline();}).catch(()=>{checkOnline();});}"
    //     "function checkOnline(){fetch('/',{method:'HEAD'}).then(()=>{window.location.href='/';}).catch(()=>{setTimeout(checkOnline,2000);});}"
    //     "function scanNetworks(){"
    //     "document.getElementById('overlay').style.display='flex';"
    //     "fetch('/scan').then(res=>res.json()).then(data=>{"
    //     "data.sort((a,b)=>b.rssi-a.rssi);"
    //     "document.getElementById('overlay').style.display='none';"
    //     "let list=document.getElementById('networkList');list.innerHTML='';"
    //     "data.forEach(net=>{let li=document.createElement('li');li.textContent=net.ssid+' (RSSI:'+net.rssi+')';li.onclick=function(){document.getElementById('ssid').value=net.ssid;closePopup();};list.appendChild(li);});"
    //     "document.getElementById('popup').style.display='flex';"
    //     "}).catch(err=>{document.getElementById('overlay').style.display='none';alert('Error while scanning: '+err);});"
    //     "}"
    //     "function closePopup(){document.getElementById('popup').style.display='none';}"
    //     "</script>"
    //     "</head><body>"
    //     "<div class='container'>";

    // // --- Device info at the top ---
    // html += "<h1 class='device-name'>" + String(_deviceInfo->name) + "</h1>";
    // html += "<p class='device-type'>Type: " + String(_deviceInfo->type) + "</p>";
    // html += "<p class='device-version'>Version: " + String(_deviceInfo->version,2) + "</p>";
    // html += "<p class='device-version'>Free heap: " + String(ESP.getFreeHeap()) + "</p>";

    // html += "<nav>"
    //             "<a href='/status'>Status</a>"
    //             "<a href='#'>Update</a>"
    //             "<a href='#' onclick='rebootAndWait()'>Reboot</a>"
    //             "<a href='#' onclick='scanNetworks()'>Scan</a>"
    //             "<a href='/leave'>Leave</a>"
    //         "</nav>"
    //         "<form action='/connect' method='POST'>"
    //         "<label for='ssid'>SSID:</label>"
    //         "<input type='text' id='ssid' name='ssid' placeholder='Enter WiFi SSID'>"
    //         "<label for='password'>Password:</label>"
    //         "<input type='password' id='password' name='password' placeholder='Enter WiFi Password'>"
    //         "<div class='toggle'><input type='checkbox' id='show' onclick='togglePassword()'><label for='show'>Show Password</label></div>"
    //         "<input type='submit' value='Connect'>"
    //         "</form>"
    //         "</div>" // container
    //         "<div id='overlay' class='overlay'><div class='spinner'></div><div>Working...</div></div>"
    //         "<div id='popup'><div class='content'><h2>Available Networks</h2><ul id='networkList'></ul><div class='button-container'><button onclick='closePopup()'>Close</button></div></div></div>"
    //         "</body></html>";
    String page = FPSTR(PAGE_HTML);
    page.replace("%DEVICE_NAME%", String(_deviceInfo->name));
    page.replace("%DEVICE_TYPE%", String(_deviceInfo->type));
    page.replace("%DEVICE_VERSION%", String(_deviceInfo->version, 2));
    page.replace("%FREE_HEAP%", String(ESP.getFreeHeap()));

    _server.send(200, "text/html", page);
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
