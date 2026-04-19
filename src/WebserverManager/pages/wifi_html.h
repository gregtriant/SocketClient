#pragma once
#include <Arduino.h>

const char WIFI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<link rel="stylesheet" href="/sc/style.css">
</head>
<body>
<div class='container'>
  <h1 class='device-name'>%APP_TITLE%</h1>
  <p id='statusMsg'></p>
  <label for='ssid'>SSID:</label>
  <input type='text' id='ssid' placeholder='Enter WiFi SSID'>
  <label for='password'>Password:</label>
  <input type='password' id='password' placeholder='Enter WiFi Password'>
  <div class='toggle'><input type='checkbox' id='show' onclick='togglePassword()'><label for='show'>Show Password</label></div>
  <button id='connectBtn' onclick='doConnect()'>Connect</button>
  <button class='btn-outline' id='scanBtn' onclick='doScan()'>Scan Networks</button>
  <ul id='networkList'></ul>
</div>
<script>
  fetch('/sc/info').then(r=>r.json()).then(d=>{
    if(d.ssid) document.getElementById('statusMsg').textContent='Connected to: '+d.ssid+' ('+d.rssi+' dBm)';
  }).catch(()=>{});
  function togglePassword(){var p=document.getElementById('password');p.type=p.type==='password'?'text':'password';}
  function doConnect(){
    var ssid=document.getElementById('ssid').value.trim();
    var pass=document.getElementById('password').value.trim();
    if(!ssid)return;
    document.getElementById('connectBtn').disabled=true;
    document.getElementById('statusMsg').textContent='Connecting...';
    var body='ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(pass);
    fetch('/sc/wifi/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
      .then(r=>r.text()).then(t=>{
        document.getElementById('statusMsg').textContent=t;
        document.getElementById('connectBtn').disabled=false;
      }).catch(()=>{
        document.getElementById('statusMsg').textContent='Error sending request.';
        document.getElementById('connectBtn').disabled=false;
      });
  }
  function doDisconnect(){
    fetch('/sc/wifi/disconnect').then(r=>r.text()).then(t=>{
      document.getElementById('statusMsg').textContent=t;
    }).catch(()=>{});
  }
  function doScan(){
    var btn=document.getElementById('scanBtn');
    btn.disabled=true;
    btn.textContent='Scanning...';
    document.getElementById('networkList').innerHTML='';
    pollScan();
  }
  function pollScan(){
    fetch('/sc/wifi/scan').then(r=>{
      if(r.status===202){setTimeout(pollScan,1500);return null;}
      return r.json();
    }).then(data=>{
      if(!data)return;
      var btn=document.getElementById('scanBtn');
      btn.disabled=false;
      btn.textContent='Scan Networks';
      data.sort((a,b)=>b.rssi-a.rssi);
      var list=document.getElementById('networkList');
      data.forEach(net=>{
        var li=document.createElement('li');
        li.textContent=net.ssid+' ('+net.rssi+' dBm)';
        li.onclick=function(){document.getElementById('ssid').value=net.ssid;};
        list.appendChild(li);
      });
    }).catch(()=>{
      var btn=document.getElementById('scanBtn');
      btn.disabled=false;
      btn.textContent='Scan Networks';
    });
  }
</script>
</body>
</html>
)rawliteral";
