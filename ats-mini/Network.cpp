#include "Common.h"
#include "Storage.h"
#include "Themes.h"
#include "Utils.h"
#include "Menu.h"
#include "Draw.h"
#include "Splash.h"
#include "WebRemote.h"

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <time.h>

#define CONNECT_TIME  3000  // Time of inactivity to start connecting WiFi
#define WIFI_MULTI_TOTAL_TIMEOUT  30000
#define SPLASH_MAX_FILE_SIZE (512U * 1024U)

#ifndef WIFI_POWER_LEVEL
#define WIFI_POWER_LEVEL WIFI_POWER_17dBm
#endif

WiFiMulti wifiMulti;

//
// Access Point (AP) mode settings
//
static const char *apSSID    = "ATS-Mini-Remote";
static const char *apPWD     = 0;       // No password
static const int   apChannel = 1;      // WiFi channel number (1..13)
static const bool  apHideMe  = false;   // TRUE: disable SSID broadcast
static const int   apClients = 3;       // Maximum simultaneous connected clients

static uint16_t ajaxInterval = 2500;

static bool itIsTimeToWiFi = false; // TRUE: Need to connect to WiFi
static uint32_t connectTime = millis();

// Settings
String loginUsername = "";
String loginPassword = "";
static bool wifiScanHidden = false;

// AsyncWebServer object on port 80
AsyncWebServer server(80);

// NTP Client to get time
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org");

static bool wifiInitAP();
static bool wifiConnect();
static void webInit();
static void wifiRegisterPowerLevelCallback();
static void wifiPowerLevelOnEvent(WiFiEvent_t event);

static void webSetConfig(AsyncWebServerRequest *request);
static void webUploadSplash(AsyncWebServerRequest *request, const String &filename,
                            size_t index, uint8_t *data, size_t len, bool final);
static bool webIsAuthenticated(AsyncWebServerRequest *request);
static bool webParseUTCDateTime(const String &text, uint32_t *epoch);

static const String webInputField(const String &name, const String &value, bool pass = false);
static const String webStyleSheet();
static const String webPage(const String &body);
static const String webUtcOffsetSelector();
static const String webThemeSelector();
static const String webRadioPage();
static const String webMemoryPage();
static const String webConfigPage();

struct SplashUploadState
{
  bool incomplete;
  bool tooLarge;
};

static bool webIsAuthenticated(AsyncWebServerRequest *request)
{
  return(loginUsername == "" || loginPassword == "" ||
         request->authenticate(loginUsername.c_str(), loginPassword.c_str()));
}

//
// Delayed WiFi connection
//
void netRequestConnect()
{
  connectTime = millis();
  itIsTimeToWiFi = true;
}

void netTickTime()
{
  // Connect to WiFi if requested
  if(itIsTimeToWiFi && ((millis() - connectTime) > CONNECT_TIME))
  {
    netInit(wifiModeIdx);
    connectTime = millis();
    itIsTimeToWiFi = false;
  }
}

//
// Get current connection status
// (-1 - not connected, 0 - disabled, 1 - connected, 2 - connected to network)
//
int8_t getWiFiStatus()
{
  wifi_mode_t mode = WiFi.getMode();

  switch(mode)
  {
    case WIFI_MODE_NULL:
      return(0);
    case WIFI_AP:
      return(WiFi.softAPgetStationNum()? 1 : -1);
    case WIFI_STA:
      return(WiFi.status()==WL_CONNECTED? 2 : -1);
    case WIFI_AP_STA:
      return((WiFi.status()==WL_CONNECTED)? 2 : WiFi.softAPgetStationNum()? 1 : -1);
    default:
      return(-1);
  }
}

char *getWiFiIPAddress()
{
  static char ip[16];
  if(WiFi.status() == WL_CONNECTED)
    return strcpy(ip, WiFi.localIP().toString().c_str());
  if(WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
    return strcpy(ip, WiFi.softAPIP().toString().c_str());
  return strcpy(ip, "");
}

//
// Stop WiFi hardware
//
void netStop()
{
  wifi_mode_t mode = WiFi.getMode();

  MDNS.end();

  // If network connection up, shut it down
  if((mode==WIFI_STA) || (mode==WIFI_AP_STA))
    WiFi.disconnect(true);

  // If access point up, shut it down
  if((mode==WIFI_AP) || (mode==WIFI_AP_STA))
    WiFi.softAPdisconnect(true);

  WiFi.mode(WIFI_MODE_NULL);
}

//
// Initialize WiFi network and services
//
void netInit(uint8_t netMode, bool showStatus)
{
  netStop();
  wifiRegisterPowerLevelCallback();

  if(netMode == NET_OFF) return;

  bool staConnected = false;

  if(netMode == NET_AP_ONLY)
  {
    WiFi.mode(WIFI_AP);
    if(wifiInitAP() && showStatus) delay(1500);
  }
  else
  {
    WiFi.mode(netMode == NET_AP_CONNECT ? WIFI_AP_STA : WIFI_STA);
    if(netMode == NET_AP_CONNECT) wifiInitAP();

    staConnected = wifiConnect();

    // If STA failed or not configured, fall back to AP mode so user is never locked out
    if(!staConnected)
    {
      WiFi.mode(WIFI_AP);
      if(wifiInitAP() && showStatus) delay(1500);
    }
    else if(showStatus)
    {
      delay(1500);
    }

    if(staConnected)
    {
      ntpClient.setUpdateInterval(5*60*1000);
      clockReset();
      for(int j=0 ; j<10 ; j++)
        if(ntpSyncTime()) break; else delay(200);
    }
  }

  if(netMode == NET_SYNC)
  {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_MODE_NULL);
  }
  else
  {
    webInit();
    MDNS.begin("atsmini");
    MDNS.addService("http", "tcp", 80);
  }
}

