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
  <div id="spinnerContainer">
    <div class="spinner"></div>
    <p id="uploadMsg" style="text-align:center;margin-top:12px;font-size:16px"></p>
  </div>
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
    btn.style.display = 'none';
    var spinner = document.getElementById('spinnerContainer');
    var uploadMsg = document.getElementById('uploadMsg');
    var progressText = document.getElementById('progressText');
    spinner.style.display = 'block';
    uploadMsg.textContent = 'Uploading... 0%';
    var formData = new FormData();
    formData.append('file', fileInput.files[0]);
    var xhr = new XMLHttpRequest();
    xhr.open('POST', '/sc/upload', true);
    xhr.upload.onprogress = function(e){
      if(e.lengthComputable){
        uploadMsg.textContent = 'Uploading... ' + Math.round((e.loaded/e.total)*100) + '%';
      }
    };
    xhr.onload = function(){
      if(xhr.status === 200){
        uploadMsg.textContent = 'Rebooting...';
        setTimeout(checkOnline, 3000);
      } else {
        spinner.style.display = 'none';
        progressText.textContent = 'Upload failed!';
        progressText.style.display = 'block';
        fileInput.disabled = false;
        btn.style.display = 'block';
      }
    };
    xhr.onerror = function(){
      spinner.style.display = 'none';
      progressText.textContent = 'Upload error!';
      progressText.style.display = 'block';
      fileInput.disabled = false;
      btn.style.display = 'block';
    };
    xhr.send(formData);
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
