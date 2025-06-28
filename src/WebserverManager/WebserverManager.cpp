#include "WebserverManager.h"

WebserverManager::WebserverManager(WifiManager *wifiManager)
{
  _wifiManager = wifiManager;

}


void WebserverManager::_setupWebServer()
{
  // Serve a simple HTML form
  _server.on("/", HTTP_GET, [this]()
             { this->_handleRoot(); });

  // Handle form submission
  _server.on("/connect", HTTP_POST, [this]()
             { this->_handleWifiConnect(); });

  _server.on("/leave-wifi", HTTP_GET, [this]()
             {
               _server.send(200, "text/plain", "Leaving Wi-Fi...");
               Serial.println("Leaving Wi-Fi...");
               WiFi.disconnect(true);
              //  _initAPMode(); 
            });
  _server.on("/restart", HTTP_GET, [this]()
             {
               _server.send(200, "text/plain", "Restarting...");
               Serial.println("Restarting...");
               ESP.restart(); });

  _server.on("/status", HTTP_GET, [this]()
             { _server.send(200, "text/plain", "Status: OK"); });

  _server.on("/scan-wifi", HTTP_GET, [this]()
             {
               _server.send(200, "text/plain", "Scanning Wi-Fi networks...");
               Serial.println("Scanning Wi-Fi networks...");
               int n = WiFi.scanNetworks();
               String networks = "Found " + String(n) + " networks:\n";
               for (int i = 0; i < n; ++i)
               {
                 networks += WiFi.SSID(i) + "\n";
               }
               _server.send(200, "text/plain", networks); });
              
  // Serve the form for any URL
  // _server.onNotFound([this]()
  //                    { this->_handleRoot(); });
}


void WebserverManager::_handleRoot()
{
  _server.send(200, "text/html",
               "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
               "<style>*{box-sizing:border-box;margin:0;padding:0}html,body{font-family:Arial,sans-serif;height:100%;width:100%;background:#f2f2f2}body{display:flex;justify-content:center;align-items:center;padding:20px}form{width:100%;max-width:400px;padding:24px;border-radius:12px;box-shadow:0 4px 12px rgba(0,0,0,0.1)}label{display:block;font-size:16px;font-weight:bold}input[type='text'],input[type='password']{width:100%;padding:12px;margin-bottom:16px;margin-top:5px;border:1px solid #ccc;border-radius:6px;font-size:16px}input[type='submit']{width:100%;padding:14px;background:#4CAF50;color:#fff;border:none;border-radius:6px;font-size:18px;cursor:pointer}input[type='submit']:hover{background:#45a049}.toggle{display:flex;align-items:center;gap:8px;margin-bottom:16px}@media(max-height:500px){body{align-items:flex-start;padding-top:40px}}</style>"
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

void WebserverManager::_handleWifiConnect()
{
  String ssid = _server.arg("ssid");
  String password = _server.arg("password");
  ssid.trim();
  password.trim();

  if (ssid.length() > 0 && password.length() > 0)
  {
    _server.send(200, "text/plain", "Connecting to Wi-Fi...");
    Serial.println("Received Wi-Fi credentials:");
    Serial.println("SSID: " + ssid);
    Serial.println("Password: " + password);

    _wifiManager->saveWifiCredentials(ssid, password);
    _wifiManager->init();
  }
  else
  {
    _server.send(400, "text/plain", "Invalid SSID or Password");
  }
}

