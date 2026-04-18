#pragma once
#include <Arduino.h>

const char PAGE_HTML_PART1[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<link rel="stylesheet" href="/sc/style.css">
</head>
<body>
<div class='container'>
  <h1 class='device-name'>%APP_TITLE%</h1>
  <nav>
    <a href='/sc/reboot'>Reboot</a>
    <a href='/sc/wifi'>Configure WiFi</a>
    <a href='/sc/upload'>OTA Update</a>
  </nav>
</div>
</body>
</html>
)rawliteral";
