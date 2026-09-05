/**
 * =========================================================================================
 * Project: M5Stack Core2 (ESP32-D0WDQ6-V3, 16MB Flash, 8MB PSRAM, 320x240 Touch LCD)
 * Dedicated Internet Radio with NS4168 Built-in Speaker, AXP192 PMIC & Web Remote
 * =========================================================================================
 */

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <Preferences.h>
#include <vector>
#include <time.h>
#include <esp_http_server.h>
#include <esp_wifi.h>
#include "Audio.h"
#include "stations_db.h"

// -----------------------------------------------------------------------------
// Preferences & Persistent Wi-Fi Configuration
// -----------------------------------------------------------------------------
Preferences prefs;
String currentSSID = "";
String currentPass = "";
httpd_handle_t webHttpdServer = NULL;
bool webServerStarted = false;

// NTP Time Configuration (UTC+5:30 IST Default)
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;
const char* ntpServer = "pool.ntp.org";

// -----------------------------------------------------------------------------
// Dynamic Runtime Stations Database
// -----------------------------------------------------------------------------
struct LiveStation {
    String name;
    String state;
    String language;
    String url;
    bool isCustom;
};

std::vector<LiveStation> runtimeStations;

// -----------------------------------------------------------------------------
// Audio & State Management
// -----------------------------------------------------------------------------
Audio audio;
int currentVolume = 16; // Range: 0 - 21 (Default 16 for M5Core2 1W speaker)
int prevVolume = 16;
bool isMuted = false;
bool isPlaying = false;
bool isBuffering = false;
String currentStreamTitle = "";
volatile bool uiNeedsUpdate = true;
bool isReconnectingWiFi = false;
uint32_t wifiConnectStartTime = 0;

// 10-Attempt Stream Connection Retry Engine State
const int MAX_STREAM_RETRIES = 10;
int streamRetryCount = 0;
bool isRetryingStream = false;
uint32_t nextStreamRetryMs = 0;
String activeTargetUrl = "";
bool streamEstablished = false;

// Active Category, Filter & Pagination State
enum CategoryType { CAT_ALL = 0, CAT_LANG = 1, CAT_STATE = 2, CAT_USER = 3 };
CategoryType activeCatType = CAT_ALL;
String activeFilterVal = "All";

std::vector<int> filteredIndices; // Matching runtimeStations indices
int currentFilterPosition = 0;   // Index within filteredIndices

#define STATIONS_PER_PAGE 4
int stationCurrentPage = 0;

// Auto-On Alarm & Play Duration State
bool timerEnabled = false;
int timerHour = 6;
int timerMin = 0;
int timerDuration = 30; // 0 = continuous, 15, 30, 45, 60, 90, 120 minutes
int lastTimerTriggerDay = -1;
bool alarmActivePlaying = false;
uint32_t alarmAutoOffExpiryMs = 0;

// Active UI Screen Tab (0: Now Playing, 1: Stations, 2: Alarm Timer)
int activeTab = 0;

// Alarm editing variables
bool edit_timer_en = false;
int edit_timer_h = 6;
int edit_timer_m = 0;
int edit_timer_dur_idx = 1;
static const int PRESET_DURATIONS[] = {15, 30, 45, 60, 90, 120, 0};
static const char* PRESET_DURATION_LABELS[] = {"15 min", "30 min", "45 min", "60 min", "90 min", "120 min", "Contin."};
static const int NUM_PRESET_DURATIONS = 7;

// Forward Declarations
void loadSavedSettings();
void loadTimerSettings();
void saveTimerSettings(bool en, int h, int m, int dur);
void checkAutoOnTimer();
void initStationDatabase();
void loadCustomStations();
void saveCustomStation(const String& name, const String& url, const String& state, const String& lang, int editIdx = -1);
void deleteCustomStation(int customIdx);
bool initWiFi();
void setupWebServer();
void applyCategoryFilter(CategoryType cType, const char* filterVal);
void playCurrentStation();
void playStationByFilterIndex(int filterIdx);
void drawFullUI();

// Audio Status Callback
void audio_msg_handler(Audio::msg_t m) {
    if (m.e == Audio::evt_streamtitle && m.msg && strlen(m.msg) > 0) {
        currentStreamTitle = String(m.msg);
        currentStreamTitle.trim();
        uiNeedsUpdate = true;
    }
}

// -----------------------------------------------------------------------------
// Station Database
// -----------------------------------------------------------------------------
void initStationDatabase() {
    runtimeStations.clear();
    runtimeStations.reserve(TOTAL_ALL_STATIONS + 30);

    for (int i = 0; i < TOTAL_ALL_STATIONS; i++) {
        LiveStation st;
        st.name = String(ALL_STATIONS[i].name);
        st.state = String(ALL_STATIONS[i].state);
        st.language = String(ALL_STATIONS[i].language);
        st.url = String(ALL_STATIONS[i].url);
        st.isCustom = false;
        runtimeStations.push_back(st);
    }
    loadCustomStations();
    Serial.printf("[STATIONS DB] Initialized %d total stations\n", (int)runtimeStations.size());
}

