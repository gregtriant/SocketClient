#pragma once
#include <Arduino.h>

const char REBOOT_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<link rel="stylesheet" href="/sc/style.css">
</head>
<body>
<div class='container'>
  <h1 class='device-name'>%APP_TITLE%</h1>
  <button id="rebootBtn" onclick="doReboot()">Reboot Device</button>
  <div id="spinnerContainer">
    <div class="spinner"></div>
    <p id="rebootMsg" style="text-align:center;margin-top:12px;font-size:16px">Rebooting...</p>
  </div>
</div>
<script>
  function doReboot(){
    document.getElementById('rebootBtn').style.display = 'none';
    document.getElementById('spinnerContainer').style.display = 'block';
    fetch('/sc/reboot',{method:'POST'}).catch(()=>{});
    setTimeout(checkOnline, 3000);
  }
  function checkOnline(){
    fetch('/sc/',{method:'HEAD'})
      .then(()=>{ window.location.href='/sc/'; })
      .catch(()=>{setTimeout(checkOnline,3000);});
  }
</script>
</body>
</html>
)rawliteral";