//
// Returns TRUE if NTP time is available
//
bool ntpIsAvailable()
{
  return(ntpClient.isTimeSet());
}

//
// Update NTP time and synchronize clock with NTP time
//
bool ntpSyncTime()
{
  if(WiFi.status()==WL_CONNECTED)
  {
    ntpClient.update();

    if(ntpClient.isTimeSet())
      return(clockSetEpoch(ntpClient.getEpochTime()));
  }
  return(false);
}

static void wifiRegisterPowerLevelCallback()
{
  static bool registered = false;

  if(registered) return;

  WiFi.onEvent(wifiPowerLevelOnEvent, ARDUINO_EVENT_WIFI_AP_START);
  WiFi.onEvent(wifiPowerLevelOnEvent, ARDUINO_EVENT_WIFI_STA_START);
  registered = true;
}

static void wifiPowerLevelOnEvent(WiFiEvent_t event)
{
  (void)event;
  WiFi.setTxPower(WIFI_POWER_LEVEL);
}

//
// Initialize WiFi access point (AP)
//
static bool wifiInitAP()
{
  // These are our own access point (AP) addresses
  IPAddress ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  // Start as access point (AP)
  WiFi.softAP(apSSID, apPWD, apChannel, apHideMe, apClients);
  WiFi.softAPConfig(ip, gateway, subnet);

  drawScreen(
    ("Use Access Point " + String(apSSID)).c_str(),
    ("IP : " + WiFi.softAPIP().toString() + " or atsmini.local").c_str()
  );

  ajaxInterval = 2500;
  return(true);
}

//
// Connect to a WiFi network
//
static bool wifiConnect()
{
  String status = "Connecting to WiFi network...";
  int apCount = 0;

  // Clean credentials
  wifiMulti.APlistClean();

  // Get the preferences
  prefs.begin("network", true, STORAGE_PARTITION);
  loginUsername = prefs.getString("loginusername", "");
  loginPassword = prefs.getString("loginpassword", "");
  wifiScanHidden = prefs.getBool("wifiscanhidden", false);

  // Try connecting to known WiFi networks
  for(int j=0 ; (j<3) ; j++)
  {
    char nameSSID[16], namePASS[16];
    sprintf(nameSSID, "wifissid%d", j+1);
    sprintf(namePASS, "wifipass%d", j+1);

    String ssid = prefs.getString(nameSSID, "");
    String password = prefs.getString(namePASS, "");

    if(ssid != "")
    {
      wifiMulti.addAP(ssid.c_str(), password.c_str());
      apCount++;
    }
  }

  // Done with preferences
  prefs.end();

  if(apCount == 0) return false;

  drawScreen(status.c_str());

  consumeAbortPending();
  wl_status_t wifiStatus = WL_NO_SSID_AVAIL;
  uint32_t start = millis();
  while(((millis() - start)<10000) && (wifiStatus!=WL_CONNECTED))
  {
    wifiStatus = (wl_status_t)wifiMulti.run(5000, wifiScanHidden);

    if(consumeAbortPending())
    {
      WiFi.disconnect();
      break;
    }

    if((wifiStatus!=WL_CONNECTED) && ((millis() - start)<10000))
      delay(1000);
  }

  // If failed connecting to WiFi network...
  if (wifiStatus != WL_CONNECTED)
  {
    // WiFi connection failed
    drawScreen(status.c_str(), "No WiFi connection");
    // Done
    return(false);
  }
  else
  {
    // WiFi connection succeeded
    drawScreen(
      ("Connected to WiFi network (" + WiFi.SSID() + ")").c_str(),
      ("IP : " + WiFi.localIP().toString() + " or atsmini.local").c_str()
    );
    // Done
    ajaxInterval = 1000;
    return(true);
  }
}