void loadCustomStations() {
    prefs.begin("cust_st", true);
    int count = prefs.getInt("count", 0);
    for (int i = 0; i < count; i++) {
        String key_n = "n_" + String(i);
        String key_u = "u_" + String(i);
        String key_s = "s_" + String(i);
        String key_l = "l_" + String(i);
        String sName = prefs.getString(key_n.c_str(), "");
        String sUrl  = prefs.getString(key_u.c_str(), "");
        String sState = prefs.getString(key_s.c_str(), "Custom");
        String sLang  = prefs.getString(key_l.c_str(), "Custom");
        if (sName.length() > 0 && sUrl.length() > 0) {
            LiveStation st;
            st.name = sName; st.url = sUrl; st.state = sState; st.language = sLang; st.isCustom = true;
            runtimeStations.push_back(st);
        }
    }
    prefs.end();
}

void saveCustomStation(const String& name, const String& url, const String& state, const String& lang, int editIdx) {
    if (name.length() == 0 || url.length() == 0) return;
    prefs.begin("cust_st", false);
    int count = prefs.getInt("count", 0);
    String stState = state.length() > 0 ? state : "Custom";
    String stLang  = lang.length() > 0 ? lang : "Custom";

    if (editIdx >= 0 && editIdx < count) {
        prefs.putString(("n_" + String(editIdx)).c_str(), name);
        prefs.putString(("u_" + String(editIdx)).c_str(), url);
        prefs.putString(("s_" + String(editIdx)).c_str(), stState);
        prefs.putString(("l_" + String(editIdx)).c_str(), stLang);
    } else {
        prefs.putString(("n_" + String(count)).c_str(), name);
        prefs.putString(("u_" + String(count)).c_str(), url);
        prefs.putString(("s_" + String(count)).c_str(), stState);
        prefs.putString(("l_" + String(count)).c_str(), stLang);
        prefs.putInt("count", count + 1);
    }
    prefs.end();
    initStationDatabase();
    applyCategoryFilter(activeCatType, activeFilterVal.c_str());
}

void deleteCustomStation(int customIdx) {
    prefs.begin("cust_st", true);
    int count = prefs.getInt("count", 0);
    std::vector<LiveStation> tempCustoms;
    for (int i = 0; i < count; i++) {
        if (i != customIdx) {
            String sName = prefs.getString(("n_" + String(i)).c_str(), "");
            String sUrl  = prefs.getString(("u_" + String(i)).c_str(), "");
            String sState = prefs.getString(("s_" + String(i)).c_str(), "Custom");
            String sLang  = prefs.getString(("l_" + String(i)).c_str(), "Custom");
            if (sName.length() > 0 && sUrl.length() > 0) {
                LiveStation st;
                st.name = sName; st.url = sUrl; st.state = sState; st.language = sLang; st.isCustom = true;
                tempCustoms.push_back(st);
            }
        }
    }
    prefs.end();

    prefs.begin("cust_st", false);
    prefs.clear();
    for (size_t i = 0; i < tempCustoms.size(); i++) {
        prefs.putString(("n_" + String(i)).c_str(), tempCustoms[i].name);
        prefs.putString(("u_" + String(i)).c_str(), tempCustoms[i].url);
        prefs.putString(("s_" + String(i)).c_str(), tempCustoms[i].state);
        prefs.putString(("l_" + String(i)).c_str(), tempCustoms[i].language);
    }
    prefs.putInt("count", tempCustoms.size());
    prefs.end();

    initStationDatabase();
    applyCategoryFilter(activeCatType, activeFilterVal.c_str());
}

// -----------------------------------------------------------------------------
// Playback Engine
// -----------------------------------------------------------------------------
void applyCategoryFilter(CategoryType cType, const char* filterVal) {
    activeCatType = cType;
    activeFilterVal = String(filterVal);
    stationCurrentPage = 0;
    filteredIndices.clear();

    for (size_t i = 0; i < runtimeStations.size(); i++) {
        if (cType == CAT_ALL || activeFilterVal == "All") {
            filteredIndices.push_back(i);
        } else if (cType == CAT_USER || activeFilterVal == "User") {
            if (runtimeStations[i].isCustom) filteredIndices.push_back(i);
        } else if (cType == CAT_LANG && runtimeStations[i].language == filterVal) {
            filteredIndices.push_back(i);
        } else if (cType == CAT_STATE && runtimeStations[i].state == filterVal) {
            filteredIndices.push_back(i);
        }
    }

    if (filteredIndices.empty()) {
        for (size_t i = 0; i < runtimeStations.size(); i++) filteredIndices.push_back(i);
    }
    currentFilterPosition = 0;
    uiNeedsUpdate = true;
}

void playStationByFilterIndex(int filterIdx) {
    if (filteredIndices.empty()) return;
    if (filterIdx < 0) filterIdx = filteredIndices.size() - 1;
    if (filterIdx >= (int)filteredIndices.size()) filterIdx = 0;

    currentFilterPosition = filterIdx;
    playCurrentStation();
}

