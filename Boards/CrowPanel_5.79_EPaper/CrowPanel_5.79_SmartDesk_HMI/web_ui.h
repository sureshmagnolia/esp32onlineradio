#pragma once
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>CrowPanel 5.79" Smart Desk Console</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;600&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg: #0d1117;
      --card: #161b22;
      --border: #30363d;
      --accent: #2ea043;
      --accent-hover: #3fb950;
      --accent-blue: #58a6ff;
      --text: #c9d1d9;
      --text-bright: #f0f6fc;
      --text-dim: #8b949e;
      --danger: #f85149;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Inter', -apple-system, sans-serif;
      background: var(--bg);
      color: var(--text);
      padding: 16px;
      display: flex;
      flex-direction: column;
      align-items: center;
      min-height: 100vh;
    }
    .container {
      width: 100%;
      max-width: 880px;
      display: flex;
      flex-direction: column;
      gap: 16px;
    }
    header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 16px 20px;
    }
    h1 {
      font-size: 1.25rem;
      font-weight: 700;
      color: var(--text-bright);
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .badge {
      font-size: 0.72rem;
      font-weight: 600;
      padding: 3px 8px;
      border-radius: 20px;
      background: rgba(46, 160, 67, 0.15);
      color: var(--accent);
      border: 1px solid rgba(46, 160, 67, 0.3);
    }
    .nav-tabs {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 8px;
    }
    .tab-btn {
      padding: 12px;
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 10px;
      color: var(--text);
      font-size: 0.95rem;
      font-weight: 600;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      transition: all 0.2s ease;
    }
    .tab-btn:hover { background: #21262d; color: var(--text-bright); }
    .tab-btn.active {
      background: var(--accent);
      color: #fff;
      border-color: var(--accent);
      box-shadow: 0 0 16px rgba(46, 160, 67, 0.35);
    }
    .card {
      background: var(--card);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 20px;
      display: flex;
      flex-direction: column;
      gap: 16px;
    }
    .card-title {
      font-size: 1rem;
      font-weight: 600;
      color: var(--text-bright);
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .preview-box {
      background: #eef2f5;
      border: 2px solid #bbc4cc;
      border-radius: 6px;
      padding: 14px 20px;
      color: #111;
      font-family: 'Inter', sans-serif;
      box-shadow: inset 0 2px 4px rgba(0,0,0,0.06);
    }
    .form-group {
      display: flex;
      flex-direction: column;
      gap: 6px;
    }
    label { font-size: 0.82rem; font-weight: 500; color: var(--text-dim); }
    input, select, textarea {
      background: #0d1117;
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 10px 14px;
      color: var(--text-bright);
      font-size: 0.92rem;
      outline: none;
      font-family: inherit;
      transition: border-color 0.2s;
    }
    input:focus, select:focus, textarea:focus { border-color: var(--accent-blue); }
    .btn-row {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
    }
    button.btn-action {
      background: var(--accent);
      color: #fff;
      border: none;
      border-radius: 8px;
      padding: 11px 18px;
      font-size: 0.92rem;
      font-weight: 600;
      cursor: pointer;
      display: flex;
      align-items: center;
      gap: 6px;
      transition: all 0.2s;
    }
    button.btn-action:hover { background: var(--accent-hover); }
    button.btn-secondary {
      background: #21262d;
      color: var(--text);
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 11px 16px;
      font-size: 0.92rem;
      font-weight: 600;
      cursor: pointer;
      display: flex;
      align-items: center;
      gap: 6px;
    }
    button.btn-secondary:hover { background: #30363d; color: var(--text-bright); }
    .slider-wrap {
      display: flex;
      align-items: center;
      gap: 14px;
    }
    .slider-wrap input[type=range] {
      flex: 1;
      accent-color: var(--accent);
    }
    .status-pill {
      display: inline-block;
      padding: 6px 12px;
      border-radius: 6px;
      background: #000;
      color: #fff;
      font-weight: 700;
      font-size: 0.85rem;
      letter-spacing: 0.5px;
    }
    .radio-bar {
      display: flex;
      align-items: center;
      justify-content: space-between;
      background: #21262d;
      padding: 12px 16px;
      border-radius: 8px;
      border: 1px solid var(--border);
    }
    .mono { font-family: 'JetBrains Mono', monospace; font-size: 0.82rem; color: var(--accent-blue); }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>📻 CrowPanel 5.79" Smart Desk</h1>
      <div id="topBadge" class="badge">● CONNECTED</div>
    </header>

    <!-- Navigation Tabs -->
    <div class="nav-tabs">
      <button class="tab-btn active" onclick="switchView('clock')">🕒 Digital Clock</button>
      <button class="tab-btn" onclick="switchView('radio')">📻 Internet Radio</button>
      <button class="tab-btn" onclick="switchView('nameboard')">📛 Desk Nameplate</button>
    </div>

    <!-- Clock Panel -->
    <div id="viewClock" class="card">
      <div class="card-title">
        <span>Display Mode: Clock Active</span>
        <button class="btn-secondary" onclick="wipeScreen()">🔄 Clear Ghosting</button>
      </div>
      <div class="preview-box">
        <div style="font-size: 2.8rem; font-weight: 800; letter-spacing: -2px;" id="prevTime">08:58 PM</div>
        <div style="font-size: 1rem; font-weight: 600; color: #444; margin-bottom: 8px;" id="prevDate">FRIDAY, 09 OCT 2026</div>
        <div id="prevRadioBanner" style="display:none; background:#000; color:#fff; padding:6px 10px; border-radius:4px; font-size:0.85rem; font-weight:600;">
          ▶ LIVE: <span id="prevRadioName">BBC World Service</span> &nbsp;|&nbsp; <span id="prevRadioVol">VOL 80%</span>
        </div>
      </div>
      <div class="btn-row">
        <button class="btn-action" onclick="setMode('clock')">✓ Apply Clock Mode</button>
      </div>
    </div>

    <!-- Radio Panel -->
    <div id="viewRadio" class="card" style="display: none;">
      <div class="card-title">
        <span>Internet Radio Controls</span>
        <span class="mono" id="streamCodec">MP3 192kbps Stereo</span>
      </div>
      <div class="radio-bar">
        <div>
          <div style="font-size: 1.1rem; font-weight: 700; color: var(--text-bright);" id="radioStationTitle">BBC World Service</div>
          <div style="font-size: 0.85rem; color: var(--text-dim);" id="radioTrackInfo">Top of the Hour Global News Bulletin</div>
        </div>
        <button id="btnPlayPause" class="btn-action" onclick="toggleRadio()">▶ PLAY</button>
      </div>
      <div class="form-group">
        <label>Station Preset</label>
        <select id="stationSelect" onchange="changeStation(this.value)">
          <option value="0">1. BBC World Service</option>
          <option value="1">2. Radio Paradise (Main Mix)</option>
          <option value="2">3. RP Mellow Mix</option>
          <option value="3">4. Classic FM</option>
          <option value="4">5. Linn Jazz</option>
          <option value="5">6. Chill Trap Lobby</option>
          <option value="6">7. Thrissur AIR</option>
          <option value="7">8. Smooth Lounge</option>
        </select>
      </div>
      <div class="form-group">
        <label>Hardware Volume (<span id="volVal">80</span>%)</label>
        <div class="slider-wrap">
          <input type="range" id="volSlider" min="0" max="100" value="80" oninput="setVolume(this.value)">
        </div>
      </div>
      <div class="btn-row">
        <button class="btn-secondary" onclick="prevStation()">⏮ Previous</button>
        <button class="btn-secondary" onclick="nextStation()">⏭ Next Station</button>
        <button class="btn-action" onclick="setMode('radio')">📱 Show Full Radio on Screen</button>
      </div>
    </div>

    <!-- Desk Nameplate Panel -->
    <div id="viewNameboard" class="card" style="display: none;">
      <div class="card-title">
        <span>Tabletop Desk Nameplate Designer</span>
        <button class="btn-action" onclick="pushNameboard()">💾 Push to E-Paper</button>
      </div>
      <div class="preview-box">
        <div style="border: 2px solid #222; padding: 12px; border-radius: 4px; background: #fff;">
          <div style="font-size: 1.6rem; font-weight: 800; letter-spacing: 0.5px;" id="prevName">SURESH KUMAR</div>
          <div style="font-size: 1rem; font-weight: 600; color: #333;" id="prevTitle">Principal IoT & Embedded Systems Architect</div>
          <div style="font-size: 0.8rem; color: #666; margin-bottom: 10px;" id="prevOrg">Magnolia Technologies Inc. • Lab 402</div>
          <div class="status-pill" id="prevStatus">● STATUS: IN A MEETING — DO NOT DISTURB</div>
        </div>
      </div>
      <div class="form-group">
        <label>Full Name</label>
        <input type="text" id="inputName" value="SURESH KUMAR" oninput="updatePreview()">
      </div>
      <div class="form-group">
        <label>Designation / Role</label>
        <input type="text" id="inputTitle" value="Principal IoT & Embedded Systems Architect" oninput="updatePreview()">
      </div>
      <div class="form-group">
        <label>Company / Department / Lab</label>
        <input type="text" id="inputOrg" value="Magnolia Technologies Inc. • Lab 402" oninput="updatePreview()">
      </div>
      <div class="form-group">
        <label>Status Message</label>
        <select id="statusPreset" onchange="applyPreset(this.value)">
          <option value="IN A MEETING — DO NOT DISTURB">🔴 In a Meeting — Do Not Disturb</option>
          <option value="DEEP FOCUS — PLEASE DO NOT DISTURB">⛔ Deep Focus — Please Do Not Disturb</option>
          <option value="AVAILABLE — FEEL FREE TO TALK">🟢 Available — Feel Free to Talk</option>
          <option value="ON A CALL — BACK SHORTLY">📞 On a Call — Back Shortly</option>
          <option value="LUNCH BREAK — BACK AT 2:00 PM">☕ Lunch Break — Back at 2:00 PM</option>
          <option value="WORKING REMOTELY TODAY">🏠 Working Remotely Today</option>
          <option value="CUSTOM">✏️ Custom Status Message...</option>
        </select>
        <input type="text" id="customStatus" style="display:none; margin-top:6px;" placeholder="Type custom status..." oninput="updatePreview()">
      </div>
      <div class="btn-row">
        <button class="btn-action" onclick="pushNameboard()">🚀 Update E-Paper Display</button>
      </div>
    </div>
  </div>

  <script>
    let activeView = 'clock';

    function switchView(view) {
      activeView = view;
      document.querySelectorAll('.tab-btn').forEach((b, i) => {
        b.classList.toggle('active', (i === 0 && view === 'clock') || (i === 1 && view === 'radio') || (i === 2 && view === 'nameboard'));
      });
      document.getElementById('viewClock').style.display = (view === 'clock') ? 'flex' : 'none';
      document.getElementById('viewRadio').style.display = (view === 'radio') ? 'flex' : 'none';
      document.getElementById('viewNameboard').style.display = (view === 'nameboard') ? 'flex' : 'none';
    }

    function setMode(mode) {
      fetch('/api/mode?set=' + mode, { method: 'POST' }).then(() => fetchStatus());
    }

    function toggleRadio() {
      fetch('/api/radio?cmd=toggle', { method: 'POST' }).then(() => fetchStatus());
    }

    function changeStation(id) {
      fetch('/api/radio?cmd=play&id=' + id, { method: 'POST' }).then(() => fetchStatus());
    }

    function prevStation() {
      fetch('/api/radio?cmd=prev', { method: 'POST' }).then(() => fetchStatus());
    }

    function nextStation() {
      fetch('/api/radio?cmd=next', { method: 'POST' }).then(() => fetchStatus());
    }

    function setVolume(v) {
      document.getElementById('volVal').innerText = v;
      fetch('/api/radio?cmd=vol&val=' + v, { method: 'POST' });
    }

    function wipeScreen() {
      fetch('/api/wipe', { method: 'POST' }).then(() => alert('Screen wipe initiated!'));
    }

    function applyPreset(v) {
      const cust = document.getElementById('customStatus');
      if (v === 'CUSTOM') {
        cust.style.display = 'block';
      } else {
        cust.style.display = 'none';
        cust.value = v;
      }
      updatePreview();
    }

    function updatePreview() {
      const name = document.getElementById('inputName').value;
      const title = document.getElementById('inputTitle').value;
      const org = document.getElementById('inputOrg').value;
      const preset = document.getElementById('statusPreset').value;
      const statusText = (preset === 'CUSTOM') ? document.getElementById('customStatus').value : preset;

      document.getElementById('prevName').innerText = name || 'NAME';
      document.getElementById('prevTitle').innerText = title || '';
      document.getElementById('prevOrg').innerText = org || '';
      document.getElementById('prevStatus').innerText = '● STATUS: ' + statusText;
    }

    function pushNameboard() {
      const name = encodeURIComponent(document.getElementById('inputName').value);
      const title = encodeURIComponent(document.getElementById('inputTitle').value);
      const org = encodeURIComponent(document.getElementById('inputOrg').value);
      const preset = document.getElementById('statusPreset').value;
      const statusText = encodeURIComponent((preset === 'CUSTOM') ? document.getElementById('customStatus').value : preset);

      fetch(`/api/nameboard?name=${name}&title=${title}&org=${org}&status=${statusText}`, { method: 'POST' })
        .then(() => alert('Pushed to Desk Nameplate! Display will enter deep sleep at 0mA.'));
    }

    function fetchStatus() {
      fetch('/api/status')
        .then(r => r.json())
        .then(data => {
          if (data.time) document.getElementById('prevTime').innerText = data.time;
          if (data.date) document.getElementById('prevDate').innerText = data.date;
          
          if (data.radio_playing) {
            document.getElementById('btnPlayPause').innerText = '⏸ PAUSE';
            document.getElementById('prevRadioBanner').style.display = 'block';
            document.getElementById('prevRadioName').innerText = data.radio_station;
            document.getElementById('prevRadioVol').innerText = 'VOL ' + data.volume + '%';
          } else {
            document.getElementById('btnPlayPause').innerText = '▶ PLAY';
            document.getElementById('prevRadioBanner').style.display = 'none';
          }

          document.getElementById('radioStationTitle').innerText = data.radio_station;
          document.getElementById('radioTrackInfo').innerText = data.radio_track || 'Live Internet Stream';
          document.getElementById('volSlider').value = data.volume;
          document.getElementById('volVal').innerText = data.volume;
          document.getElementById('stationSelect').value = data.station_index;
        })
        .catch(e => console.log('Offline / Fetch err:', e));
    }

    setInterval(fetchStatus, 3000);
    fetchStatus();
  </script>
</body>
</html>
)rawliteral";
