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
  <div id='info'></div>
  <pre id='status' style='margin:16px 0'></pre>
  
</div>
<script>
  fetch('/sc/info').then(r=>r.json()).then(d=>{
    var html='';
    Object.entries(d).forEach(function(e){
      if(e[0]==='app'||e[0]==='version')return;
      html+='<div class="info-row"><b>'+e[0]+':</b> '+e[1]+'</div>';
    });
    document.getElementById('info').innerHTML=html;
  }).catch(function(){});
  fetch('/sc/status').then(r=>r.text()).then(function(t){
    try{t=JSON.stringify(JSON.parse(t),null,2);}catch(e){}
    document.getElementById('status').textContent=t;
  }).catch(function(){});
</script>
</body>
</html>
)rawliteral";