void playCurrentStation() {
    if (filteredIndices.empty()) return;

    int realIdx = filteredIndices[currentFilterPosition];
    const LiveStation& st = runtimeStations[realIdx];

    Serial.printf("\n[RADIO] Switching to [%d/%d]: %s\n", currentFilterPosition + 1, (int)filteredIndices.size(), st.name.c_str());

    prefs.begin("air_radio", false);
    prefs.putInt("last_st", realIdx);
    prefs.end();

    activeTargetUrl = st.url;
    streamRetryCount = 0;
    isRetryingStream = false;
    streamEstablished = false;

    isPlaying = true;
    isBuffering = true;
    currentStreamTitle = "Connecting...";
    uiNeedsUpdate = true;

    audio.stopSong();
    audio.connecttohost(st.url.c_str());
}

void setSystemVolume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 21) vol = 21;
    currentVolume = vol;
    isMuted = (currentVolume == 0);
    audio.setVolume(currentVolume);

    prefs.begin("air_radio", false);
    prefs.putInt("last_vol", currentVolume);
    prefs.end();
    uiNeedsUpdate = true;
}

void togglePlayPause() {
    if (isPlaying) {
        audio.stopSong();
        isPlaying = false;
        isBuffering = false;
        currentStreamTitle = "Paused";
    } else {
        playCurrentStation();
    }
    uiNeedsUpdate = true;
}

// -----------------------------------------------------------------------------
// Settings & Timer
// -----------------------------------------------------------------------------
void loadSavedSettings() {
    prefs.begin("air_radio", true);
    currentSSID = prefs.getString("wifi_ssid", "suresh2.4gExt");
    currentPass = prefs.getString("wifi_pass", "alangium");
    int savedStation = prefs.getInt("last_st", 0);
    int savedVol = prefs.getInt("last_vol", 16);
    prefs.end();

    if (savedVol >= 0 && savedVol <= 21) currentVolume = savedVol;
    if (savedStation >= 0 && savedStation < (int)runtimeStations.size()) currentFilterPosition = savedStation;

    loadTimerSettings();
}

void loadTimerSettings() {
    prefs.begin("radio_timer", true);
    timerEnabled = prefs.getBool("en", false);
    timerHour = prefs.getInt("h", 6);
    timerMin = prefs.getInt("m", 0);
    timerDuration = prefs.getInt("dur", 30);
    prefs.end();
}

void saveTimerSettings(bool en, int h, int m, int dur) {
    prefs.begin("radio_timer", false);
    prefs.putBool("en", en);
    prefs.putInt("h", h);
    prefs.putInt("m", m);
    prefs.putInt("dur", dur);
    prefs.end();
    timerEnabled = en;
    timerHour = h;
    timerMin = m;
    timerDuration = dur;
}

