#pragma once
#include <Arduino.h>

const char UPLOAD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<link rel="stylesheet" href="/sc/style.css">
</head>
<body>
<div class='container'>
  <h1 class='device-name'>%APP_TITLE%</h1>
  <label for='file'>Choose a file:</label>
  <div class="file-input-wrapper">
    <input type='file' id='file' name='file' accept=".bin" onchange="onFileChange()">
    <span id="fileName">No file chosen</span>
  </div>
  <div id="progressText"></div>
  <button id="uploadBtn" onclick="doUpload()">Upload</button>
  <div id="spinnerContainer"><div class="spinner"></div></div>
</div>
<script>
  function onFileChange(){
    var f = document.getElementById('file');
    document.getElementById('fileName').textContent = f.files.length ? f.files[0].name : 'No file chosen';
  }
  function doUpload(){
    var fileInput = document.getElementById('file');
    if (!fileInput.files.length) return;
    fileInput.disabled = true;
    var btn = document.getElementById('uploadBtn');
    btn.disabled = true;
    var progressText = document.getElementById('progressText');
    progressText.style.display = 'block';
    progressText.textContent = '0%';
    var formData = new FormData();
    formData.append('file', fileInput.files[0]);
    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/sc/upload', true);
    xhr.upload.onprogress = function(e){
      if(e.lengthComputable){
        progressText.textContent = Math.round((e.loaded/e.total)*100) + '%';
      }
    };
    xhr.onload = function(){
      if(xhr.status === 200){
        progressText.textContent = 'Rebooting...';
        btn.style.display = 'none';
        document.getElementById('spinnerContainer').style.display = 'block';
        setTimeout(checkOnline, 3000);
      } else {
        progressText.textContent = 'Upload failed!';
        fileInput.disabled = false;
        btn.disabled = false;
      }
    };
    xhr.onerror = function(){
      progressText.textContent = 'Upload error!';
      fileInput.disabled = false;
      btn.disabled = false;
    };
    xhr.send(formData);
  }
  function checkOnline(){
    fetch('/sc/',{method:'HEAD'})
      .then(()=>{
        document.getElementById('spinnerContainer').style.display = 'none';
        document.getElementById('progressText').style.display = 'none';
        var btn = document.getElementById('uploadBtn');
        btn.style.display = 'block';
        btn.disabled = false;
        var fileInput = document.getElementById('file');
        fileInput.disabled = false;
        fileInput.value = '';
        document.getElementById('fileName').textContent = 'No file chosen';
      })
      .catch(()=>{setTimeout(checkOnline,3000);});
  }
</script>
</body>
</html>
)rawliteral";
