#ifndef WEBREMOTE_H
#define WEBREMOTE_H

#include <Arduino.h>

const char WEB_REMOTE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>ATS-Mini Web Remote</title>
<style>
:root{
  --bg:#090d14;--card:#121824;--card2:#182234;--border:rgba(255,255,255,0.08);
  --cyan:#00d2ff;--amber:#f59e0b;--green:#10b981;--red:#ef4444;--purple:#a855f7;
  --text:#f1f5f9;--sub:#94a3b8;
}
*{box-sizing:border-box;margin:0;padding:0;user-select:none;-webkit-tap-highlight-color:transparent}
body{background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;font-size:14px;padding:8px;max-width:540px;margin:0 auto}
header{display:flex;justify-content:space-between;align-items:center;padding:8px 12px;background:var(--card);border:1px solid var(--border);border-radius:12px;margin-bottom:8px}
.title-box{display:flex;align-items:center;gap:8px}
.led{width:10px;height:10px;border-radius:50%;background:var(--green);box-shadow:0 0 8px var(--green);animation:pulse 2s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.4}}
.title{font-weight:700;font-size:15px;letter-spacing:1px;color:var(--cyan)}
.header-right{display:flex;align-items:center;gap:10px;font-size:12px;color:var(--sub)}
.btn-cfg{background:var(--card2);border:1px solid var(--border);color:var(--text);padding:4px 8px;border-radius:6px;text-decoration:none;font-size:12px}
.vfo-card{background:linear-gradient(180deg,#0a0f1d,#0d1424);border:1px solid rgba(0,210,255,0.25);border-radius:14px;padding:12px;box-shadow:0 8px 24px rgba(0,0,0,0.6);margin-bottom:8px}
.vfo-top{display:flex;justify-content:space-between;align-items:center;margin-bottom:4px}
.badge{padding:2px 8px;border-radius:6px;font-weight:700;font-size:11px;text-transform:uppercase;letter-spacing:0.5px}
.badge-band{background:#1e293b;color:var(--cyan);border:1px solid rgba(0,210,255,0.3)}
.badge-mode{background:var(--amber);color:#000}
.badge-mode.FM{background:var(--cyan);color:#000}
.badge-mode.AM{background:var(--amber);color:#000}
.badge-mode.USB{background:var(--green);color:#000}
.badge-mode.LSB{background:var(--purple);color:#fff}
.freq-display{font-family:ui-monospace,"SFMono-Regular",Consolas,monospace;text-align:center;font-size:38px;font-weight:800;color:#fff;text-shadow:0 0 12px rgba(0,210,255,0.5);margin:6px 0;letter-spacing:1px}
.freq-unit{font-size:16px;color:var(--cyan);margin-left:4px;font-weight:600}
.station-info{min-height:20px;text-align:center;font-size:13px;color:var(--amber);overflow:hidden;text-overflow:ellipsis;white-space:nowrap;margin-bottom:6px}
.smeter-box{margin-top:6px;padding:6px;background:rgba(0,0,0,0.4);border-radius:8px;border:1px solid var(--border)}
.smeter-bar{display:flex;gap:2px;height:8px;margin-bottom:4px}
.s-seg{flex:1;background:#1e293b;border-radius:1px;transition:background 0.1s}
.s-seg.on-g{background:var(--green);box-shadow:0 0 4px var(--green)}
.s-seg.on-a{background:var(--amber);box-shadow:0 0 4px var(--amber)}
.s-seg.on-r{background:var(--red);box-shadow:0 0 4px var(--red)}
.smeter-labels{display:flex;justify-content:space-between;font-size:9px;color:var(--sub);font-family:monospace}
.smeter-readout{display:flex;justify-content:space-between;font-size:11px;color:var(--sub);margin-top:3px}

/* DXing Suite Styles */
.dx-tabs{display:flex;gap:4px;margin-bottom:8px;overflow-x:auto;scrollbar-width:none}
.dx-tabs::-webkit-scrollbar{display:none}
.dx-tab{flex:1;min-width:70px;padding:7px 4px;font-size:11px;font-weight:700;border-radius:6px;background:var(--card2);border:1px solid var(--border);color:var(--sub);cursor:pointer;text-align:center;transition:all 0.15s}
.dx-tab.active{background:rgba(0,210,255,0.25);color:var(--cyan);border-color:var(--cyan);box-shadow:0 0 8px rgba(0,210,255,0.3)}
.dx-panel{display:none;background:rgba(15,23,42,0.6);border-radius:8px;padding:8px;border:1px solid rgba(255,255,255,0.06)}
.dx-panel.active{display:block}
.dx-chip{display:inline-flex;align-items:center;justify-content:center;background:var(--card2);border:1px solid var(--border);color:#e2e8f0;padding:6px 10px;border-radius:6px;font-size:11px;font-family:monospace;cursor:pointer;transition:all 0.15s;margin:2px}
.dx-chip:hover,.dx-chip:active{background:rgba(0,210,255,0.2);border-color:var(--cyan);color:#fff}
.dx-sec-title{font-size:11px;font-weight:700;color:var(--amber);margin:8px 0 4px;text-transform:uppercase;letter-spacing:0.5px}
.dx-log-table{width:100%;border-collapse:collapse;font-size:11px;margin-top:6px}
.dx-log-table th{background:#0f172a;color:var(--sub);padding:5px 4px;text-align:left;border-bottom:1px solid var(--border)}
.dx-log-table td{padding:5px 4px;border-bottom:1px solid rgba(255,255,255,0.05);color:var(--text)}
.dx-weather-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:6px;margin-bottom:8px}
.dx-weather-box{background:var(--card2);border:1px solid var(--border);border-radius:8px;padding:8px;text-align:center}
.dx-weather-val{font-size:18px;font-weight:800;color:var(--cyan);font-family:monospace}
.dx-weather-lbl{font-size:10px;color:var(--sub);margin-top:2px}

.card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:10px;margin-bottom:8px}
.grid-2{display:grid;grid-template-columns:1fr 1fr;gap:6px}
.grid-4{display:grid;grid-template-columns:repeat(4,1fr);gap:6px}
.grid-3{display:grid;grid-template-columns:repeat(3,1fr);gap:6px}
button{background:var(--card2);border:1px solid var(--border);color:var(--text);padding:10px;border-radius:8px;font-weight:600;font-size:13px;cursor:pointer;display:flex;align-items:center;justify-content:center;transition:all 0.1s}
button:active{transform:scale(0.96);background:rgba(0,210,255,0.2)}
button.active{background:var(--cyan);color:#000;border-color:var(--cyan);box-shadow:0 0 8px rgba(0,210,255,0.4)}
.btn-tune{font-size:15px;padding:12px 6px;font-family:monospace}
.btn-accent{background:linear-gradient(135deg,#0284c7,#06b6d4);color:#fff;border:none}
.pill-row{display:flex;gap:4px;overflow-x:auto;padding-bottom:4px;scrollbar-width:none}
.pill-row::-webkit-scrollbar{display:none}
.pill{flex:1;min-width:48px;padding:6px 4px;font-size:11px;border-radius:6px;background:var(--card2);border:1px solid var(--border);text-align:center;color:var(--sub);cursor:pointer}
.pill.active{background:rgba(0,210,255,0.2);color:var(--cyan);border-color:var(--cyan);font-weight:700}
.slider-row{display:flex;align-items:center;gap:10px;margin:6px 0}
.slider-label{min-width:60px;font-size:12px;color:var(--sub)}
.slider{flex:1;-webkit-appearance:none;height:6px;border-radius:3px;background:#1e293b;outline:none}
.slider::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;border-radius:50%;background:var(--cyan);cursor:pointer;box-shadow:0 0 6px var(--cyan)}
.val-badge{min-width:40px;text-align:right;font-size:12px;font-weight:700;color:var(--cyan);font-family:monospace}
.keypad-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;margin-top:8px}
.key-btn{padding:12px;font-size:16px;font-weight:700;font-family:monospace}
.nav-tabs{display:flex;gap:4px;margin-bottom:8px;background:var(--card);padding:4px;border-radius:10px;border:1px solid var(--border)}
.nav-tab{flex:1;padding:8px 4px;text-align:center;font-size:12px;font-weight:600;border-radius:6px;cursor:pointer;color:var(--sub)}
.nav-tab.active{background:var(--card2);color:var(--cyan);border:1px solid rgba(0,210,255,0.3)}
.tab-content{display:none}
.tab-content.active{display:block}
.band-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;max-height:240px;overflow-y:auto;padding:2px}
.band-btn{padding:8px 4px;font-size:11px;text-align:center;display:flex;flex-direction:column;gap:2px}
.band-btn span{font-size:9px;color:var(--sub)}
.mem-list{max-height:280px;overflow-y:auto;display:flex;flex-direction:column;gap:4px}
.mem-item{display:flex;justify-content:space-between;align-items:center;background:var(--card2);padding:6px 10px;border-radius:8px;border:1px solid var(--border);font-size:12px}
.mem-num{color:var(--cyan);font-weight:700;min-width:24px;font-family:monospace}
.mem-freq{font-family:monospace;font-weight:600}
.mem-actions{display:flex;gap:4px}
.mem-actions button{padding:4px 8px;font-size:11px}
.wifi-card{background:var(--card2);border:1px solid var(--border);border-radius:8px;padding:8px;margin-bottom:6px}
input[type=text],input[type=password]{width:100%;padding:8px;background:#0d1424;border:1px solid var(--border);border-radius:6px;color:#fff;font-size:13px;margin:4px 0}
.toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:rgba(0,0,0,0.9);color:var(--cyan);border:1px solid var(--cyan);padding:8px 16px;border-radius:20px;font-size:12px;display:none;z-index:999}
</style>
</head>
<body>
<header>
  <div class="title-box">
    <div class="led" id="led"></div>
    <span class="title">ATS-MINI</span>
  </div>
  <div class="header-right">
    <span id="conn-info">AP 192.168.4.1</span>
    <span id="bat-info">🔋 --%</span>
    <a href="/config" class="btn-cfg">⚙ Config</a>
  </div>
</header>

<div class="vfo-card">
  <div class="vfo-top">
    <span class="badge badge-band" id="vfo-band">--</span>
    <span class="badge badge-mode" id="vfo-mode">--</span>
    <span class="badge" style="background:#1e293b;color:var(--sub)" id="vfo-step">--</span>
    <span class="badge" style="background:#1e293b;color:var(--sub)" id="vfo-bw">--</span>
  </div>
  <div class="freq-display"><span id="vfo-freq">------</span><span class="freq-unit" id="vfo-unit">kHz</span></div>
  <div class="station-info" id="vfo-station">Scanning...</div>
  <div class="smeter-box">
    <div class="smeter-bar" id="smeter-bar">
      <div class="s-seg"></div><div class="s-seg"></div><div class="s-seg"></div><div class="s-seg"></div>
      <div class="s-seg"></div><div class="s-seg"></div><div class="s-seg"></div><div class="s-seg"></div>
      <div class="s-seg"></div><div class="s-seg"></div><div class="s-seg"></div><div class="s-seg"></div>
      <div class="s-seg"></div><div class="s-seg"></div><div class="s-seg"></div><div class="s-seg"></div>
    </div>
    <div class="smeter-labels">
      <span>S1</span><span>S3</span><span>S5</span><span>S7</span><span>S9</span><span>+20</span><span>+40</span><span>+60</span>
    </div>
    <div class="smeter-readout">
      <span id="s-val">S0</span>
      <span id="rssi-val">0 dBµV</span>
      <span id="snr-val">SNR: 0 dB</span>
    </div>
  </div>
</div>

<div class="card">
  <div class="grid-4" style="margin-bottom:6px">
    <button class="btn-tune" onclick="cmd('/api/step?delta=-10')">◀◀ -10</button>
    <button class="btn-tune" onclick="cmd('/api/step?dir=-1')">◀ -Step</button>
    <button class="btn-tune" onclick="cmd('/api/step?dir=1')">+Step ▶</button>
    <button class="btn-tune" onclick="cmd('/api/step?delta=10')">+10 ▶▶</button>
  </div>
  <div class="grid-2">
    <button onclick="cmd('/api/seek?dir=-1')">🔍 Seek Down</button>
    <button onclick="cmd('/api/seek?dir=1')">Seek Up 🔍</button>
  </div>
  <div class="grid-2" style="margin-top:6px">
    <button id="btn-keypad-toggle" onclick="toggleKeypad()">🔢 Direct Frequency Keypad</button>
    <button id="btn-bfo-toggle" onclick="toggleBfo()">🎛 SSB BFO Fine Tune</button>
  </div>
  <div id="keypad-panel" style="display:none;margin-top:8px;background:var(--card2);padding:8px;border-radius:8px">
    <div style="display:flex;gap:6px">
      <input type="text" id="keypad-input" placeholder="e.g. 104.5 or 7120" style="font-family:monospace;font-size:16px;text-align:center" readonly>
      <button style="min-width:50px" onclick="kpClear()">C</button>
    </div>
    <div class="keypad-grid">
      <button class="key-btn" onclick="kpNum('1')">1</button>
      <button class="key-btn" onclick="kpNum('2')">2</button>
      <button class="key-btn" onclick="kpNum('3')">3</button>
      <button class="key-btn" onclick="kpNum('4')">4</button>
      <button class="key-btn" onclick="kpNum('5')">5</button>
      <button class="key-btn" onclick="kpNum('6')">6</button>
      <button class="key-btn" onclick="kpNum('7')">7</button>
      <button class="key-btn" onclick="kpNum('8')">8</button>
      <button class="key-btn" onclick="kpNum('9')">9</button>
      <button class="key-btn" onclick="kpNum('.')">.</button>
      <button class="key-btn" onclick="kpNum('0')">0</button>
      <button class="key-btn" onclick="kpDel()">⌫</button>
    </div>
    <button class="btn-accent" style="width:100%;margin-top:6px;padding:12px;font-size:15px" onclick="kpTune()">🎯 TUNE FREQUENCY</button>
  </div>
  <div id="bfo-panel" style="display:none;margin-top:8px;background:var(--card2);padding:8px;border-radius:8px">
    <div class="slider-row">
      <span class="slider-label">BFO</span>
      <input type="range" class="slider" id="bfo-slider" min="-5000" max="5000" step="10" value="0" oninput="onBfoInput(this.value)">
      <span class="val-badge" id="bfo-val">0 Hz</span>
    </div>
    <div class="grid-4" style="margin-top:4px">
      <button onclick="cmd('/api/bfo?delta=-50')">-50</button>
      <button onclick="cmd('/api/bfo?delta=-10')">-10</button>
      <button onclick="cmd('/api/bfo?val=0')">Zero</button>
      <button onclick="cmd('/api/bfo?delta=10')">+10</button>
    </div>
  </div>
</div>

<div class="card">
  <div style="font-size:11px;color:var(--sub);margin-bottom:4px">MODE</div>
  <div class="grid-4" style="margin-bottom:8px">
    <button id="m-fm" onclick="cmd('/api/mode?idx=0')">FM</button>
    <button id="m-lsb" onclick="cmd('/api/mode?idx=1')">LSB</button>
    <button id="m-usb" onclick="cmd('/api/mode?idx=2')">USB</button>
    <button id="m-am" onclick="cmd('/api/mode?idx=3')">AM</button>
  </div>
  <div class="slider-row">
    <span class="slider-label">VOL</span>
    <input type="range" class="slider" id="vol-slider" min="0" max="63" value="45" oninput="onVolInput(this.value)">
    <span class="val-badge" id="vol-val">45</span>
    <button style="min-width:60px;padding:6px" id="btn-mute" onclick="cmd('/api/mute')">Mute</button>
  </div>
  <div style="font-size:11px;color:var(--sub);margin:6px 0 2px">FILTER BANDWIDTH</div>
  <div class="pill-row" id="bw-pills"></div>
  <div style="font-size:11px;color:var(--sub);margin:6px 0 2px">TUNING STEP</div>
  <div class="pill-row" id="step-pills"></div>
  <div class="grid-2" style="margin-top:8px">
    <button id="btn-agc" onclick="cmd('/api/agc')">AGC: Auto</button>
    <div style="display:flex;align-items:center;gap:4px">
      <span style="font-size:11px;color:var(--sub)">SQ:</span>
      <input type="range" class="slider" id="sq-slider" min="0" max="100" value="0" onchange="cmd('/api/squelch?val='+this.value)">
      <span class="val-badge" id="sq-val" style="min-width:26px">0</span>
    </div>
  </div>
</div>

<div class="nav-tabs">
  <div class="nav-tab active" onclick="showTab(0)">All Bands (28)</div>
  <div class="nav-tab" onclick="showTab(1)">Memory (99)</div>
  <div class="nav-tab" onclick="showTab(2)">Wi-Fi Setup</div>
</div>

<div class="tab-content active" id="tab-0">
  <div class="card">
    <div class="band-grid" id="band-list">Loading bands...</div>
  </div>
</div>

<div class="tab-content" id="tab-1">
  <div class="card">
    <div style="display:flex;gap:6px;margin-bottom:8px">
      <input type="text" id="save-mem-name" placeholder="Channel Name (e.g. BBC)" style="margin:0">
      <input type="number" id="save-mem-slot" min="1" max="99" value="1" style="width:70px;background:#0d1424;border:1px solid var(--border);border-radius:6px;color:#fff;text-align:center">
      <button class="btn-accent" style="white-space:nowrap" onclick="saveMemory()">Save VFO</button>
    </div>
    <div class="mem-list" id="mem-list">Loading memories...</div>
  </div>
</div>

<div class="tab-content" id="tab-2">
  <div class="card">
    <div style="font-size:13px;font-weight:700;color:var(--cyan);margin-bottom:6px">Connect ATS-Mini to Home Wi-Fi</div>
    <p style="font-size:12px;color:var(--sub);margin-bottom:8px">Scan nearby networks or enter your home Wi-Fi details. Once saved, the radio will connect and be accessible on your local network.</p>
    <button class="btn-accent" style="width:100%;margin-bottom:8px" onclick="scanWifi()">🔍 Scan Available Networks</button>
    <div id="wifi-scan-results" style="margin-bottom:8px"></div>
    <div style="display:flex;flex-direction:column;gap:6px">
      <input type="text" id="wifi-ssid" placeholder="Wi-Fi SSID">
      <input type="password" id="wifi-pass" placeholder="Wi-Fi Password">
      <button class="btn-accent" onclick="saveWifi()">💾 Connect & Save Credentials</button>
    </div>
  </div>
</div>

<div class="toast" id="toast"></div>

<script>

let state = {}, bandsData = [], polling = true, bfoTimer = null, volTimer = null;
function toast(msg){const t=document.getElementById('toast');t.innerText=msg;t.style.display='block';setTimeout(()=>t.style.display='none',2000)}
function cmd(url){fetch(url).then(r=>r.json()).then(d=>{if(d.status==='ok')fetchStatus()}).catch(e=>console.error(e))}

// --- DXing Suite Logic ---
let dxSuiteOpen = true;
let dxLogs = JSON.parse(localStorage.getItem('ats_mini_dx_logs') || '[]');

function toggleDxSuite(){
  dxSuiteOpen = !dxSuiteOpen;
  document.getElementById('dx-suite-content').style.display = dxSuiteOpen ? 'block' : 'none';
  document.getElementById('btn-dx-toggle').innerText = dxSuiteOpen ? 'Collapse ?' : 'Expand ?';
}

function switchDxTab(tabId){
  document.querySelectorAll('.dx-tab').forEach(t=>t.classList.remove('active'));
  document.querySelectorAll('.dx-panel').forEach(p=>p.classList.remove('active'));
  const btn = event.target;
  if(btn) btn.classList.add('active');
  const target = document.getElementById('dx-tab-' + tabId);
  if(target) target.classList.add('active');
  if(tabId === 'logbook') renderDxLog();
  if(tabId === 'station-id') checkLiveStation();
}

function tuneDirect(freqKhz, modeStr){
  cmd('/api/freq?val=' + freqKhz);
  setTimeout(()=>{
    if(modeStr){
      const modeIdx = modeStr==='FM'?0 : (modeStr==='AM'?1 : (modeStr==='USB'?2 : 3));
      cmd('/api/mode?idx=' + modeIdx);
    }
  }, 250);
  toast('Tuned to ' + freqKhz + ' kHz ' + (modeStr||''));
}

// Station identification database & live query
const swRegistry = [
  {freq:15000, name:"WWV Fort Collins", loc:"USA", pwr:"10 kW", lang:"Time/Standard"},
  {freq:10000, name:"WWV Fort Collins", loc:"USA", pwr:"10 kW", lang:"Time/Standard"},
  {freq:5000, name:"WWV Fort Collins", loc:"USA", pwr:"10 kW", lang:"Time/Standard"},
  {freq:20000, name:"WWV Fort Collins", loc:"USA", pwr:"2.5 kW", lang:"Time/Standard"},
  {freq:3330, name:"CHU Ottawa", loc:"Canada", pwr:"3 kW", lang:"Bilingual Time"},
  {freq:7850, name:"CHU Ottawa", loc:"Canada", pwr:"10 kW", lang:"Bilingual Time"},
  {freq:14670, name:"CHU Ottawa", loc:"Canada", pwr:"3 kW", lang:"Bilingual Time"},
  {freq:11850, name:"BBC World Service", loc:"Woofferton, UK", pwr:"250 kW", lang:"English"},
  {freq:9740, name:"BBC World Service", loc:"Ascension Island", pwr:"250 kW", lang:"English/Swahili"},
  {freq:11590, name:"All India Radio", loc:"Bengaluru, India", pwr:"500 kW", lang:"Hindi / External"},
  {freq:9425, name:"Voice of America", loc:"Greenville, NC", pwr:"250 kW", lang:"English / Persian"},
  {freq:11780, name:"Voice of America", loc:"Sao Tome", pwr:"100 kW", lang:"French / English"},
  {freq:9580, name:"Deutsche Welle", loc:"Issoudun, France", pwr:"250 kW", lang:"Amharic / English"},
  {freq:13630, name:"China Radio Intl", loc:"Kashi, China", pwr:"500 kW", lang:"Multi-language"},
  {freq:11880, name:"NHK World Radio Japan", loc:"Yamata, Japan", pwr:"300 kW", lang:"Japanese / English"},
  {freq:9600, name:"Radio Romania Intl", loc:"Galbeni, Romania", pwr:"300 kW", lang:"English / Romanian"},
  {freq:4625, name:"UVB-76 (The Buzzer)", loc:"St. Petersburg, Russia", pwr:"10 kW", lang:"Buzzer Marker"},
  {freq:5505, name:"Shannon VOLMET", loc:"Ballygirreen, Ireland", pwr:"3 kW", lang:"Aviation Weather"},
  {freq:8957, name:"Shannon VOLMET", loc:"Ballygirreen, Ireland", pwr:"3 kW", lang:"Aviation Weather"},
  {freq:13264, name:"Shannon VOLMET", loc:"Ballygirreen, Ireland", pwr:"3 kW", lang:"Aviation Weather"},
  {freq:5450, name:"RAF Military VOLMET", loc:"Inskip, UK", pwr:"10 kW", lang:"Military Weather"},
  {freq:6604, name:"New York VOLMET", loc:"Brentwood, NY", pwr:"5 kW", lang:"Aviation Weather"},
  {freq:5598, name:"North Atlantic ATC (NAT-A)", loc:"Shanwick / Gander", pwr:"10 kW", lang:"Oceanic Air Traffic"},
  {freq:5616, name:"North Atlantic ATC (NAT-B)", loc:"Shanwick / Gander", pwr:"10 kW", lang:"Oceanic Air Traffic"},
  {freq:8864, name:"North Atlantic ATC (NAT-A)", loc:"Shanwick / Gander", pwr:"10 kW", lang:"Oceanic Air Traffic"},
  {freq:8891, name:"North Atlantic ATC (NAT-B)", loc:"Shanwick / Gander", pwr:"10 kW", lang:"Oceanic Air Traffic"},
  {freq:5634, name:"Indian Ocean ATC", loc:"Mumbai / Colombo", pwr:"5 kW", lang:"Oceanic Air Traffic"},
  {freq:8879, name:"Indian Ocean ATC", loc:"Mumbai Radio", pwr:"5 kW", lang:"Oceanic Air Traffic"},
  {freq:10018, name:"Indian Ocean ATC", loc:"Mumbai Radio", pwr:"5 kW", lang:"Oceanic Air Traffic"}
];

function checkLiveStation(){
  const f = state.freq || 0;
  document.getElementById('dx-curr-freq').innerText = f + ' kHz';
  
  // 1. Try querying firmware /api/eibi
  fetch('/api/eibi?freq=' + f).then(r=>r.json()).then(d=>{
    if(d.match && d.name){
      document.getElementById('dx-station-match').innerText = d.name;
      document.getElementById('dx-station-details').innerText = 'EiBi Schedule: ' + d.start + ' - ' + d.end + ' UTC';
      return;
    }
    matchFromRegistry(f);
  }).catch(()=>{
    matchFromRegistry(f);
  });
}

function matchFromRegistry(f){
  const found = swRegistry.find(s=>s.freq === f);
  if(found){
    document.getElementById('dx-station-match').innerText = found.name;
    document.getElementById('dx-station-details').innerText = found.loc + ' ? ' + found.lang + ' ? ' + found.pwr;
  } else {
    document.getElementById('dx-station-match').innerText = 'Unlisted / Weak Carrier';
    document.getElementById('dx-station-details').innerText = 'Tune to nearby standard broadcast or utility channel';
  }
}

// Logbook functions
function logCurrentDx(){
  const now = new Date();
  const utcStr = now.toISOString().slice(0,19).replace('T',' ') + ' UTC';
  const freq = state.freq || 0;
  const mode = state.mode || 'AM';
  const rssi = state.rssi || 0;
  const snr = state.snr || 0;
  const matchElem = document.getElementById('dx-station-match');
  const stationName = matchElem ? matchElem.innerText : 'Unknown';
  const rst = document.getElementById('dx-log-rst').value.trim() || '599';
  const qth = document.getElementById('dx-log-qth').value.trim() || '';

  const entry = {
    id: Date.now(),
    utc: utcStr,
    freq: freq,
    mode: mode,
    rssi: rssi,
    snr: snr,
    station: stationName,
    rst: rst,
    qth: qth
  };

  dxLogs.unshift(entry);
  if(dxLogs.length > 200) dxLogs.pop();
  localStorage.setItem('ats_mini_dx_logs', JSON.stringify(dxLogs));
  renderDxLog();
  toast('Signal Logged: ' + freq + ' kHz (' + mode + ')');
}

function renderDxLog(){
  const tbody = document.getElementById('dx-log-tbody');
  if(!tbody) return;
  if(dxLogs.length === 0){
    tbody.innerHTML = '<tr><td colspan="6" style="text-align:center;color:var(--sub);padding:12px">No logged DX catches yet.</td></tr>';
    return;
  }
  let h = '';
  dxLogs.forEach((item, idx)=>{
    h += '<tr>';
    h += '<td style="font-family:monospace;font-size:10px">' + item.utc.slice(5,16) + '</td>';
    h += '<td style="font-family:monospace;font-weight:700;color:var(--cyan)">' + item.freq + '</td>';
    h += '<td><span class="badge badge-mode ' + item.mode + '" style="font-size:9px;padding:1px 4px">' + item.mode + '</span></td>';
    h += '<td><b>' + item.station + '</b>' + (item.qth ? ' ('+item.qth+')':'') + '</td>';
    h += '<td style="font-family:monospace;font-size:10px">' + item.rssi + 'dBuV (' + item.rst + ')</td>';
    h += '<td><button style="padding:2px 5px;font-size:10px;background:transparent;border:none;color:var(--red)" onclick="delDxLog(' + item.id + ')">?</button></td>';
    h += '</tr>';
  });
  tbody.innerHTML = h;
}

function delDxLog(id){
  dxLogs = dxLogs.filter(e=>e.id !== id);
  localStorage.setItem('ats_mini_dx_logs', JSON.stringify(dxLogs));
  renderDxLog();
}

function clearDxLog(){
  if(!confirm('Clear all logged DX records?')) return;
  dxLogs = [];
  localStorage.removeItem('ats_mini_dx_logs');
  renderDxLog();
  toast('Logbook cleared');
}

function exportCsv(){
  if(dxLogs.length === 0){ toast('Logbook is empty'); return; }
  var csv = 'UTC Time,Frequency (kHz),Mode,Station Name,RSSI (dBuV),SNR (dB),RST,Notes
';
  dxLogs.forEach(function(e){
    csv += '"' + e.utc + '",' + e.freq + ',"' + e.mode + '","' + (e.station||'') + '",' + e.rssi + ',' + e.snr + ',"' + (e.rst||'') + '","' + (e.qth||'') + '"
';
  });
  downloadFile(csv, 'ats_mini_dx_log.csv', 'text/csv');
}

function exportAdif(){
  if(dxLogs.length === 0){ toast('Logbook is empty'); return; }
  var adi = 'ADIF Export from ATS-Mini ESP32 Radio
<EOH>
';
  dxLogs.forEach(function(e){
    var dStr = e.utc.slice(0,10).replace(/-/g,'');
    var tStr = e.utc.slice(11,16).replace(/:/g,'');
    var freqMhz = (e.freq / 1000.0).toFixed(4);
    adi += '<QSO_DATE:' + dStr.length + '>' + dStr;
    adi += '<TIME_ON:' + tStr.length + '>' + tStr;
    adi += '<FREQ:' + freqMhz.length + '>' + freqMhz;
    adi += '<MODE:' + e.mode.length + '>' + e.mode;
    var rstStr = String(e.rst || '599');
    adi += '<RST_RCVD:' + rstStr.length + '>' + rstStr;
    if(e.station){
      var stStr = String(e.station);
      adi += '<COMMENT:' + stStr.length + '>' + stStr;
    }
    adi += '<EOR>
';
  });
  downloadFile(adi, 'ats_mini_dx_log.adi', 'text/plain');
}

function downloadFile(content, fileName, mimeType){
  const b = new Blob([content], {type: mimeType});
  const u = URL.createObjectURL(b);
  const a = document.createElement('a');
  a.href = u;
  a.download = fileName;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(u);
  toast('Downloaded ' + fileName);
}

function fetchStatus(){
  if(!polling) return;
  fetch('/api/status').then(r=>r.json()).then(d=>{
    state = d;
    renderStatus();
  }).catch(()=>{
    document.getElementById('led').style.background = 'var(--red)';
    document.getElementById('led').style.boxShadow = '0 0 8px var(--red)';
  });
}
function renderStatus(){
  document.getElementById('led').style.background = 'var(--green)';
  document.getElementById('led').style.boxShadow = '0 0 8px var(--green)';
  document.getElementById('vfo-band').innerText = state.band_name || '--';
  const mb = document.getElementById('vfo-mode');
  mb.innerText = state.mode || '--';
  mb.className = 'badge badge-mode ' + (state.mode || '');
  document.getElementById('vfo-step').innerText = state.step || '--';
  document.getElementById('vfo-bw').innerText = state.bw || '--';
  let fStr = state.mode === 'FM' ? (state.freq/100).toFixed(2) : (state.freq + (state.bfo||0)/1000).toFixed(2);
  document.getElementById('vfo-freq').innerText = fStr;
  document.getElementById('vfo-unit').innerText = state.mode === 'FM' ? 'MHz' : 'kHz';
  document.getElementById('vfo-station').innerText = state.rds || state.station || (state.band_name + ' Band');
  document.getElementById('bat-info').innerText = '🔋 ' + state.bat_pct + '% (' + state.bat_v.toFixed(2) + 'V)';
  document.getElementById('conn-info').innerText = state.is_ap ? ('AP ' + state.ip) : state.ssid;
  ['fm','lsb','usb','am'].forEach((m,idx)=>{
    const el = document.getElementById('m-'+m);
    if(el) el.className = (state.mode_idx === idx ? 'active' : '');
  });
  if(!volTimer){
    document.getElementById('vol-slider').value = state.vol;
    document.getElementById('vol-val').innerText = state.vol;
  }
  const btnMute = document.getElementById('btn-mute');
  if(state.muted){btnMute.innerText='MUTED';btnMute.style.background='var(--red)'}
  else{btnMute.innerText='Mute';btnMute.style.background='var(--card2)'}
  if(!bfoTimer){
    document.getElementById('bfo-slider').value = state.bfo || 0;
    document.getElementById('bfo-val').innerText = (state.bfo || 0) + ' Hz';
  }
  document.getElementById('btn-agc').innerText = state.agc === 0 ? 'AGC: Auto' : ('ATTN: -' + state.agc + 'dB');
  document.getElementById('sq-slider').value = state.sq || 0;
  document.getElementById('sq-val').innerText = state.sq || 0;
  renderSMeter(state.rssi, state.snr);
}
function renderSMeter(rssi, snr){
  const segs = document.querySelectorAll('.s-seg');
  let level = Math.min(16, Math.max(0, Math.round(rssi / 4)));
  segs.forEach((s,i)=>{
    s.className = 's-seg';
    if(i < level){
      if(i < 9) s.classList.add('on-g');
      else if(i < 13) s.classList.add('on-a');
      else s.classList.add('on-r');
    }
  });
  let sText = level <= 9 ? ('S' + level) : ('S9+' + ((level - 9) * 10) + 'dB');
  document.getElementById('s-val').innerText = sText;
  document.getElementById('rssi-val').innerText = rssi + ' dBµV';
  document.getElementById('snr-val').innerText = 'SNR: ' + snr + ' dB';
}
function onVolInput(v){
  document.getElementById('vol-val').innerText = v;
  clearTimeout(volTimer);
  volTimer = setTimeout(()=>{cmd('/api/volume?val='+v);volTimer=null},120);
}
function onBfoInput(v){
  document.getElementById('bfo-val').innerText = v + ' Hz';
  clearTimeout(bfoTimer);
  bfoTimer = setTimeout(()=>{cmd('/api/bfo?val='+v);bfoTimer=null},150);
}
function toggleKeypad(){
  const p = document.getElementById('keypad-panel');
  p.style.display = p.style.display==='none' ? 'block' : 'none';
}
function toggleBfo(){
  const p = document.getElementById('bfo-panel');
  p.style.display = p.style.display==='none' ? 'block' : 'none';
}
function kpNum(n){document.getElementById('keypad-input').value += n}
function kpDel(){const el=document.getElementById('keypad-input');el.value = el.value.slice(0,-1)}
function kpClear(){document.getElementById('keypad-input').value = ''}
function kpTune(){
  const v = parseFloat(document.getElementById('keypad-input').value);
  if(!v) return;
  let url = v > 200 ? ('/api/tune?khz=' + v) : ('/api/tune?mhz=' + v);
  cmd(url);
  toggleKeypad();
  toast('Tuning to ' + v);
}
function showTab(idx){
  document.querySelectorAll('.nav-tab').forEach((t,i)=>t.classList.toggle('active',i===idx));
  document.querySelectorAll('.tab-content').forEach((c,i)=>c.classList.toggle('active',i===idx));
  if(idx===0 && !bandsData.length) loadBands();
  if(idx===1) loadMemories();
}
function loadBands(){
  fetch('/api/bands').then(r=>r.json()).then(data=>{
    bandsData = data;
    const g = document.getElementById('band-list');
    g.innerHTML = '';
    data.forEach(b=>{
      const btn = document.createElement('button');
      btn.className = 'band-btn' + (state.band_idx === b.idx ? ' active' : '');
      btn.innerHTML = `<strong>${b.name}</strong><span>${b.mode}</span>`;
      btn.onclick = ()=>{cmd('/api/band?idx='+b.idx);toast('Switched to '+b.name)};
      g.appendChild(btn);
    });
  });
}
function loadMemories(){
  fetch('/api/memories').then(r=>r.json()).then(data=>{
    const l = document.getElementById('mem-list');
    l.innerHTML = '';
    if(!data.length){l.innerHTML='<div style="color:var(--sub);padding:8px">No memories saved yet.</div>';return}
    data.forEach(m=>{
      const item = document.createElement('div');
      item.className = 'mem-item';
      let f = m.mode==='FM' ? (m.freq/1000000).toFixed(2)+'M' : (m.freq/1000).toFixed(1)+'k';
      item.innerHTML = `<div><span class="mem-num">#${m.slot}</span> <strong>${m.band}</strong> ${f} <span style="color:var(--sub)">${m.mode}</span> ${m.name?('<em>'+m.name+'</em>'):''}</div><div class="mem-actions"><button class="btn-accent" onclick="cmd('/api/memory_tune?slot=${m.slot}')">Tune</button><button style="color:var(--red)" onclick="delMem(${m.slot})">✕</button></div>`;
      l.appendChild(item);
    });
  });
}
function saveMemory(){
  const slot = document.getElementById('save-mem-slot').value;
  const name = document.getElementById('save-mem-name').value;
  fetch('/api/memory_save?slot='+slot+'&name='+encodeURIComponent(name),{method:'POST'}).then(r=>r.json()).then(()=>{toast('Saved slot #'+slot);loadMemories()});
}
function delMem(slot){
  fetch('/api/memory_delete?slot='+slot,{method:'POST'}).then(r=>r.json()).then(()=>{toast('Cleared slot #'+slot);loadMemories()});
}
function scanWifi(){
  const el = document.getElementById('wifi-scan-results');
  el.innerHTML = '<div style="color:var(--cyan);padding:8px">Scanning 2.4GHz Wi-Fi networks...</div>';
  fetch('/api/wifi_scan').then(r=>r.json()).then(nets=>{
    if(!nets.length){el.innerHTML='<div style="color:var(--sub);padding:8px">No networks found.</div>';return}
    el.innerHTML = '';
    nets.forEach(n=>{
      const c = document.createElement('div');
      c.className = 'wifi-card';
      c.style.cursor = 'pointer';
      c.innerHTML = `<div style="display:flex;justify-content:space-between"><strong>${n.ssid}</strong><span>${n.rssi} dBm ${n.secure?'🔒':''}</span></div>`;
      c.onclick = ()=>{document.getElementById('wifi-ssid').value = n.ssid};
      el.appendChild(c);
    });
  });
}
function saveWifi(){
  const ssid = document.getElementById('wifi-ssid').value;
  const pass = document.getElementById('wifi-pass').value;
  if(!ssid){toast('Please select an SSID');return}
  fetch('/api/wifi_save?ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(pass),{method:'POST'}).then(r=>r.json()).then(()=>{toast('Credentials saved! Connecting...');});
}
function loadBandwidths(){
  const bwMap = {
    0: ['Auto', '110k', '84k', '60k', '40k'],
    1: ['0.5k', '1.0k', '1.2k', '2.2k', '3.0k', '4.0k'],
    2: ['0.5k', '1.0k', '1.2k', '2.2k', '3.0k', '4.0k'],
    3: ['1.0k', '1.8k', '2.0k', '2.5k', '3.0k', '4.0k', '6.0k']
  };
  const stepMap = {
    0: ['10k', '50k', '100k', '200k', '1M'],
    1: ['10Hz', '25Hz', '50Hz', '100Hz', '500Hz', '1kHz', '5kHz', '9kHz', '10kHz'],
    2: ['10Hz', '25Hz', '50Hz', '100Hz', '500Hz', '1kHz', '5kHz', '9kHz', '10kHz'],
    3: ['1kHz', '5kHz', '9kHz', '10kHz', '50kHz', '100kHz', '1MHz']
  };
  const m = state.mode_idx || 0;
  const bp = document.getElementById('bw-pills');
  bp.innerHTML = '';
  (bwMap[m]||[]).forEach((b,i)=>{
    const p = document.createElement('div');
    p.className = 'pill' + (state.bw === b ? ' active' : '');
    p.innerText = b;
    p.onclick = ()=>{cmd('/api/bandwidth?idx='+i)};
    bp.appendChild(p);
  });
  const sp = document.getElementById('step-pills');
  sp.innerHTML = '';
  (stepMap[m]||[]).forEach((s,i)=>{
    const p = document.createElement('div');
    p.className = 'pill' + (state.step === s ? ' active' : '');
    p.innerText = s;
    p.onclick = ()=>{cmd('/api/step_size?idx='+i)};
    sp.appendChild(p);
  });
}
setInterval(fetchStatus, 700);
setTimeout(()=>{fetchStatus();loadBands();loadBandwidths();const now=Math.floor(Date.now()/1000);const tz=-new Date().getTimezoneOffset();fetch('/api/time?epoch='+now+'&minutes='+tz).catch(()=>{});}, 200);
setInterval(loadBandwidths, 2500);

</script>
</body>
</html>
)rawliteral";

#endif // WEBREMOTE_H