void checkAutoOnTimer() {
    if (alarmActivePlaying && (timerDuration > 0)) {
        if (millis() >= alarmAutoOffExpiryMs) {
            Serial.println("[TIMER] Auto-off duration expired! Stopping playback...");
            alarmActivePlaying = false;
            if (isPlaying) {
                audio.stopSong();
                isPlaying = false;
                isBuffering = false;
            }
            uiNeedsUpdate = true;
            return;
        }
    }

    if (!timerEnabled) return;
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2020 - 1900)) return;

    if (timeinfo.tm_hour == timerHour && timeinfo.tm_min == timerMin) {
        if (lastTimerTriggerDay != timeinfo.tm_yday) {
            lastTimerTriggerDay = timeinfo.tm_yday;
            Serial.printf("[TIMER ALARM] Triggering Alarm at %02d:%02d!\n", timerHour, timerMin);
            if (isMuted) setSystemVolume(16);
            if (!isPlaying) playCurrentStation();
            if (timerDuration > 0) {
                alarmActivePlaying = true;
                alarmAutoOffExpiryMs = millis() + ((uint32_t)timerDuration * 60000UL);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Wi-Fi Engine
// -----------------------------------------------------------------------------
bool initWiFi() {
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);

    String candidateSSIDs[2];
    String candidatePasss[2];
    int numCandidates = 1;
    candidateSSIDs[0] = currentSSID;
    candidatePasss[0] = currentPass;

    if (currentSSID == "suresh2.4gExt") {
        candidateSSIDs[1] = "suresh";
        candidatePasss[1] = currentPass;
        numCandidates = 2;
    } else if (currentSSID == "suresh") {
        candidateSSIDs[1] = "suresh2.4gExt";
        candidatePasss[1] = currentPass;
        numCandidates = 2;
    }

    for (int cand = 0; cand < numCandidates; cand++) {
        for (int attempt = 1; attempt <= 2; attempt++) {
            Serial.printf("[WIFI] Connecting to '%s' (Attempt %d/2) ...\n", candidateSSIDs[cand].c_str(), attempt);
            WiFi.begin(candidateSSIDs[cand].c_str(), candidatePasss[cand].c_str());

            int timeout = 0;
            while (WiFi.status() != WL_CONNECTED && timeout < 30) {
                delay(500);
                Serial.print(".");
                timeout++;
            }

            if (WiFi.status() == WL_CONNECTED) {
                WiFi.setSleep(false);
                esp_wifi_set_ps(WIFI_PS_NONE);
                currentSSID = candidateSSIDs[cand];
                currentPass = candidatePasss[cand];
                Serial.printf("\n[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
                setupWebServer();
                return true;
            }
            Serial.println("\n[WIFI] Attempt failed.");
            WiFi.disconnect(false, false);
            delay(200);
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// Web Remote (esp_http_server)
// -----------------------------------------------------------------------------
enum WebAction { ACT_NONE = 0, ACT_PLAY_PAUSE = 1, ACT_PREV = 2, ACT_NEXT = 3, ACT_PLAY_STATION = 4 };
volatile WebAction pendingWebAction = ACT_NONE;
volatile int pendingStationId = -1;
volatile int pendingVol = -1;

static esp_err_t http_root_handler(httpd_req_t *req) {
    extern const char PAGE_INDEX[] PROGMEM;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=3600");
    return httpd_resp_send(req, PAGE_INDEX, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_status_handler(httpd_req_t *req) {
    char json[320];
    String sTitle = currentStreamTitle;
    if (sTitle.length() == 0) sTitle = isPlaying ? "Live Stream" : "Stopped";
    sTitle.replace("\"", "\\\"");

    String stName = "Radio", stState = "India", stLang = "AIR";
    if (!filteredIndices.empty()) {
        int rIdx = filteredIndices[currentFilterPosition];
        stName = runtimeStations[rIdx].name; stName.replace("\"", "\\\"");
        stState = runtimeStations[rIdx].state; stState.replace("\"", "\\\"");
        stLang = runtimeStations[rIdx].language; stLang.replace("\"", "\\\"");
    }

    int bat = M5.Power.getBatteryLevel();
    bool chg = M5.Power.isCharging();

    snprintf(json, sizeof(json),
             "{\"playing\":%s,\"buffering\":%s,\"source\":\"radio\",\"title\":\"%s\",\"state\":\"%s\",\"lang\":\"%s\",\"vol\":%d,\"muted\":%s,\"bat\":%d,\"charging\":%s,\"total\":%d}",
             isPlaying ? "true" : "false", isBuffering ? "true" : "false",
             stName.c_str(), stState.c_str(), stLang.c_str(),
             currentVolume, isMuted ? "true" : "false", bat, chg ? "true" : "false", (int)runtimeStations.size());

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_cmd_handler(httpd_req_t *req) {
    char buf[64];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[32];
        if (httpd_query_key_value(buf, "action", param, sizeof(param)) == ESP_OK) {
            if (strcmp(param, "playpause") == 0) pendingWebAction = ACT_PLAY_PAUSE;
            else if (strcmp(param, "prev") == 0) pendingWebAction = ACT_PREV;
            else if (strcmp(param, "next") == 0) pendingWebAction = ACT_NEXT;
            else if (strcmp(param, "play") == 0) {
                char id_str[16];
                if (httpd_query_key_value(buf, "id", id_str, sizeof(id_str)) == ESP_OK) {
                    pendingStationId = atoi(id_str);
                    pendingWebAction = ACT_PLAY_STATION;
                }
            }
        }
        if (httpd_query_key_value(buf, "vol", param, sizeof(param)) == ESP_OK) {
            pendingVol = atoi(param);
        }
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static const httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = http_root_handler, .user_ctx = NULL };
static const httpd_uri_t uri_root_head = { .uri = "/", .method = HTTP_HEAD, .handler = http_root_handler, .user_ctx = NULL };
static const httpd_uri_t uri_status = { .uri = "/api/status", .method = HTTP_GET, .handler = http_status_handler, .user_ctx = NULL };
static const httpd_uri_t uri_cmd = { .uri = "/api/cmd", .method = HTTP_GET, .handler = http_cmd_handler, .user_ctx = NULL };

void setupWebServer() {
    if (webServerStarted) return;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 5;
    config.lru_purge_enable = true;

    if (httpd_start(&webHttpdServer, &config) == ESP_OK) {
        httpd_register_uri_handler(webHttpdServer, &uri_root);
        httpd_register_uri_handler(webHttpdServer, &uri_root_head);
        httpd_register_uri_handler(webHttpdServer, &uri_status);
        httpd_register_uri_handler(webHttpdServer, &uri_cmd);
        webServerStarted = true;
        Serial.printf("[WEB REMOTE] Live at http://%s\n", WiFi.localIP().toString().c_str());
    }
}

// -----------------------------------------------------------------------------
// M5GFX Hardware Display UI (320 x 240)
// -----------------------------------------------------------------------------
void drawHeader() {
    M5.Display.fillRect(0, 0, 320, 26, 0x18C3); // Dark slate header
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextColor(0x07FF, 0x18C3); // Cyan
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(1);

    // Wi-Fi info
    if (WiFi.status() == WL_CONNECTED) {
        M5.Display.drawString(WiFi.localIP().toString(), 6, 8);
    } else {
        M5.Display.setTextColor(0xF800, 0x18C3); // Red
        M5.Display.drawString("NO WI-FI", 6, 8);
    }

    // Battery & Charging
    int bat = M5.Power.getBatteryLevel();
    bool chg = M5.Power.isCharging();
    char batBuf[16];
    snprintf(batBuf, sizeof(batBuf), "%s%d%%", chg ? "+" : "", bat);
    M5.Display.setTextDatum(top_right);
    M5.Display.setTextColor(chg ? 0x07E0 : 0xFFE0, 0x18C3);
    M5.Display.drawString(batBuf, 314, 8);
}

void drawBottomBar() {
    M5.Display.fillRect(0, 196, 320, 44, 0x10A2); // Bottom bar

    // Tab buttons: [ Radio ] [ List ] [ Timer ]
    M5.Display.fillRoundRect(6, 202, 98, 32, 6, (activeTab == 0) ? 0x07FF : 0x2124);
    M5.Display.setTextColor((activeTab == 0) ? 0x0000 : 0xFFFF);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("Now Playing", 55, 218);

    M5.Display.fillRoundRect(111, 202, 98, 32, 6, (activeTab == 1) ? 0x07FF : 0x2124);
    M5.Display.setTextColor((activeTab == 1) ? 0x0000 : 0xFFFF);
    M5.Display.drawString("Stations", 160, 218);

    M5.Display.fillRoundRect(216, 202, 98, 32, 6, (activeTab == 2) ? 0x07FF : 0x2124);
    M5.Display.setTextColor((activeTab == 2) ? 0x0000 : 0xFFFF);
    M5.Display.drawString("Alarm", 265, 218);
}

void drawNowPlayingTab() {
    M5.Display.fillRect(0, 26, 320, 170, 0x0841);

    if (filteredIndices.empty()) return;
    int realIdx = filteredIndices[currentFilterPosition];
    const LiveStation& st = runtimeStations[realIdx];

    // 1. Station Name Box
    M5.Display.fillRoundRect(8, 32, 304, 60, 8, 0x18E3);
    M5.Display.setTextColor(0xFFE0); // Gold
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextSize(1);
    M5.Display.drawString(st.name, 160, 50);

    // Badges: Language & State
    M5.Display.setFont(&fonts::Font0);
    char badgeBuf[64];
    snprintf(badgeBuf, sizeof(badgeBuf), "[ %s ]  [ %s ]", st.language.c_str(), st.state.c_str());
    M5.Display.setTextColor(0x07FF); // Cyan
    M5.Display.drawString(badgeBuf, 160, 74);

    // 2. Clock & Date Display
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year >= (2020 - 1900)) {
        int dispH = timeinfo.tm_hour;
        const char* ampm = "AM";
        if (dispH >= 12) {
            ampm = "PM";
            if (dispH > 12) dispH -= 12;
        }
        if (dispH == 0) dispH = 12;

        char clockBuf[32];
        snprintf(clockBuf, sizeof(clockBuf), "%02d:%02d:%02d %s", dispH, timeinfo.tm_min, timeinfo.tm_sec, ampm);
        M5.Display.setTextColor(0xFFFF);
        M5.Display.setFont(&fonts::Font2);
        M5.Display.drawString(clockBuf, 160, 108);

        char dateBuf[32];
        static const char* DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        static const char* MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        snprintf(dateBuf, sizeof(dateBuf), "%s, %02d %s %d", DAYS[timeinfo.tm_wday], timeinfo.tm_mday, MONTHS[timeinfo.tm_mon], timeinfo.tm_year + 1900);
        M5.Display.setTextColor(0x9CD3);
        M5.Display.setFont(&fonts::Font0);
        M5.Display.drawString(dateBuf, 160, 126);
    }

    // 3. Playback Controls Row: [ |<< Prev ]  [ Play/Pause ]  [ Next >>| ]  [ Vol - ] [ Vol + ]
    M5.Display.fillRoundRect(8, 145, 52, 38, 6, 0x2124);
    M5.Display.setTextColor(0xFFFF);
    M5.Display.drawString("|<<", 34, 164);

    uint16_t playColor = isPlaying ? 0x07E0 : 0xF800;
    M5.Display.fillRoundRect(66, 145, 78, 38, 6, playColor);
    M5.Display.setTextColor(0x0000);
    M5.Display.drawString(isPlaying ? "PAUSE" : "PLAY", 105, 164);

    M5.Display.fillRoundRect(150, 145, 52, 38, 6, 0x2124);
    M5.Display.setTextColor(0xFFFF);
    M5.Display.drawString(">>|", 176, 164);

    M5.Display.fillRoundRect(210, 145, 48, 38, 6, 0x2945);
    M5.Display.setTextColor(0xFFFF);
    M5.Display.drawString("VOL-", 234, 164);

    M5.Display.fillRoundRect(264, 145, 48, 38, 6, 0x2945);
    M5.Display.setTextColor(0xFFFF);
    M5.Display.drawString("VOL+", 288, 164);
}

void drawStationsTab() {
    M5.Display.fillRect(0, 26, 320, 170, 0x0841);

    int totalFiltered = filteredIndices.size();
    int totalPages = (totalFiltered + STATIONS_PER_PAGE - 1) / STATIONS_PER_PAGE;
    if (totalPages == 0) totalPages = 1;

    M5.Display.setTextColor(0xFFE0);
    M5.Display.setTextDatum(middle_left);
    M5.Display.drawString("FILTER: " + activeFilterVal, 10, 40);

    M5.Display.setTextDatum(middle_right);
    char pBuf[16];
    snprintf(pBuf, sizeof(pBuf), "%d / %d", stationCurrentPage + 1, totalPages);
    M5.Display.drawString(pBuf, 310, 40);

    int startIdx = stationCurrentPage * STATIONS_PER_PAGE;
    for (int i = 0; i < STATIONS_PER_PAGE; i++) {
        int idx = startIdx + i;
        int y = 56 + (i * 28);
        if (idx < totalFiltered) {
            int realIdx = filteredIndices[idx];
            bool isCur = (idx == currentFilterPosition);
            M5.Display.fillRoundRect(8, y, 304, 25, 4, isCur ? 0x07FF : 0x18E3);
            M5.Display.setTextColor(isCur ? 0x0000 : 0xFFFF);
            M5.Display.setTextDatum(middle_left);
            String stText = runtimeStations[realIdx].name;
            if (stText.length() > 28) stText = stText.substring(0, 26) + "..";
            M5.Display.drawString(stText, 16, y + 12);
        }
    }

    M5.Display.fillRoundRect(8, 170, 148, 24, 4, 0x2124);
    M5.Display.setTextColor(0xFFFF);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("< Prev Page", 82, 182);

    M5.Display.fillRoundRect(164, 170, 148, 24, 4, 0x2124);
    M5.Display.drawString("Next Page >", 238, 182);
}

void drawAlarmTab() {
    M5.Display.fillRect(0, 26, 320, 170, 0x0841);

    uint16_t enCol = edit_timer_en ? 0x07E0 : 0x7BEF;
    M5.Display.fillRoundRect(10, 36, 300, 32, 6, enCol);
    M5.Display.setTextColor(0x0000);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString(edit_timer_en ? "ALARM: ENABLED" : "ALARM: DISABLED", 160, 52);

    int dispH = edit_timer_h;
    const char* ampm = "AM";
    if (dispH >= 12) {
        ampm = "PM";
        if (dispH > 12) dispH -= 12;
    }
    if (dispH == 0) dispH = 12;

    M5.Display.fillRoundRect(10, 78, 36, 32, 4, 0x2124);
    M5.Display.setTextColor(0xFFFF);
    M5.Display.drawString("-", 28, 94);

    char hBuf[8];
    snprintf(hBuf, sizeof(hBuf), "%02d", dispH);
    M5.Display.setTextColor(0xFFE0);
    M5.Display.drawString(hBuf, 62, 94);

    M5.Display.fillRoundRect(80, 78, 36, 32, 4, 0x2124);
    M5.Display.setTextColor(0xFFFF);
    M5.Display.drawString("+", 98, 94);

    M5.Display.drawString(":", 125, 94);

    M5.Display.fillRoundRect(136, 78, 36, 32, 4, 0x2124);
    M5.Display.drawString("-", 154, 94);

    char mBuf[8];
    snprintf(mBuf, sizeof(mBuf), "%02d", edit_timer_m);
    M5.Display.setTextColor(0xFFE0);
    M5.Display.drawString(mBuf, 188, 94);

    M5.Display.fillRoundRect(206, 78, 36, 32, 4, 0x2124);
    M5.Display.setTextColor(0xFFFF);
    M5.Display.drawString("+", 224, 94);

    uint16_t ampmCol = (edit_timer_h >= 12) ? 0xFD20 : 0x07FF;
    M5.Display.fillRoundRect(250, 78, 60, 32, 4, ampmCol);
    M5.Display.setTextColor(0x0000);
    M5.Display.drawString(ampm, 280, 94);

    M5.Display.fillRoundRect(10, 120, 36, 28, 4, 0x2124);
    M5.Display.setTextColor(0xFFFF);
    M5.Display.drawString("<", 28, 134);

    M5.Display.setTextColor(0x07FF);
    M5.Display.drawString("Dur: " + String(PRESET_DURATION_LABELS[edit_timer_dur_idx]), 145, 134);

    M5.Display.fillRoundRect(274, 120, 36, 28, 4, 0x2124);
    M5.Display.setTextColor(0xFFFF);
    M5.Display.drawString(">", 292, 134);

    M5.Display.fillRoundRect(60, 156, 200, 30, 6, 0x07FF);
    M5.Display.setTextColor(0x0000);
    M5.Display.drawString("SAVE ALARM", 160, 171);
}

void drawFullUI() {
    drawHeader();
    if (activeTab == 0) drawNowPlayingTab();
    else if (activeTab == 1) drawStationsTab();
    else if (activeTab == 2) drawAlarmTab();
    drawBottomBar();
}

// -----------------------------------------------------------------------------
// Touch & Hardware Button Handler
// -----------------------------------------------------------------------------
void handleTouchAndButtons() {
    // 1. Check physical capacitive buttons below screen (BtnA, BtnB, BtnC)
    if (M5.BtnA.wasPressed()) {
        playStationByFilterIndex(currentFilterPosition - 1);
        return;
    }
    if (M5.BtnB.wasPressed()) {
        togglePlayPause();
        return;
    }
    if (M5.BtnC.wasPressed()) {
        playStationByFilterIndex(currentFilterPosition + 1);
        return;
    }

    // 2. Check Screen Touch
    auto t = M5.Touch.getDetail();
    if (!t.wasPressed()) return;

    int x = t.x;
    int y = t.y;

    if (y >= 196) {
        if (x < 105) activeTab = 0;
        else if (x < 210) activeTab = 1;
        else activeTab = 2;
        uiNeedsUpdate = true;
        return;
    }

    if (activeTab == 0) {
        if (y >= 145 && y <= 185) {
            if (x >= 8 && x <= 60) playStationByFilterIndex(currentFilterPosition - 1);
            else if (x >= 66 && x <= 144) togglePlayPause();
            else if (x >= 150 && x <= 202) playStationByFilterIndex(currentFilterPosition + 1);
            else if (x >= 210 && x <= 258) setSystemVolume(currentVolume - 1);
            else if (x >= 264 && x <= 312) setSystemVolume(currentVolume + 1);
        }
    } else if (activeTab == 1) {
        for (int i = 0; i < STATIONS_PER_PAGE; i++) {
            int rowY = 56 + (i * 28);
            if (y >= rowY && y <= rowY + 25) {
                int clickedIdx = (stationCurrentPage * STATIONS_PER_PAGE) + i;
                if (clickedIdx < (int)filteredIndices.size()) {
                    currentFilterPosition = clickedIdx;
                    activeTab = 0;
                    playCurrentStation();
                    return;
                }
            }
        }
        if (y >= 170 && y <= 194) {
            int totalPages = (filteredIndices.size() + STATIONS_PER_PAGE - 1) / STATIONS_PER_PAGE;
            if (x < 156) {
                if (stationCurrentPage > 0) stationCurrentPage--;
                else stationCurrentPage = totalPages - 1;
                uiNeedsUpdate = true;
            } else {
                if (stationCurrentPage < totalPages - 1) stationCurrentPage++;
                else stationCurrentPage = 0;
                uiNeedsUpdate = true;
            }
        }
    } else if (activeTab == 2) {
        if (y >= 36 && y <= 68) {
            edit_timer_en = !edit_timer_en;
            uiNeedsUpdate = true;
        } else if (y >= 78 && y <= 110) {
            if (x >= 10 && x <= 46) { edit_timer_h--; if (edit_timer_h < 0) edit_timer_h = 23; uiNeedsUpdate = true; }
            else if (x >= 80 && x <= 116) { edit_timer_h++; if (edit_timer_h > 23) edit_timer_h = 0; uiNeedsUpdate = true; }
            else if (x >= 136 && x <= 172) { edit_timer_m -= 5; if (edit_timer_m < 0) edit_timer_m = 55; uiNeedsUpdate = true; }
            else if (x >= 206 && x <= 242) { edit_timer_m += 5; if (edit_timer_m >= 60) edit_timer_m = 0; uiNeedsUpdate = true; }
            else if (x >= 250 && x <= 310) {
                if (edit_timer_h >= 12) edit_timer_h -= 12;
                else edit_timer_h += 12;
                uiNeedsUpdate = true;
            }
        } else if (y >= 120 && y <= 148) {
            if (x < 46) { edit_timer_dur_idx--; if (edit_timer_dur_idx < 0) edit_timer_dur_idx = NUM_PRESET_DURATIONS - 1; uiNeedsUpdate = true; }
            else if (x > 270) { edit_timer_dur_idx++; if (edit_timer_dur_idx >= NUM_PRESET_DURATIONS) edit_timer_dur_idx = 0; uiNeedsUpdate = true; }
        } else if (y >= 156 && y <= 186) {
            int selDur = PRESET_DURATIONS[edit_timer_dur_idx];
            saveTimerSettings(edit_timer_en, edit_timer_h, edit_timer_m, selDur);
            activeTab = 0;
            uiNeedsUpdate = true;
        }
    }
}

// -----------------------------------------------------------------------------
// Setup & Loop
// -----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println("\n=======================================================");
    Serial.println("  M5Stack Core2 - Multi-Station Internet Radio");
    Serial.println("=======================================================");

    auto cfg = M5.config();
    cfg.internal_spk = false;
    M5.begin(cfg);

    M5.Power.Axp192.setGPIO2(true);
    Serial.println("[POWER] AXP192 GPIO2 enabled: Speaker amplifier powered.");

    M5.Display.setBrightness(128);
    M5.Display.fillScreen(0x0000);

    initStationDatabase();
    loadSavedSettings();

    filteredIndices.clear();
    for (size_t i = 0; i < runtimeStations.size(); i++) filteredIndices.push_back(i);

    Audio::audio_info_callback = audio_msg_handler;
    audio.setPinout(12, 0, 2);
    audio.setVolume(currentVolume);
    audio.setConnectionTimeout(4000, 10000);

    drawFullUI();

    M5.Display.fillRect(10, 80, 300, 40, 0x18C3);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(0x07FF);
    M5.Display.drawString("Connecting to Wi-Fi...", 160, 100);

    bool wifiOk = initWiFi();
    if (wifiOk) {
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov");
        setupWebServer();
        playCurrentStation();
    } else {
        M5.Display.drawString("Wi-Fi Failed. Check settings.", 160, 100);
        delay(2000);
    }
    uiNeedsUpdate = true;
}

void loop() {
    M5.update();
    audio.loop();
    handleTouchAndButtons();

    if (pendingVol != -1) {
        setSystemVolume(pendingVol);
        pendingVol = -1;
    }
    if (pendingWebAction != ACT_NONE) {
        if (pendingWebAction == ACT_PLAY_PAUSE) togglePlayPause();
        else if (pendingWebAction == ACT_PREV) playStationByFilterIndex(currentFilterPosition - 1);
        else if (pendingWebAction == ACT_NEXT) playStationByFilterIndex(currentFilterPosition + 1);
        else if (pendingWebAction == ACT_PLAY_STATION) {
            if (pendingStationId >= 0 && pendingStationId < (int)runtimeStations.size()) {
                for (size_t i = 0; i < filteredIndices.size(); i++) {
                    if (filteredIndices[i] == pendingStationId) { currentFilterPosition = i; break; }
                }
                activeTab = 0;
                playCurrentStation();
            }
        }
        pendingWebAction = ACT_NONE;
    }

    static uint32_t lastTimerCheck = 0;
    if (millis() - lastTimerCheck >= 1000) {
        lastTimerCheck = millis();
        checkAutoOnTimer();
        if (activeTab == 0) {
            drawHeader();
            drawNowPlayingTab();
            drawBottomBar();
        }
    }

    if (uiNeedsUpdate) {
        uiNeedsUpdate = false;
        drawFullUI();
    }
}

// -----------------------------------------------------------------------------
// Web Remote Page HTML
// -----------------------------------------------------------------------------
const char PAGE_INDEX[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>M5Core2 Radio Remote</title>
<style>
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:#0d1117;color:#c9d1d9;margin:0;padding:16px;text-align:center;}
.box{background:#161b22;border:1px solid #30363d;border-radius:12px;padding:16px;margin:0 auto 14px;max-width:420px;box-sizing:border-box;box-shadow:0 4px 12px rgba(0,0,0,0.3);}
h2{margin:2px 0 10px;color:#58a6ff;font-size:20px;}
.title{font-size:22px;font-weight:700;color:#f0883e;margin:8px 0;}
.badge{display:inline-block;padding:3px 10px;background:#21262d;border-radius:12px;font-size:12px;margin:2px;color:#79c0ff;border:1px solid #30363d;}
.btn{background:#238636;color:#fff;border:none;padding:12px 18px;font-size:15px;font-weight:600;border-radius:8px;cursor:pointer;margin:4px;}
.btn:active{opacity:0.75;}
.btn-sec{background:#21262d;color:#c9d1d9;border:1px solid #30363d;}
.btn-danger{background:#da3633;}
.slider{width:80%;margin:16px auto;}
</style>
</head>
<body>
<div class="box">
  <h2>📻 M5Core2 Internet Radio</h2>
  <div id="stName" class="title">Connecting...</div>
  <div><span id="stLang" class="badge">AIR</span><span id="stState" class="badge">India</span></div>
  <p id="stTitle" style="font-size:13px;color:#8b949e;">Live Stream</p>
  <div>
    <button class="btn btn-sec" onclick="cmd('prev')">⏮ Prev</button>
    <button id="playBtn" class="btn" onclick="cmd('playpause')">▶ Play</button>
    <button class="btn btn-sec" onclick="cmd('next')">Next ⏭</button>
  </div>
  <div style="margin-top:14px;">
    <span>🔊 Vol: <span id="volVal">16</span></span><br>
    <input type="range" min="0" max="21" value="16" class="slider" id="volSlider" onchange="setVol(this.value)">
  </div>
  <div style="font-size:12px;color:#8b949e;margin-top:8px;">🔋 Battery: <span id="batVal">--</span></div>
</div>
<script>
function cmd(a){fetch('/api/cmd?action='+a).then(()=>setTimeout(sync,200));}
function setVol(v){fetch('/api/cmd?vol='+v).then(()=>setTimeout(sync,200));}
function sync(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('stName').innerText=d.title;
    document.getElementById('stLang').innerText=d.lang;
    document.getElementById('stState').innerText=d.state;
    document.getElementById('playBtn').innerText=d.playing?'⏸ Pause':'▶ Play';
    document.getElementById('volVal').innerText=d.vol;
    document.getElementById('volSlider').value=d.vol;
    document.getElementById('batVal').innerText=d.bat+'% '+(d.charging?'⚡':'');
  }).catch(()=>{});
}
setInterval(sync,2000);
sync();
</script>
</body>
</html>
)rawliteral";
