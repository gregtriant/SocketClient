#pragma once
#include <Arduino.h>

const char COMMON_CSS[] PROGMEM = R"rawliteral(
:root{--main-color:#1976d2;--main-color-hover:rgb(23,103,183);--background-color:#f2f2f2;--container-bg:#fff;--border-color:#ccc;--input-bg:#fff;--input-text:#555;--button-text:#fff;}
*{box-sizing:border-box;margin:0;padding:0}
html,body{font-family:Arial,sans-serif;height:100%;background:var(--background-color)}
body{display:flex;justify-content:center;padding:0 20px}
.container{width:100%;max-width:400px;margin:40px 0}
.device-name{font-size:24px;font-weight:bold;margin-bottom:24px}
nav{display:flex;flex-direction:column;gap:8px}
nav a{text-align:center;padding:12px;border:2px solid var(--main-color);color:var(--main-color);background:var(--container-bg);text-decoration:none;border-radius:6px;font-size:16px;transition:all 0.2s ease}
nav a:hover{background:var(--main-color);color:var(--button-text)}
label{display:block;font-size:16px;font-weight:bold}
input[type='text'],input[type='password']{width:100%;padding:12px;margin-bottom:16px;margin-top:5px;border:1px solid var(--border-color);border-radius:6px;font-size:16px;background:var(--input-bg);color:var(--input-text)}
.toggle{display:flex;align-items:center;gap:8px;margin-bottom:16px}
button{width:100%;padding:12px;background:var(--main-color);color:var(--button-text);border:none;border-radius:6px;font-size:16px;cursor:pointer;transition:all 0.2s ease;margin-bottom:8px;}
button:hover:not(:disabled){background:var(--main-color-hover)}
button:disabled{opacity:0.5;cursor:not-allowed}
.btn-outline{background:var(--container-bg);color:var(--main-color);border:2px solid var(--main-color);}
.btn-outline:hover:not(:disabled){background:var(--main-color);color:var(--button-text);}
.file-input-wrapper{margin-bottom:16px;margin-top:5px;position:relative;display:flex;align-items:center;gap:10px;border:1px solid var(--border-color);border-radius:6px;padding:10px;cursor:pointer;background:#f9f9f9;}
.file-input-wrapper input[type="file"]{opacity:0;position:absolute;left:0;top:0;height:100%;width:100%;cursor:pointer;}
#fileName{font-size:14px;color:var(--input-text);}
#progressText{font-size:16px;color:#333;margin-bottom:12px;display:none;}
#statusMsg{font-size:14px;color:#555;margin-bottom:16px;min-height:20px;}
#networkList{list-style:none;padding:0;margin:0;margin-top:8px;}
#networkList li{padding:10px;border:1px solid #ddd;border-radius:6px;margin-bottom:8px;cursor:pointer;background:var(--container-bg);transition:background 0.2s;}
#networkList li:hover{background:#f0f0f0}
.spinner{border:6px solid #f3f3f3;border-top:6px solid var(--main-color);border-radius:50%;width:50px;height:50px;animation:spin 1s linear infinite;margin:0 auto}
@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}
#spinnerContainer{display:none;padding:16px 0}
)rawliteral";
