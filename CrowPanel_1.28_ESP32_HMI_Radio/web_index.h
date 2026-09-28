#pragma once
#include <Arduino.h>

// Static Web Remote HTML/CSS/JS (Stored in Flash Memory - Zero RAM, Instant <2ms delivery)
const char PAGE_INDEX[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>EDIFIER 1.28" Radio Remote</title>
<style>
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:#0a0c10;color:#c9d1d9;margin:0;padding:14px;text-align:center;}
.box{background:#161b22;border:1px solid #30363d;border-radius:14px;padding:16px;margin:0 auto 14px;max-width:520px;box-sizing:border-box;box-shadow:0 6px 18px rgba(0,0,0,0.4);}
h2{margin:2px 0 10px;color:#58a6ff;font-size:18px;display:flex;align-items:center;justify-content:center;gap:8px;}
h3{margin:6px 0 4px;color:#f0f6fc;font-size:19px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
.sub{color:#8b949e;font-size:13px;margin-bottom:12px;}
.badge{display:inline-block;padding:4px 12px;border-radius:12px;font-size:11px;font-weight:700;letter-spacing:0.5px;margin-bottom:6px;}
.badge-live{background:#238636;color:#fff;}
.badge-buf{background:#d29922;color:#000;}
.badge-pause{background:#30363d;color:#8b949e;}
.badge-ch{background:#1f6feb;color:#fff;font-size:11px;padding:2px 8px;border-radius:10px;font-weight:bold;}
.badge-lang{background:#21262d;color:#58a6ff;border:1px solid #30363d;font-size:11px;padding:2px 8px;border-radius:10px;}
.badge-state{background:#161b22;color:#8b949e;border:1px solid #21262d;font-size:10px;padding:2px 6px;border-radius:8px;}
.row{display:flex;justify-content:center;align-items:center;gap:10px;margin:12px 0;}
.btn{background:#21262d;color:#c9d1d9;border:1px solid #30363d;padding:9px 16px;border-radius:8px;font-size:13px;font-weight:600;cursor:pointer;touch-action:manipulation;transition:all 0.15s ease;}
.btn:hover{background:#30363d;}
.btn:active{transform:scale(0.97);}
.btn-blue{background:#1f6feb;color:#fff;border-color:#388bfd;}
.btn-blue:hover{background:#388bfd;}
.btn-play{background:#238636;color:#fff;border:none;padding:6px 12px;border-radius:6px;font-size:12px;cursor:pointer;font-weight:600;}
.btn-play:hover{background:#2ea043;}
.btn-del{background:#da3633;color:#fff;border:none;padding:6px 10px;border-radius:6px;font-size:12px;cursor:pointer;}
.btn-del:hover{background:#f85149;}
.btn-add{background:#1f6feb;color:#fff;border:none;padding:6px 14px;border-radius:6px;font-size:12px;cursor:pointer;font-weight:600;}
.btn-add:hover{background:#388bfd;}
.btn-saved{background:#30363d;color:#8b949e;border:none;padding:6px 12px;border-radius:6px;font-size:12px;cursor:default;}
select,input[type=text],input[type=url],input[type=time],input[type=password]{width:100%;box-sizing:border-box;padding:10px;margin:6px 0;background:#0d1117;color:#f0f6fc;border:1px solid #30363d;border-radius:6px;font-size:14px;}
select:focus,input:focus{outline:none;border-color:#58a6ff;}
#toast{position:fixed;top:14px;left:50%;transform:translateX(-50%);background:#58a6ff;color:#0d1117;padding:8px 18px;border-radius:20px;font-weight:bold;font-size:13px;display:none;z-index:99;box-shadow:0 4px 16px rgba(0,0,0,0.6);}
.pills{display:flex;flex-wrap:wrap;gap:6px;justify-content:center;margin:10px 0;}
.pill{padding:5px 12px;border-radius:16px;background:#21262d;color:#8b949e;font-size:12px;font-weight:600;cursor:pointer;user-select:none;border:1px solid #30363d;transition:all 0.15s ease;}
.pill:hover{color:#c9d1d9;border-color:#58a6ff;}
.pill.active{background:#1f6feb;color:#fff;border-color:#388bfd;}
.cat-list{max-height:360px;overflow-y:auto;border:1px solid #30363d;border-radius:8px;background:#0d1117;text-align:left;}
.cat-item{display:flex;justify-content:space-between;align-items:center;padding:10px 12px;border-bottom:1px solid #21262d;}
.cat-item:last-child{border-bottom:none;}
.cat-info{flex:1;min-width:0;padding-right:10px;}
.cat-name{font-weight:600;font-size:14px;color:#f0f6fc;}
.cat-meta{font-size:12px;color:#8b949e;margin-top:3px;display:flex;align-items:center;gap:6px;flex-wrap:wrap;}
.badge-lang{background:#1f6feb;color:#fff;padding:2px 7px;border-radius:10px;font-size:10px;font-weight:600;}
.badge-state{background:#21262d;color:#8b949e;padding:2px 7px;border-radius:10px;font-size:10px;border:1px solid #30363d;}
.badge-air{background:#d29922;color:#0d1117;padding:2px 7px;border-radius:10px;font-size:10px;font-weight:700;}
.badge-cat{background:#8957e5;color:#fff;padding:2px 7px;border-radius:10px;font-size:10px;font-weight:600;}
table.fav-table{width:100%;border-collapse:collapse;margin-top:8px;font-size:13px;}
table.fav-table th, table.fav-table td{padding:8px 10px;text-align:left;border-bottom:1px solid #21262d;}
table.fav-table th{color:#8b949e;font-weight:600;font-size:11px;text-transform:uppercase;border-bottom:1px solid #30363d;}
</style>
</head>
<body>
<div id="toast"></div>

<!-- 1. Live Radio Remote Box -->
<div class="box">
  <h2><svg viewBox="170 55 110 155" width="20" height="20" style="vertical-align:middle;"><path d="M 265,155 L 260,153 L 258,153 L 251,156 L 247,162 L 247,164 L 240,177 L 240,179 L 232,195 L 232,200 L 234,202 L 236,203 L 240,203 L 247,200 L 253,196 L 260,189 L 264,183 L 268,175 L 270,167 L 270,163 L 268,158 Z M 244,110 L 238,110 L 235,111 L 231,115 L 223,131 L 223,133 L 219,140 L 219,142 L 204,172 L 204,174 L 198,185 L 197,188 L 197,194 L 199,198 L 201,200 L 205,202 L 211,202 L 215,200 L 218,197 L 219,195 L 219,193 L 225,182 L 225,180 L 232,167 L 232,165 L 240,150 L 240,148 L 245,139 L 245,137 L 251,126 L 252,123 L 252,119 L 251,116 L 246,111 Z M 227,62 L 222,62 L 218,64 L 215,67 L 207,83 L 207,85 L 202,94 L 202,96 L 195,109 L 195,111 L 193,113 L 193,115 L 188,124 L 188,126 L 186,128 L 186,130 L 181,139 L 177,152 L 177,166 L 179,173 L 183,177 L 187,177 L 190,174 L 195,164 L 195,162 L 202,149 L 202,147 L 207,138 L 207,136 L 223,104 L 223,102 L 228,93 L 228,91 L 235,78 L 236,75 L 236,72 L 234,67 L 231,64 Z" fill="#ffffff" fill-rule="evenodd"/></svg> <b style="color:#fff;">EDIFIER</b> 1.28" Radio Remote</h2>
  <div id="t_badge" class="badge badge-pause">[ CONNECTING... ]</div>
  <h3 id="t_name">Connecting to Radio...</h3>
  <div class="sub" id="t_meta">Live HMI Rotary Portal</div>
  
  <div class="row">
    <button type="button" class="btn" onclick="sendCmd('prev','⏮ Prev')">⏮ Prev</button>
    <button type="button" class="btn btn-blue" id="b_play" onclick="sendCmd('play_pause','Play/Pause')">▶ Play</button>
    <button type="button" class="btn" onclick="sendCmd('next','Next ⏭')">Next ⏭</button>
  </div>
  
  <div class="row" style="margin-top:14px;">
    <button type="button" class="btn" id="b_mute" onclick="sendCmd('mute','Mute')">🔊 Mute</button>
    <input type="range" id="v_slider" min="0" max="21" value="21" style="flex:1;" oninput="sendVol(this.value)">
    <span id="v_val" style="min-width:44px;font-weight:bold;color:#58a6ff;font-size:13px;">100%</span>
  </div>
</div>

<!-- 2. Ambient Light & LED Ring Box -->
<div class="box">
  <div style="font-weight:600;color:#bf7af0;margin-bottom:8px;text-align:left;display:flex;align-items:center;gap:6px;">
    <span>✨</span> 5x NeoPixel Ambient Illumination
  </div>
  <div class="row" style="margin:6px 0;gap:10px;">
    <select id="led_mode" onchange="sendLedConfig()" style="flex:1;">
      <option value="0">Soft Audio Breathing</option>
      <option value="1">Volume VU Meter</option>
      <option value="2">Rotary Movement Reactive</option>
      <option value="3">Rainbow Cycle</option>
      <option value="4">Solid Cyan Glow</option>
      <option value="5">LEDs Disabled</option>
    </select>
  </div>
  <div class="row" style="margin-top:6px;">
    <label style="font-size:12px;color:#8b949e;min-width:70px;text-align:left;">Brightness:</label>
    <input type="range" id="led_bright" min="0" max="100" value="30" style="flex:1;" oninput="sendLedConfig()">
    <span id="led_bval" style="min-width:40px;font-weight:bold;color:#bf7af0;font-size:12px;">30%</span>
  </div>
</div>

<!-- 3. Auto-On Alarm & Sleep Timer Box -->
<div class="box">
  <div style="font-weight:600;color:#58a6ff;margin-bottom:8px;text-align:left;">⏰ Auto-On Alarm & Sleep Timer</div>
  <div class="row" style="margin:8px 0;gap:10px;">
    <input type="time" id="tm_val" value="06:30" style="flex:1;font-size:16px;padding:8px;text-align:center;">
    <label style="display:flex;align-items:center;gap:6px;font-size:14px;cursor:pointer;color:#f0f6fc;">
      <input type="checkbox" id="tm_en" style="width:18px;height:18px;"> Enable
    </label>
  </div>
  <div style="margin:8px 0 10px;text-align:left;">
    <label style="font-size:12px;color:#8b949e;display:block;margin-bottom:4px;">Auto-Off Duration (Playback limit):</label>
    <select id="tm_dur">
      <option value="15">15 Minutes</option>
      <option value="30" selected>30 Minutes</option>
      <option value="45">45 Minutes</option>
      <option value="60">60 Minutes</option>
      <option value="90">90 Minutes</option>
      <option value="120">120 Minutes</option>
      <option value="0">Continuous (No Auto-Off)</option>
    </select>
  </div>
  <button type="button" class="btn btn-blue" style="width:100%;margin-top:4px;" onclick="saveTimer()">💾 Save Alarm & Duration</button>
</div>

<!-- 4. Device Favorites Box -->
<div class="box">
  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;">
    <div style="font-weight:600;color:#ffd54f;font-size:16px;">⭐ Radio Favorites (Stored on Device)</div>
    <span style="font-size:12px;color:#8b949e;"><span id="favCount" style="color:#58a6ff;font-weight:bold;">0</span> saved</span>
  </div>
  <div style="font-size:12px;color:#8b949e;text-align:left;margin-bottom:10px;">
    Tuned with the rotary knob & 1.28" round touch screen.
  </div>
  <div style="overflow-x:auto;">
    <table class="fav-table">
      <thead>
        <tr><th style="width:15%;">CH</th><th style="width:48%;">Station Name</th><th style="width:20%;">Lang</th><th style="width:17%;text-align:right;">Actions</th></tr>
      </thead>
      <tbody id="favTableBody">
        <tr><td colspan="4" style="text-align:center;color:#8b949e;">Loading favorites...</td></tr>
      </tbody>
    </table>
  </div>
</div>

<!-- 5. Add Custom Stream Box -->
<div class="box">
  <div style="font-weight:600;color:#58a6ff;margin-bottom:8px;text-align:left;">➕ Add Custom Stream</div>
  <input type="text" id="cs_name" placeholder="Station Name (e.g. Club FM)" required>
  <input type="url" id="cs_url" placeholder="Stream URL (.mp3 / .aac / .m3u8)" required>
  <div style="display:flex;gap:8px;">
    <input type="text" id="cs_state" placeholder="State (e.g. Kerala)">
    <input type="text" id="cs_lang" placeholder="Language (e.g. Malayalam)">
  </div>
  <label style="display:block;margin:6px 0 8px;font-size:13px;color:#c9d1d9;text-align:left;cursor:pointer;">
    <input type="checkbox" id="cs_play_now"> ▶ Play this station immediately
  </label>
  <button type="button" class="btn btn-blue" style="width:100%;margin-top:4px;" onclick="submitCustomStation()">➕ Add to Radio Favorites</button>
</div>

<!-- 6. Master Online Catalog Box -->
<div class="box">
  <div style="font-weight:600;color:#58a6ff;font-size:16px;margin-bottom:4px;text-align:left;">🌐 Master Online Catalog (GitHub)</div>
  <div style="font-size:12px;color:#8b949e;text-align:left;margin-bottom:8px;">
    Live 600+ station directory. Search or filter by language and category. Tap <b style="color:#2ea043;">[▶ Play]</b> to listen immediately, or <b style="color:#ffd54f;">[⭐ Add]</b> to save into favorites.
  </div>
  <input type="text" id="catSearch" placeholder="🔍 Search 600+ stations by name, city, state, language, genre..." oninput="filterCatalog()">
  
  <div style="font-size:11px;color:#8b949e;text-align:left;margin-top:6px;font-weight:600;">LANGUAGES:</div>
  <div class="pills" id="catPills" style="margin:4px 0 6px;">
    <div class="pill active" onclick="selectPill(this, 'ALL')">All</div>
    <div class="pill" onclick="selectPill(this, 'Malayalam')">🌴 Malayalam</div>
    <div class="pill" onclick="selectPill(this, 'Tamil')">🪔 Tamil</div>
    <div class="pill" onclick="selectPill(this, 'Hindi')">🇮🇳 Hindi</div>
    <div class="pill" onclick="selectPill(this, 'English')">🌍 English</div>
    <div class="pill" onclick="selectPill(this, 'Telugu')">Telugu</div>
    <div class="pill" onclick="selectPill(this, 'Kannada')">Kannada</div>
    <div class="pill" onclick="selectPill(this, 'Marathi')">Marathi</div>
  </div>

  <div style="font-size:11px;color:#8b949e;text-align:left;margin-top:2px;font-weight:600;">STATION TYPE:</div>
  <div class="pills" id="catTypePills" style="margin:4px 0 8px;">
    <div class="pill active" onclick="selectTypePill(this, 'ALL')">All Types</div>
    <div class="pill" onclick="selectTypePill(this, 'Private')">📻 Private & FM</div>
    <div class="pill" onclick="selectTypePill(this, 'AIR')">🏛️ All India Radio</div>
  </div>

  <div style="display:flex;justify-content:space-between;align-items:center;font-size:12px;color:#8b949e;margin:4px 2px 6px;">
    <span>Showing <b id="catCount" style="color:#58a6ff;">0</b> stations</span>
    <span style="color:#8b949e;font-size:11px;">GitHub Catalog</span>
  </div>

  <div class="cat-list" id="catContainer">
    <div style="padding:20px;text-align:center;color:#8b949e;">Loading 570+ online stations...</div>
  </div>
</div>

<!-- 7. Always-ON WiFi Connection & AP Setup Box -->
<div class="box">
  <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;">
    <div style="font-weight:600;color:#58a6ff;font-size:16px;">📶 WiFi & Access Point Settings</div>
    <button type="button" class="btn btn-blue" id="b_scan_wifi" style="padding:5px 12px;font-size:12px;" onclick="scanWifi()">🔍 Scan Networks</button>
  </div>
  <div style="font-size:12px;color:#8b949e;text-align:left;margin-bottom:10px;">
    SoftAP: <b style="color:#f0f6fc;">Edifier-Radio</b> (IP: <span style="color:#58a6ff;">192.168.4.1</span>)<br>
    Station IP: <span id="sta_ip" style="color:#58a6ff;">Connected...</span>
  </div>
  <div id="wifi_scan_box" style="display:none;margin-bottom:12px;text-align:left;">
    <div style="font-size:12px;color:#8b949e;margin-bottom:6px;display:flex;justify-content:space-between;">
      <span>Available 2.4GHz Networks:</span>
      <span id="wifi_scan_status" style="color:#58a6ff;">Scanning...</span>
    </div>
    <div id="wifi_list" style="max-height:180px;overflow-y:auto;border:1px solid #30363d;border-radius:8px;background:#0d1117;">
      <!-- Populated dynamically -->
    </div>
  </div>
  <input type="text" id="wf_ssid" placeholder="Home WiFi SSID (or tap network above)">
  <input type="password" id="wf_pass" placeholder="Home WiFi Password">
  <button type="button" class="btn btn-blue" style="width:100%;margin-top:6px;" onclick="saveWifi()">💾 Connect to WiFi</button>
</div>

<script>
const CATALOG_URLS = [
  "https://raw.githubusercontent.com/sureshmagnolia/esp32onlineradio/waveshare28-stereo-radio/stations.json",
  "https://raw.githubusercontent.com/sureshmagnolia/esp32onlineradio/main/stations.json",
  "https://raw.githubusercontent.com/sureshmagnolia/ESP32-SI4732-Radio/main/stations.json"
];
let deviceFavorites = [];
let onlineCatalog = [];
let activePill = "ALL";
let activeTypePill = "ALL";
let tTimer = null;

function showToast(m){
  let e=document.getElementById('toast');
  e.innerText=m;
  e.style.display='block';
  if(tTimer)clearTimeout(tTimer);
  tTimer=setTimeout(()=>e.style.display='none',2800);
}

function sendCmd(a,msg){
  if(msg)showToast(msg);
  fetch('/api/cmd?action='+a).then(()=>setTimeout(syncStatus,150)).catch(()=>{});
}

let vTimer=null;
function sendVol(v){
  document.getElementById('v_val').innerText=Math.round((v/21)*100)+'%';
  if(vTimer)clearTimeout(vTimer);
  vTimer=setTimeout(()=>fetch('/api/cmd?action=vol&val='+v).catch(()=>{}),100);
}

let ledTimer=null;
function sendLedConfig(){
  let m = document.getElementById('led_mode').value;
  let b = document.getElementById('led_bright').value;
  document.getElementById('led_bval').innerText = b + '%';
  if(ledTimer) clearTimeout(ledTimer);
  ledTimer = setTimeout(()=>{
    fetch('/api/led?mode='+m+'&bright='+b).catch(()=>{});
  }, 100);
}

function syncStatus(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('t_name').innerText=d.title||'Radio';
    document.getElementById('t_meta').innerText=(d.state||'')+(d.lang?' • '+d.lang:'');
    let b=document.getElementById('t_badge');
    if(d.buffering){b.innerText='[ BUFFERING ]';b.className='badge badge-buf';}
    else if(d.playing){b.innerText='[ LIVE ]';b.className='badge badge-live';}
    else{b.innerText='[ PAUSED ]';b.className='badge badge-pause';}
    let bp=document.getElementById('b_play');
    bp.innerText=d.playing?'⏸ Pause':'▶ Play';
    if(d.playing){bp.className='btn btn-blue';}else{bp.className='btn';}
    document.getElementById('v_slider').value=d.vol;
    document.getElementById('v_val').innerText=d.muted?'MUTED':(Math.round((d.vol/21)*100)+'%');
    document.getElementById('b_mute').innerText=d.muted?'🔇 Unmute':'🔊 Mute';
    if(d.sta_ip) document.getElementById('sta_ip').innerText = d.sta_ip;
    if(d.led_mode!==undefined) document.getElementById('led_mode').value = d.led_mode;
    if(d.led_bright!==undefined) {
      document.getElementById('led_bright').value = d.led_bright;
      document.getElementById('led_bval').innerText = d.led_bright + '%';
    }
  }).catch(()=>{});
}

function loadTimer(){
  fetch('/api/timer').then(r=>r.json()).then(d=>{
    if(d.hour!==undefined){
      let hh=String(d.hour).padStart(2,'0');
      let mm=String(d.min).padStart(2,'0');
      document.getElementById('tm_val').value=hh+':'+mm;
      document.getElementById('tm_en').checked=!!d.enabled;
      document.getElementById('tm_dur').value=String(d.dur||30);
    }
  }).catch(()=>{});
}

function saveTimer(){
  let val=document.getElementById('tm_val').value;
  let en=document.getElementById('tm_en').checked?1:0;
  let dur=document.getElementById('tm_dur').value;
  let parts=val.split(':');
  let h=parseInt(parts[0])||0;
  let m=parseInt(parts[1])||0;
  fetch('/api/timer?en='+en+'&h='+h+'&m='+m+'&dur='+dur)
    .then(r=>r.json())
    .then(d=>{
      showToast('Alarm & Timer saved!');
      loadTimer();
    }).catch(()=>showToast('Failed to save timer'));
}

function loadFavorites(){
  fetch('/api/favorites').then(r=>r.json()).then(d=>{
    deviceFavorites = Array.isArray(d) ? d : (d.favorites || []);
    renderFavorites();
    renderCatalog();
  }).catch(e=>{
    console.error("Failed to load favorites", e);
  });
}

function renderFavorites(){
  let tb = document.getElementById('favTableBody');
  document.getElementById('favCount').innerText = deviceFavorites.length;
  if(deviceFavorites.length === 0){
    tb.innerHTML = '<tr><td colspan="4" style="text-align:center;color:#8b949e;padding:16px;">No favorites saved yet. Add from catalog below!</td></tr>';
    return;
  }
  let h = '';
  deviceFavorites.forEach((st, idx) => {
    let chStr = 'CH ' + String(idx + 1).padStart(2, '0');
    h += '<tr>';
    h += '<td><span class="badge-ch">' + chStr + '</span></td>';
    h += '<td><div style="font-weight:600;color:#f0f6fc;">' + escapeHtml(st.name) + '</div><div style="font-size:11px;color:#8b949e;">' + escapeHtml(st.state || '') + '</div></td>';
    h += '<td><span class="badge-lang">' + escapeHtml(st.lang || 'Radio') + '</span></td>';
    h += '<td style="text-align:right;white-space:nowrap;">';
    h += '<button type="button" class="btn-play" onclick="playFav(' + idx + ')">▶</button> ';
    h += '<button type="button" class="btn-del" onclick="deleteFav(' + idx + ', \'' + escapeJs(st.name) + '\')">🗑</button>';
    h += '</td>';
    h += '</tr>';
  });
  tb.innerHTML = h;
}

function playFav(idx){
  showToast('Tuning to CH ' + String(idx + 1).padStart(2, '0') + '...');
  fetch('/api/play?id=' + idx).then(()=>setTimeout(syncStatus, 300)).catch(()=>{});
}

function deleteFav(idx, name){
  if(!confirm('Remove "' + name + '" from radio favorites?')) return;
  showToast('Removing ' + name + '...');
  fetch('/api/favorites/delete?id=' + idx, { method: 'POST' })
    .then(r => r.json())
    .then(res => {
      showToast('Removed ' + name);
      loadFavorites();
    }).catch(() => showToast('Failed to delete favorite'));
}

function submitCustomStation(){
  let name = document.getElementById('cs_name').value.trim();
  let url = document.getElementById('cs_url').value.trim();
  let state = document.getElementById('cs_state').value.trim();
  let lang = document.getElementById('cs_lang').value.trim();
  let playNow = document.getElementById('cs_play_now').checked ? 1 : 0;
  if(!name || !url){
    showToast('Name and URL required');
    return;
  }
  showToast('Adding station...');
  let body = 'name=' + encodeURIComponent(name) +
             '&url=' + encodeURIComponent(url) +
             '&state=' + encodeURIComponent(state) +
             '&lang=' + encodeURIComponent(lang) +
             '&play=' + playNow;
  fetch('/api/favorites/add', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: body
  }).then(r => r.json()).then(res => {
    showToast('Station added to Favorites!');
    document.getElementById('cs_name').value = '';
    document.getElementById('cs_url').value = '';
    document.getElementById('cs_state').value = '';
    document.getElementById('cs_lang').value = '';
    loadFavorites();
  }).catch(() => showToast('Failed to add station'));
}

function loadCatalog(){
  let tryLoad = (idx) => {
    if(idx >= CATALOG_URLS.length){
      document.getElementById('catContainer').innerHTML = 
        '<div style="padding:20px;text-align:center;color:#da3633;">Failed to load online catalog from GitHub.<br><small>Check internet connection</small></div>';
      return;
    }
    fetch(CATALOG_URLS[idx])
      .then(r => r.json())
      .then(data => {
        onlineCatalog = Array.isArray(data) ? data : ((data && data.stations) ? data.stations : []);
        renderCatalog();
      })
      .catch(() => tryLoad(idx + 1));
  };
  tryLoad(0);
}

function isStationInFavorites(st){
  if(!deviceFavorites || deviceFavorites.length === 0) return false;
  return deviceFavorites.some(f => f.url && st.url && f.url.trim().toLowerCase() === st.url.trim().toLowerCase());
}

function renderCatalog(){
  let c = document.getElementById('catContainer');
  let cntElem = document.getElementById('catCount');
  if(!onlineCatalog || onlineCatalog.length === 0){
    c.innerHTML = '<div style="padding:20px;text-align:center;color:#8b949e;">Loading 600+ online stations...</div>';
    if(cntElem) cntElem.innerText = '0';
    return;
  }
  let q = document.getElementById('catSearch').value.trim().toLowerCase();
  let filtered = onlineCatalog.filter(st => {
    let langStr = (st.lang || st.language || '').toLowerCase();
    let matchLang = (activePill === 'ALL') || (langStr === activePill.toLowerCase());
    if(!matchLang) return false;

    let isAir = (st.cat && st.cat.toLowerCase() === 'air') || 
                (st.name && /air\s|akashvani|vividh|rainbow|fm gold/i.test(st.name));
    if(activeTypePill === 'AIR' && !isAir) return false;
    if(activeTypePill === 'Private' && isAir) return false;

    if(!q) return true;
    let n = (st.name || '').toLowerCase();
    let s = (st.state || '').toLowerCase();
    let cat = (st.cat || '').toLowerCase();
    return n.includes(q) || s.includes(q) || langStr.includes(q) || cat.includes(q);
  });

  if(cntElem) cntElem.innerText = filtered.length;

  if(filtered.length === 0){
    c.innerHTML = '<div style="padding:20px;text-align:center;color:#8b949e;">No stations found matching your query.</div>';
    return;
  }

  let html = '';
  filtered.forEach(st => {
    let saved = isStationInFavorites(st);
    let lang = st.lang || st.language || 'Radio';
    let isAir = (st.cat && st.cat.toLowerCase() === 'air') || 
                (st.name && /air\s|akashvani|vividh|rainbow|fm gold/i.test(st.name));
    let catBadge = isAir ? '<span class="badge-air">AIR</span>' : '<span class="badge-cat">Private</span>';

    html += '<div class="cat-item">';
    html += '  <div class="cat-info">';
    html += '    <div class="cat-name">' + escapeHtml(st.name) + '</div>';
    html += '    <div class="cat-meta">';
    html += catBadge;
    if(lang) html += '<span class="badge-lang">' + escapeHtml(lang) + '</span>';
    if(st.state) html += '<span class="badge-state">' + escapeHtml(st.state) + '</span>';
    html += '    </div>';
    html += '  </div>';
    let encData = encodeURIComponent(JSON.stringify(st));
    html += '  <div style="display:flex;gap:6px;align-items:center;">';
    html += '    <button type="button" class="btn-play" onclick="playCatalogDirect(\'' + encData + '\')">▶ Play</button>';
    if(saved){
      html += '    <button type="button" class="btn-saved">✓ Saved</button>';
    } else {
      html += '    <button type="button" class="btn-add" onclick="addCatalogStation(\'' + encData + '\')">⭐ Add</button>';
    }
    html += '  </div>';
    html += '</div>';
  });
  c.innerHTML = html;
}

function playCatalogDirect(encStr){
  try {
    let st = JSON.parse(decodeURIComponent(encStr));
    showToast('Tuning to ' + st.name + '...');
    let body = 'url=' + encodeURIComponent(st.url) +
               '&name=' + encodeURIComponent(st.name) +
               '&state=' + encodeURIComponent(st.state || '') +
               '&lang=' + encodeURIComponent(st.lang || st.language || '');
    fetch('/api/play', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: body
    }).then(() => {
      setTimeout(syncStatus, 300);
    }).catch(() => showToast('Failed to tune stream'));
  } catch(e) {
    console.error(e);
  }
}

function addCatalogStation(encStr){
  try {
    let st = JSON.parse(decodeURIComponent(encStr));
    showToast('Saving ' + st.name + '...');
    let body = 'name=' + encodeURIComponent(st.name) +
               '&url=' + encodeURIComponent(st.url) +
               '&state=' + encodeURIComponent(st.state || '') +
               '&lang=' + encodeURIComponent(st.lang || st.language || '') +
               '&play=0';
    fetch('/api/favorites/add', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: body
    }).then(r => r.json()).then(res => {
      showToast('Added ' + st.name + ' to favorites!');
      loadFavorites();
      renderCatalog();
    }).catch(() => showToast('Failed to add favorite'));
  } catch(e) {
    console.error(e);
  }
}

function filterCatalog(){
  renderCatalog();
}

function selectPill(elem, pill){
  let pills = document.querySelectorAll('#catPills .pill');
  pills.forEach(p => p.classList.remove('active'));
  elem.classList.add('active');
  activePill = pill;
  renderCatalog();
}

function selectTypePill(elem, typePill){
  let pills = document.querySelectorAll('#catTypePills .pill');
  pills.forEach(p => p.classList.remove('active'));
  elem.classList.add('active');
  activeTypePill = typePill;
  renderCatalog();
}

function scanWifi(){
  let btn = document.getElementById('b_scan_wifi');
  let sBox = document.getElementById('wifi_scan_box');
  let sStatus = document.getElementById('wifi_scan_status');
  let list = document.getElementById('wifi_list');
  
  btn.disabled = true;
  btn.innerText = '⌛ Scanning...';
  sBox.style.display = 'block';
  sStatus.innerText = 'Scanning airwaves...';
  list.innerHTML = '<div style="padding:12px;text-align:center;color:#8b949e;font-size:12px;">Scanning 2.4GHz Wi-Fi networks...</div>';
  
  fetch('/api/wifi/scan')
    .then(r => r.json())
    .then(nets => {
      btn.disabled = false;
      btn.innerText = '🔍 Rescan';
      if(!nets || nets.length === 0){
        sStatus.innerText = 'No networks found';
        list.innerHTML = '<div style="padding:12px;text-align:center;color:#8b949e;font-size:12px;">No Wi-Fi networks detected. Try again.</div>';
        return;
      }
      sStatus.innerText = nets.length + ' found';
      nets.sort((a,b) => b.rssi - a.rssi);
      let html = '';
      nets.forEach(n => {
        let pct = Math.min(100, Math.max(0, 2 * (n.rssi + 100)));
        let lock = n.secure ? '🔒' : '🔓';
        let barColor = pct > 65 ? '#2ea043' : (pct > 35 ? '#d29922' : '#da3633');
        html += '<div onclick="selectWifi(\'' + escapeJs(n.ssid) + '\')" style="display:flex;justify-content:space-between;align-items:center;padding:9px 12px;border-bottom:1px solid #21262d;cursor:pointer;" onmouseover="this.style.background=\'#1c2128\'" onmouseout="this.style.background=\'transparent\'">';
        html += '<div style="font-weight:600;font-size:13px;color:#f0f6fc;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:280px;">' + escapeHtml(n.ssid) + '</div>';
        html += '<div style="display:flex;align-items:center;gap:8px;font-size:12px;color:#8b949e;white-space:nowrap;">';
        html += '<span>' + lock + '</span>';
        html += '<span style="color:' + barColor + ';font-weight:bold;">' + pct + '%</span>';
        html += '</div></div>';
      });
      list.innerHTML = html;
    })
    .catch(err => {
      btn.disabled = false;
      btn.innerText = '🔍 Scan Networks';
      sStatus.innerText = 'Scan error';
      list.innerHTML = '<div style="padding:12px;text-align:center;color:#da3633;font-size:12px;">Error scanning Wi-Fi networks.</div>';
    });
}

function selectWifi(ssid){
  document.getElementById('wf_ssid').value = ssid;
  let p = document.getElementById('wf_pass');
  p.focus();
  showToast('Selected: ' + ssid);
}

function saveWifi(){
  let s = document.getElementById('wf_ssid').value.trim();
  let p = document.getElementById('wf_pass').value.trim();
  if(!s){
    showToast('SSID required');
    return;
  }
  showToast('Saving WiFi...');
  fetch('/api/wifi?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p))
    .then(r=>r.json())
    .then(d=>{
      showToast('WiFi credentials saved! Connecting...');
    }).catch(()=>showToast('Failed to save WiFi'));
}

function escapeHtml(s){
  if(!s) return '';
  return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

function escapeJs(s){
  if(!s) return '';
  return s.replace(/\\/g,'\\\\').replace(/"/g,'\\"').replace(/'/g,"\\'");
}

syncStatus();
loadTimer();
loadFavorites();
loadCatalog();
setInterval(syncStatus, 2000);
</script>
</body>
</html>)rawliteral";