//
// Initialize internal web server
//
static String getStatusJson()
{
  String json = "{";
  json += "\"freq\":" + String(currentFrequency) + ",";
  uint32_t freqHz = (currentMode == FM) ? (currentFrequency * 10000) : (currentFrequency * 1000 + currentBFO);
  json += "\"freq_hz\":" + String(freqHz) + ",";
  json += "\"bfo\":" + String(currentBFO) + ",";
  json += "\"band_idx\":" + String(bandIdx) + ",";
  json += "\"band_name\":\"" + String(getCurrentBand()->bandName) + "\",";
  json += "\"mode_idx\":" + String(currentMode) + ",";
  json += "\"mode\":\"" + String(bandModeDesc[currentMode]) + "\",";
  json += "\"rssi\":" + String(rssi) + ",";
  json += "\"snr\":" + String(snr) + ",";
  float v = batteryMonitor();
  json += "\"bat_v\":" + String(v, 2) + ",";
  int pct = constrain((int)((v - 3.3f) / (4.2f - 3.3f) * 100.0f), 0, 100);
  json += "\"bat_pct\":" + String(pct) + ",";
  json += "\"vol\":" + String(volume) + ",";
  json += "\"muted\":" + String(muteOn(MUTE_MAIN) ? "true" : "false") + ",";
  json += "\"sq\":" + String(currentSquelch[currentMode] & 0x7f) + ",";
  json += "\"bw_idx\":" + String(bands[bandIdx].bandwidthIdx) + ",";
  json += "\"bw\":\"" + String(getCurrentBandwidth()->desc) + "\",";
  json += "\"step_idx\":" + String(bands[bandIdx].currentStepIdx) + ",";
  json += "\"step\":\"" + String(getCurrentStep()->desc) + "\",";
  json += "\"agc\":" + String(agcIdx) + ",";

  const char *st = getStationName();
  String stStr = (st && st[0]) ? st : "";
  stStr.replace("\"", "\\\"");
  json += "\"station\":\"" + stStr + "\",";

  const char *rt = getRadioText();
  String rtStr = (rt && rt[0]) ? rt : "";
  rtStr.replace("\"", "\\\"");
  json += "\"rds\":\"" + rtStr + "\",";

  bool isConnected = (WiFi.status() == WL_CONNECTED);
  json += "\"is_ap\":" + String(isConnected ? "false" : "true") + ",";
  json += "\"ip\":\"" + String(isConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\",";
  json += "\"ssid\":\"" + String(isConnected ? WiFi.SSID() : "ATS-Mini-Remote") + "\"";
  json += "}";
  return json;
}

static void webInit()
{
  static bool serverInitialized = false;
  if(serverInitialized) return;
  serverInitialized = true;

  // Web Remote Single Page Application
  server.on("/", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", WEB_REMOTE_HTML);
  });

  // REST API: Live Status
  server.on("/api/status", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "application/json", getStatusJson());
  });

  // REST API: Tune Frequency
  server.on("/api/tune", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    uint32_t freqHz = 0;
    if(request->hasParam("freq"))
      freqHz = request->getParam("freq")->value().toInt();
    else if(request->hasParam("khz"))
      freqHz = (uint32_t)(request->getParam("khz")->value().toFloat() * 1000.0f);
    else if(request->hasParam("mhz"))
      freqHz = (uint32_t)(request->getParam("mhz")->value().toFloat() * 1000000.0f);

    if(freqHz > 0)
    {
      Band *curBand = getCurrentBand();
      uint16_t targetFreq = freqFromHz(freqHz, currentMode);
      int targetBfo = isSSB() ? bfoFromHz(freqHz) : 0;
      int targetBand = -1;

      if(isFreqInBand(curBand, targetFreq))
        targetBand = bandIdx;
      else
      {
        for(int i = 0; i < getTotalBands(); i++)
        {
          if(isFreqInBand(&bands[i], targetFreq))
          {
            targetBand = i;
            break;
          }
        }
      }

      if(targetBand >= 0)
      {
        if(targetBand != bandIdx) selectBand(targetBand);
        updateFrequency(targetFreq, true);
        if(isSSB()) updateBFO(targetBfo, true);
        clearStationInfo();
        identifyFrequency(currentFrequency + currentBFO / 1000);
        prefsRequestSave(SAVE_CUR_BAND);
        webNeedRedraw = true;
        request->send(200, "application/json", "{\"status\":\"ok\"}");
        return;
      }
    }
    request->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Out of band\"}");
  });

  // REST API: Step Tune
  server.on("/api/step", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    int delta = 1;
    if(request->hasParam("delta"))
      delta = request->getParam("delta")->value().toInt();
    else if(request->hasParam("dir"))
      delta = request->getParam("dir")->value().toInt();

    doTune(delta);
    prefsRequestSave(SAVE_CUR_BAND);
    webNeedRedraw = true;
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  // REST API: BFO Fine Tune
  server.on("/api/bfo", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    if(isSSB())
    {
      if(request->hasParam("val"))
        updateBFO(request->getParam("val")->value().toInt(), true);
      else if(request->hasParam("delta"))
        updateBFO(currentBFO + request->getParam("delta")->value().toInt(), true);

      prefsRequestSave(SAVE_CUR_BAND);
      webNeedRedraw = true;
    }
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  // REST API: Band Selection
  server.on("/api/band", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    if(request->hasParam("idx"))
    {
      int idx = request->getParam("idx")->value().toInt();
      if(idx >= 0 && idx < getTotalBands())
      {
        selectBand(idx);
        prefsRequestSave(SAVE_ALL);
        webNeedRedraw = true;
        request->send(200, "application/json", "{\"status\":\"ok\"}");
        return;
      }
    }
    request->send(400, "application/json", "{\"status\":\"error\"}");
  });

  // REST API: Mode Selection
  server.on("/api/mode", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    int m = -1;
    if(request->hasParam("idx"))
      m = request->getParam("idx")->value().toInt();
    else if(request->hasParam("name"))
    {
      String n = request->getParam("name")->value();
      for(int i = 0; i < getTotalModes(); i++)
        if(n.equalsIgnoreCase(bandModeDesc[i])) { m = i; break; }
    }

    if(m >= 0 && m < getTotalModes())
    {
      if(m == FM && currentMode != FM)
      {
        selectBand(0);
      }
      else if(m != FM && currentMode == FM)
      {
        selectBand(1);
        bands[bandIdx].bandMode = m;
        selectBand(bandIdx);
      }
      else
      {
        bands[bandIdx].bandMode = m;
        selectBand(bandIdx);
      }
      prefsRequestSave(SAVE_ALL);
      webNeedRedraw = true;
      request->send(200, "application/json", "{\"status\":\"ok\"}");
      return;
    }
    request->send(400, "application/json", "{\"status\":\"error\"}");
  });

  // REST API: Volume
  server.on("/api/volume", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    if(request->hasParam("val"))
    {
      volume = constrain(request->getParam("val")->value().toInt(), 0, 63);
      if(!muteOn(MUTE_MAIN)) rx.setVolume(volume);
    }
    else if(request->hasParam("delta"))
    {
      doVolume(request->getParam("delta")->value().toInt());
    }
    prefsRequestSave(SAVE_SETTINGS);
    webNeedRedraw = true;
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  // REST API: Mute
  server.on("/api/mute", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    if(request->hasParam("val"))
      muteOn(MUTE_MAIN, request->getParam("val")->value().toInt() != 0);
    else
      muteOn(MUTE_MAIN, !muteOn(MUTE_MAIN));

    webNeedRedraw = true;
    request->send(200, "application/json", "{\"status\":\"ok\",\"muted\":" + String(muteOn(MUTE_MAIN) ? "true" : "false") + "}");
  });

  // REST API: Bandwidth
  server.on("/api/bandwidth", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    if(request->hasParam("idx"))
    {
      int idx = request->getParam("idx")->value().toInt();
      if(idx >= 0 && idx <= getLastBandwidth(currentMode))
      {
        bands[bandIdx].bandwidthIdx = idx;
        setBandwidth();
        prefsRequestSave(SAVE_CUR_BAND);
        webNeedRedraw = true;
        request->send(200, "application/json", "{\"status\":\"ok\"}");
        return;
      }
    }
    else if(request->hasParam("dir"))
    {
      doBandwidth(request->getParam("dir")->value().toInt());
      prefsRequestSave(SAVE_CUR_BAND);
      webNeedRedraw = true;
      request->send(200, "application/json", "{\"status\":\"ok\"}");
      return;
    }
    request->send(400, "application/json", "{\"status\":\"error\"}");
  });

  // REST API: Step Size
  server.on("/api/step_size", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    if(request->hasParam("idx"))
    {
      int idx = request->getParam("idx")->value().toInt();
      if(idx >= 0 && idx <= getLastStep(currentMode))
      {
        bands[bandIdx].currentStepIdx = idx;
        rx.setFrequencyStep(getStep(currentMode, idx)->step);
        prefsRequestSave(SAVE_CUR_BAND);
        webNeedRedraw = true;
        request->send(200, "application/json", "{\"status\":\"ok\"}");
        return;
      }
    }
    else if(request->hasParam("dir"))
    {
      doStep(request->getParam("dir")->value().toInt());
      prefsRequestSave(SAVE_CUR_BAND);
      webNeedRedraw = true;
      request->send(200, "application/json", "{\"status\":\"ok\"}");
      return;
    }
    request->send(400, "application/json", "{\"status\":\"error\"}");
  });

  // REST API: AGC Toggle
  server.on("/api/agc", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    int dir = request->hasParam("dir") ? request->getParam("dir")->value().toInt() : 1;
    doAgc(dir);
    prefsRequestSave(SAVE_SETTINGS);
    webNeedRedraw = true;
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  // REST API: Squelch
  server.on("/api/squelch", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    if(request->hasParam("val"))
    {
      int val = constrain(request->getParam("val")->value().toInt(), 0, 100);
      uint8_t squelchParam = currentSquelch[currentMode] & 0x80;
      currentSquelch[currentMode] = squelchParam | (val & 0x7f);
      prefsRequestSave(SAVE_SETTINGS);
      webNeedRedraw = true;
    }
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  // REST API: Time & Timezone sync
  server.on("/api/time", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    if(request->hasParam("epoch"))
    {
      uint32_t ep = request->getParam("epoch")->value().toInt();
      if(ep > 1700000000) clockSetEpoch(ep);
    }
    if(request->hasParam("offset"))
    {
      int idx = request->getParam("offset")->value().toInt();
      if(idx >= 0 && idx < getTotalUTCOffsets())
      {
        utcOffsetIdx = idx;
        prefsRequestSave(SAVE_SETTINGS);
        webNeedRedraw = true;
      }
    }
    else if(request->hasParam("minutes"))
    {
      int minutes = request->getParam("minutes")->value().toInt();
      for(int i = 0; i < getTotalUTCOffsets(); i++)
      {
        if(utcOffsets[i].offset * 15 == minutes)
        {
          utcOffsetIdx = i;
          prefsRequestSave(SAVE_SETTINGS);
          webNeedRedraw = true;
          break;
        }
      }
    }
    request->send(200, "application/json", "{\"status\":\"ok\",\"time\":\"" + String((clockGet() ? clockGet() : "--:--")) + "\",\"offset\":\"" + String(utcOffsets[utcOffsetIdx].desc) + "\"}");
  });

  // REST API: Seek (dispatched safely to main loop to prevent I2C/TFT race conditions)
  server.on("/api/seek", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    int dir = request->hasParam("dir") ? request->getParam("dir")->value().toInt() : 1;
    webPendingSeek = dir;
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  // REST API: Bands List
  server.on("/api/bands", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    String json = "[";
    for(int i = 0; i < getTotalBands(); i++)
    {
      if(i > 0) json += ",";
      json += "{\"idx\":" + String(i) + ",";
      json += "\"name\":\"" + String(bands[i].bandName) + "\",";
      json += "\"mode\":\"" + String(bandModeDesc[bands[i].bandMode]) + "\",";
      json += "\"min\":" + String(bands[i].minimumFreq) + ",";
      json += "\"max\":" + String(bands[i].maximumFreq) + "}";
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  // REST API: Memories List
  server.on("/api/memories", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    String json = "[";
    bool first = true;
    for(int i = 0; i < getTotalMemories(); i++)
    {
      if(memories[i].freq > 0)
      {
        if(!first) json += ",";
        first = false;
        json += "{\"slot\":" + String(i + 1) + ",";
        json += "\"band\":\"" + String(bands[memories[i].band].bandName) + "\",";
        json += "\"freq\":" + String(memories[i].freq) + ",";
        json += "\"mode\":\"" + String(bandModeDesc[memories[i].mode]) + "\",";
        char nameBuf[12];
        memset(nameBuf, 0, sizeof(nameBuf));
        strncpy(nameBuf, memories[i].name, sizeof(nameBuf) - 1);
        json += "\"name\":\"" + String(nameBuf) + "\"}";
      }
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  // REST API: Memory Tune
  server.on("/api/memory_tune", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    if(request->hasParam("slot"))
    {
      int slot = request->getParam("slot")->value().toInt();
      if(slot >= 1 && slot <= getTotalMemories())
      {
        if(memories[slot - 1].freq > 0)
        {
          tuneToMemory(&memories[slot - 1]);
          prefsRequestSave(SAVE_CUR_BAND);
          webNeedRedraw = true;
          request->send(200, "application/json", "{\"status\":\"ok\"}");
          return;
        }
      }
    }
    request->send(400, "application/json", "{\"status\":\"error\"}");
  });

  // REST API: Memory Save
  server.on("/api/memory_save", HTTP_POST, [] (AsyncWebServerRequest *request) {
    int slot = 1;
    if(request->hasParam("slot", true))
      slot = request->getParam("slot", true)->value().toInt();
    else if(request->hasParam("slot"))
      slot = request->getParam("slot")->value().toInt();

    if(slot >= 1 && slot <= getTotalMemories())
    {
      uint32_t freqHz = (currentMode == FM) ? (currentFrequency * 10000) : (currentFrequency * 1000 + currentBFO);
      memories[slot - 1].freq = freqHz;
      memories[slot - 1].band = bandIdx;
      memories[slot - 1].mode = currentMode;
      memset(memories[slot - 1].name, 0, sizeof(memories[slot - 1].name));

      String name = "";
      if(request->hasParam("name", true))
        name = request->getParam("name", true)->value();
      else if(request->hasParam("name"))
        name = request->getParam("name")->value();

      if(name.length() > 0)
        strncpy(memories[slot - 1].name, name.c_str(), sizeof(memories[slot - 1].name) - 1);

      prefsRequestSave(SAVE_MEMORIES);
      request->send(200, "application/json", "{\"status\":\"ok\"}");
      return;
    }
    request->send(400, "application/json", "{\"status\":\"error\"}");
  });

  // REST API: Memory Delete
  server.on("/api/memory_delete", HTTP_POST, [] (AsyncWebServerRequest *request) {
    int slot = 1;
    if(request->hasParam("slot", true))
      slot = request->getParam("slot", true)->value().toInt();
    else if(request->hasParam("slot"))
      slot = request->getParam("slot")->value().toInt();

    if(slot >= 1 && slot <= getTotalMemories())
    {
      memories[slot - 1].freq = 0;
      prefsRequestSave(SAVE_MEMORIES);
      request->send(200, "application/json", "{\"status\":\"ok\"}");
      return;
    }
    request->send(400, "application/json", "{\"status\":\"error\"}");
  });

  // REST API: Wi-Fi Scan
  server.on("/api/wifi_scan", HTTP_GET, [] (AsyncWebServerRequest *request) {
    int n = WiFi.scanNetworks(false, false);
    String json = "[";
    if(n > 0)
    {
      for(int i = 0; i < n; i++)
      {
        if(i > 0) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"secure\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "false" : "true") + "}";
      }
    }
    json += "]";
    WiFi.scanDelete();
    request->send(200, "application/json", json);
  });

  // REST API: Wi-Fi Save & Connect
  server.on("/api/wifi_save", HTTP_POST, [] (AsyncWebServerRequest *request) {
    String ssid = "";
    String password = "";

    if(request->hasParam("ssid", true))
      ssid = request->getParam("ssid", true)->value();
    else if(request->hasParam("ssid"))
      ssid = request->getParam("ssid")->value();

    if(request->hasParam("password", true))
      password = request->getParam("password", true)->value();
    else if(request->hasParam("password"))
      password = request->getParam("password")->value();

    if(ssid.length() > 0)
    {
      prefs.begin("network", false, STORAGE_PARTITION);
      prefs.putString("wifissid1", ssid);
      prefs.putString("wifipass1", password);
      prefs.end();

      wifiModeIdx = NET_AP_CONNECT;
      prefsRequestSave(SAVE_SETTINGS, true);

      netRequestConnect();
      request->send(200, "application/json", "{\"status\":\"ok\"}");
      return;
    }
    request->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"No SSID\"}");
  });

  // Legacy pages
  server.on("/status", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/html", webRadioPage());
  });

  server.on("/memory", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/html", webMemoryPage());
  });

  server.on("/config", HTTP_ANY, [] (AsyncWebServerRequest *request) {
    if(!webIsAuthenticated(request)) return request->requestAuthentication();
    request->send(200, "text/html", webConfigPage());
  });

  server.on("/splash.png", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!LittleFS.exists(SPLASH_PATH))
      return request->send(404, "text/plain", "Not found");
    request->send(LittleFS, SPLASH_PATH, "image/png");
  });

  server.onNotFound([] (AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  // This method saves configuration form contents
  server.on("/setconfig", HTTP_POST, webSetConfig, webUploadSplash);

  // Start web server
  server.begin();
}

static void webUploadSplash(AsyncWebServerRequest *request, const String &filename,
                            size_t index, uint8_t *data, size_t len, bool final)
{
  if(!webIsAuthenticated(request)) return;

  if(index == 0)
  {
    LittleFS.remove(SPLASH_TEMP_PATH);

    SplashUploadState *state = static_cast<SplashUploadState *>(calloc(1, sizeof(SplashUploadState)));
    if(!state) return;

    request->_tempObject = state;
    request->onDisconnect([request, state]() {
      if(state->incomplete)
        request->_tempFile.close();

      // Discard an upload that was not installed by webSetConfig().
      LittleFS.remove(SPLASH_TEMP_PATH);
    });

    // The browser filter is only advisory, so enforce the extension here too.
    if(filename.endsWith(".png"))
    {
      request->_tempFile = LittleFS.open(SPLASH_TEMP_PATH, "w");
      state->incomplete = request->_tempFile;
    }
  }

  SplashUploadState *state = static_cast<SplashUploadState *>(request->_tempObject);

  if(request->_tempFile && len)
  {
    if((index + len) > SPLASH_MAX_FILE_SIZE)
    {
      state->tooLarge = true;
      state->incomplete = false;
      request->_tempFile.close();
      LittleFS.remove(SPLASH_TEMP_PATH);
    }
    else if(request->_tempFile.write(data, len) != len)
    {
      state->incomplete = false;
      request->_tempFile.close();
      LittleFS.remove(SPLASH_TEMP_PATH);
    }
  }

  if(final && request->_tempFile)
    request->_tempFile.close();

  if(final && state)
    state->incomplete = false;
}

void webSetConfig(AsyncWebServerRequest *request)
{
  if(!webIsAuthenticated(request)) return request->requestAuthentication();

  uint32_t prefsSave = 0;
  uint32_t epoch;
  bool setClock = false;

  if(request->hasParam("datetime", true))
  {
    String dateTime = request->getParam("datetime", true)->value();
    if(dateTime != "")
    {
      if(!webParseUTCDateTime(dateTime, &epoch))
        return request->send(400, "text/plain", "Date/time must use the YYYY-mm-dd HH:MM:SS format and contain a valid UTC date and time.");
      setClock = true;
    }
  }

  if(request->hasParam("deletesplash", true))
  {
    LittleFS.remove(SPLASH_TEMP_PATH);
    LittleFS.remove(SPLASH_PATH);
  }
  else if(request->hasParam("splash", true, true))
  {
    String filename = request->getParam("splash", true, true)->value();

    if(filename != "")
    {
      SplashUploadState *state = static_cast<SplashUploadState *>(request->_tempObject);
      if(state && state->tooLarge)
        return request->send(413, "text/plain", "The splash image must not exceed 512 KB.");

      if(!filename.endsWith(".png"))
      {
        LittleFS.remove(SPLASH_TEMP_PATH);
        return request->send(400, "text/plain", "The splash image filename must end in .png.");
      }

      if(!LittleFS.exists(SPLASH_TEMP_PATH))
        return request->send(500, "text/plain", "The splash image could not be stored.");

      String error = splashValidate();
      if(error != "")
      {
        LittleFS.remove(SPLASH_TEMP_PATH);
        return request->send(400, "text/plain", error);
      }

      if(!LittleFS.rename(SPLASH_TEMP_PATH, SPLASH_PATH))
      {
        LittleFS.remove(SPLASH_TEMP_PATH);
        return request->send(500, "text/plain", "The splash image could not be installed.");
      }
    }
  }

  // Start modifying preferences
  prefs.begin("network", false, STORAGE_PARTITION);

  // Save user name and password
  if(request->hasParam("username", true) && request->hasParam("password", true))
  {
    loginUsername = request->getParam("username", true)->value();
    loginPassword = request->getParam("password", true)->value();

    prefs.putString("loginusername", loginUsername);
    prefs.putString("loginpassword", loginPassword);
  }

  // Save SSIDs and their passwords
  bool haveSSID = false;
  for(int j=0 ; j<3 ; j++)
  {
    char nameSSID[16], namePASS[16];

    sprintf(nameSSID, "wifissid%d", j+1);
    sprintf(namePASS, "wifipass%d", j+1);

    if(request->hasParam(nameSSID, true) && request->hasParam(namePASS, true))
    {
      String ssid = request->getParam(nameSSID, true)->value();
      String pass = request->getParam(namePASS, true)->value();
      prefs.putString(nameSSID, ssid);
      prefs.putString(namePASS, pass);
      haveSSID |= ssid != "" && pass != "";
    }
  }

  // Save hidden SSID scanning preference
  wifiScanHidden = request->hasParam("wifiscanhidden", true);
  prefs.putBool("wifiscanhidden", wifiScanHidden);

  // Save time zone
  if(request->hasParam("utcoffset", true))
  {
    int idx = request->getParam("utcoffset", true)->value().toInt();
    if(idx >= 0 && idx < getTotalUTCOffsets())
    {
      utcOffsetIdx = idx;
      prefsSave |= SAVE_SETTINGS;
    }
  }

  // Save theme
  if(request->hasParam("theme", true))
  {
    String theme = request->getParam("theme", true)->value();
    themeIdx = theme.toInt();
    prefsSave |= SAVE_SETTINGS;
  }

  // Save scroll direction and menu zoom
  scrollDirection = request->hasParam("scroll", true)? -1 : 1;
  zoomMenu        = request->hasParam("zoom", true);
  prefsSave |= SAVE_SETTINGS;

  // Done with the preferences
  prefs.end();

  // Save preferences immediately
  prefsRequestSave(prefsSave, true);

  if(setClock) clockSetEpoch(epoch);

  // Show config page again
  request->redirect("/config");

  // If we are currently in AP mode, and infrastructure mode requested,
  // and there is at least one SSID / PASS pair, request network connection
  if(haveSSID && (wifiModeIdx>NET_AP_ONLY) && (WiFi.status()!=WL_CONNECTED))
    netRequestConnect();
}

static const String webInputField(const String &name, const String &value, bool pass)
{
  String newValue(value);

  newValue.replace("\"", "&quot;");
  newValue.replace("'", "&apos;");

  return(
    "<INPUT TYPE='" + String(pass? "PASSWORD":"TEXT") + "' NAME='" +
    name + "' VALUE='" + newValue + "'>"
  );
}

static bool webParseUTCDateTime(const String &text, uint32_t *epoch)
{
  int year, month, day, hour, minute, second;
  return(epoch && text.length() == 19 &&
         sscanf(text.c_str(), "%4d-%2d-%2d %2d:%2d:%2d",
                &year, &month, &day, &hour, &minute, &second) == 6 &&
         clockUTCDateTimeToEpoch(year, month, day, hour, minute, second, epoch));
}

static const String webStyleSheet()
{
  return
"BODY"
"{"
  "margin: 0;"
  "padding: 0;"
"}"
"H1"
"{"
  "text-align: center;"
"}"
"TABLE"
"{"
  "width: 100%;"
  "max-width: 768px;"
  "border: 0px;"
  "margin-left: auto;"
  "margin-right: auto;"
"}"
"TH, TD"
"{"
  "padding: 0.5em;"
"}"
"TH.HEADING"
"{"
  "background-color: #80A0FF;"
  "column-span: all;"
  "text-align: center;"
"}"
"TD.LABEL"
"{"
  "text-align: right;"
"}"
"INPUT[type=text], INPUT[type=password], SELECT"
"{"
  "width: 95%;"
  "padding: 0.5em;"
"}"
"INPUT[type=submit]"
"{"
  "width: 50%;"
  "padding: 0.5em 0;"
"}"
".CENTER"
"{"
  "text-align: center;"
"}"
;
}

static const String webPage(const String &body)
{
  return
"<!DOCTYPE HTML>"
"<HTML>"
"<HEAD>"
  "<META CHARSET='UTF-8'>"
  "<META NAME='viewport' CONTENT='width=device-width, initial-scale=1.0'>"
  "<TITLE>ATS-Mini Config</TITLE>"
  "<STYLE>" + webStyleSheet() + "</STYLE>"
"</HEAD>"
"<BODY STYLE='font-family: sans-serif;'>" + body + "</BODY>"
"</HTML>"
;
}

static const String webUtcOffsetSelector()
{
  String result = "";

  for(int i=0 ; i<getTotalUTCOffsets(); i++)
  {
    char text[96];

    sprintf(text,
      "<OPTION VALUE='%d' DATA-MINUTES='%d'%s>%s</OPTION>",
      i, utcOffsets[i].offset * 15, utcOffsetIdx==i? " SELECTED":"",
      utcOffsets[i].desc
    );

    result += text;
  }

  return(result);
}

static const String webThemeSelector()
{
  String result = "";

  for(int i=0 ; i<getTotalThemes(); i++)
  {
    char text[64];

    sprintf(text,
      "<OPTION VALUE='%d'%s>%s</OPTION>",
       i, themeIdx==i? " SELECTED":"", theme[i].name
    );

    result += text;
  }

  return(result);
}

static const String webRadioPage()
{
  String ip = "";
  String ssid = "";
  String receiverTime = "Not synchronized";
  int offsetMinutes = getCurrentUTCOffset() * 15;
  int offsetMagnitude = abs(offsetMinutes);
  char utcOffset[10];
  snprintf(utcOffset, sizeof(utcOffset), "UTC%c%02d:%02d",
           offsetMinutes < 0? '-' : '+', offsetMagnitude / 60, offsetMagnitude % 60);
  String freq = currentMode == FM?
    String(currentFrequency / 100.0) + "MHz "
  : String(currentFrequency + currentBFO / 1000.0) + "kHz ";

  if(clockAvailable())
  {
    time_t localTime = time(NULL) + offsetMinutes * 60;
    struct tm fields;
    gmtime_r(&localTime, &fields);
    char text[20];

    strftime(text, sizeof(text),
             clockGetDate(NULL, NULL, NULL, NULL)? "%Y-%m-%d %H:%M:%S" : "%H:%M:%S",
             &fields);

    receiverTime = text;
  }

  receiverTime += " (" + String(utcOffset) + ")";

  if(WiFi.status()==WL_CONNECTED)
  {
    ip = WiFi.localIP().toString();
    ssid = WiFi.SSID();
  }
  else
  {
    ip = WiFi.softAPIP().toString();
    ssid = String(apSSID);
  }

  return webPage(
"<H1>ATS-Mini Pocket Receiver</H1>"
"<P ALIGN='CENTER'>"
  "<A HREF='/memory'>Memory</A>&nbsp;|&nbsp;<A HREF='/config'>Config</A>"
"</P>"
"<TABLE COLUMNS=2>"
"<TR>"
  "<TD CLASS='LABEL'>IP Address</TD>"
  "<TD><A HREF='http://" + ip + "'>" + ip + "</A> (" + ssid + ")</TD>"
"</TR>"
"<TR>"
  "<TD CLASS='LABEL'>MAC Address</TD>"
  "<TD>" + String(getMACAddress()) + "</TD>"
"</TR>"
"<TR>"
  "<TD CLASS='LABEL'>Firmware</TD>"
  "<TD>" + String(getVersion(true)) + "</TD>"
"</TR>"
"<TR>"
  "<TD CLASS='LABEL'>Date/Time</TD>"
  "<TD>" + receiverTime + "</TD>"
"</TR>"
"<TR>"
  "<TD CLASS='LABEL'>Band</TD>"
  "<TD>" + String(getCurrentBand()->bandName) + "</TD>"
"</TR>"
"<TR>"
  "<TD CLASS='LABEL'>Frequency</TD>"
  "<TD>" + freq + String(bandModeDesc[currentMode]) + "</TD>"
"</TR>"
"<TR>"
  "<TD CLASS='LABEL'>Signal Strength</TD>"
  "<TD>" + String(rssi) + "dBuV</TD>"
"</TR>"
"<TR>"
  "<TD CLASS='LABEL'>Signal to Noise</TD>"
  "<TD>" + String(snr) + "dB</TD>"
"</TR>"
"<TR>"
  "<TD CLASS='LABEL'>Battery Voltage</TD>"
  "<TD>" + String(batteryMonitor()) + "V</TD>"
"</TR>"
"</TABLE>"
);
}

static const String webMemoryPage()
{
  String items = "";

  for(int j=0 ; j<MEMORY_COUNT ; j++)
  {
    char text[64];
    sprintf(text, "<TR><TD CLASS='LABEL' WIDTH='10%%'>%02d</TD><TD>", j+1);
    items += text;

    if(!memories[j].freq)
      items += "&nbsp;---&nbsp;</TD></TR>";
    else
    {
      String freq = memories[j].mode == FM?
        String(memories[j].freq / 1000000.0) + "MHz "
      : String(memories[j].freq / 1000.0) + "kHz ";
      items += freq + bandModeDesc[memories[j].mode] + "</TD></TR>";
    }
  }

  return webPage(
"<H1>ATS-Mini Pocket Receiver Memory</H1>"
"<P ALIGN='CENTER'>"
  "<A HREF='/'>📻 Web Remote</A>&nbsp;|&nbsp;<A HREF='/config'>Config</A>"
"</P>"
"<TABLE COLUMNS=2>" + items + "</TABLE>"
);
}

const String webConfigPage()
{
  prefs.begin("network", true, STORAGE_PARTITION);
  String ssid1 = prefs.getString("wifissid1", "");
  String pass1 = prefs.getString("wifipass1", "");
  String ssid2 = prefs.getString("wifissid2", "");
  String pass2 = prefs.getString("wifipass2", "");
  String ssid3 = prefs.getString("wifissid3", "");
  String pass3 = prefs.getString("wifipass3", "");
  bool scanHidden = prefs.getBool("wifiscanhidden", false);
  prefs.end();

  String splashImage = LittleFS.exists(SPLASH_PATH)?
    "<IMG SRC='/splash.png?" + String(millis()) + "' ALT='Current splash screen' STYLE='max-width:100%;height:auto;'>"
  : "Not installed";
  String splashResolution = String(spr.width()) + "x" + String(spr.height());

  return webPage(
"<H1>ATS-Mini Config</H1>"
"<P ALIGN='CENTER'>"
  "<A HREF='/'>📻 Web Remote</A>"
  "&nbsp;|&nbsp;<A HREF='/memory'>Memory</A>"
"</P>"
"<FORM ACTION='/setconfig' METHOD='POST' ENCTYPE='multipart/form-data' ONSUBMIT='browserDateTime(true)'>"
  "<TABLE COLUMNS=2>"
  "<TR><TH COLSPAN=2 CLASS='HEADING'>WiFi Network 1</TH></TR>"
  "<TR>"
    "<TD CLASS='LABEL'>SSID</TD>"
    "<TD>" + webInputField("wifissid1", ssid1) + "</TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Password</TD>"
    "<TD>" + webInputField("wifipass1", pass1, true) + "</TD>"
  "</TR>"
  "<TR><TH COLSPAN=2 CLASS='HEADING'>WiFi Network 2</TH></TR>"
  "<TR>"
    "<TD CLASS='LABEL'>SSID</TD>"
    "<TD>" + webInputField("wifissid2", ssid2) + "</TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Password</TD>"
    "<TD>" + webInputField("wifipass2", pass2, true) + "</TD>"
  "</TR>"
  "<TR><TH COLSPAN=2 CLASS='HEADING'>WiFi Network 3</TH></TR>"
  "<TR>"
    "<TD CLASS='LABEL'>SSID</TD>"
    "<TD>" + webInputField("wifissid3", ssid3) + "</TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Password</TD>"
    "<TD>" + webInputField("wifipass3", pass3, true) + "</TD>"
  "</TR>"
  "<TR><TH COLSPAN=2 CLASS='HEADING'>This Web UI Login Credentials</TH></TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Username</TD>"
    "<TD>" + webInputField("username", loginUsername) + "</TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Password</TD>"
    "<TD>" + webInputField("password", loginPassword, true) + "</TD>"
  "</TR>"
  "<TR><TH COLSPAN=2 CLASS='HEADING'>Settings</TH></TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Scan Hidden SSIDs</TD>"
    "<TD><INPUT TYPE='CHECKBOX' NAME='wifiscanhidden' VALUE='on'" +
    (scanHidden? " CHECKED ":"") + "></TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Use Browser Date/Time</TD>"
    "<TD><INPUT TYPE='CHECKBOX' ID='browserdatetime' ONCHANGE='browserDateTime()'></TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>UTC Date/Time</TD>"
    "<TD><INPUT TYPE='TEXT' ID='datetime' NAME='datetime' PLACEHOLDER='YYYY-mm-dd HH:MM:SS'></TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Time Zone</TD>"
    "<TD>"
      "<SELECT ID='utcoffset' NAME='utcoffset'>" + webUtcOffsetSelector() + "</SELECT>"
    "</TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Theme</TD>"
    "<TD>"
      "<SELECT NAME='theme'>" + webThemeSelector() + "</SELECT>"
    "</TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Reverse Scrolling</TD>"
    "<TD><INPUT TYPE='CHECKBOX' NAME='scroll' VALUE='on'" +
    (scrollDirection<0? " CHECKED ":"") + "></TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Zoomed Menu</TD>"
    "<TD><INPUT TYPE='CHECKBOX' NAME='zoom' VALUE='on'" +
    (zoomMenu? " CHECKED ":"") + "></TD>"
  "</TR>"
  "<TR><TH COLSPAN=2 CLASS='HEADING'>Splash Screen</TH></TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Current Image</TD>"
    "<TD>" + splashImage + "</TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Upload PNG</TD>"
    "<TD><INPUT TYPE='FILE' NAME='splash' ACCEPT='.png'>"
    "<BR><SMALL>Required resolution: " + splashResolution + " pixels; maximum size: 512 KB</SMALL></TD>"
  "</TR>"
  "<TR>"
    "<TD CLASS='LABEL'>Delete Image</TD>"
    "<TD><INPUT TYPE='CHECKBOX' NAME='deletesplash' VALUE='on'></TD>"
  "</TR>"
  "<TR><TH COLSPAN=2 CLASS='HEADING'>"
    "<INPUT TYPE='SUBMIT' VALUE='Save'>"
  "</TH></TR>"
  "</TABLE>"
"</FORM>"
"<SCRIPT>"
"function browserDateTime(submit)"
"{"
  "const enabled=document.getElementById('browserdatetime').checked;"
  "const dateTime=document.getElementById('datetime');"
  "const utcOffset=document.getElementById('utcoffset');"
  "if(enabled)"
  "{"
    "const now=new Date();"
    "dateTime.value=now.toISOString().slice(0,19).replace('T',' ');"
    "const minutes=-now.getTimezoneOffset();"
    "for(const option of utcOffset.options)"
      "if(Number(option.dataset.minutes)===minutes) utcOffset.value=option.value;"
  "}"
  "dateTime.disabled=utcOffset.disabled=enabled&&!submit;"
"}"
"</SCRIPT>"
);
}
