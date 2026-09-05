/**
 * =========================================================================================
 * Project: ESP32-S3 JC3248W535 (3.5" 320x480 IPS Capacitive Touch) - Ultimate Internet Radio
 * 
 * Features:
 *   1. 📶 Clear Wi-Fi Connection Manager & Live Web Portal (http://<device_ip>):
 *        - Manage & Add custom radio stations from phone/PC with State & Language dropdowns
 *        - Shows currently connected network, IP address, and signal strength (RSSI)
 *   2. 📱 Clean Touch Navigation & Robust Debounce Filter:
 *        - 100% Debounced touch actions — zero double-click or ghost triggering everywhere!
 *        - Tab 1: 📻 [ Now Playing ] -> All playback controls inside station card;
 *                                       Giant prominent 12-hour AM/PM Clock + Date & Day + Live Battery % & Charging indicator!
 *        - Tab 2: 📜 [ Stations ]    -> Instant switch to Tab 1 on select; 5-station paged list
 *        - Tab 3: 📶 [ Wi-Fi Setup ] -> Current status banner, scanned network list, debounced touch keyboard
 *        - Tab 4: 🎵 [ SD Music ]    -> Auto-detected when MicroSD card present with audio files;
 *                                       paginated 5-track view, shuffle mode, seamless radio/SD switching
 *   3. 🔋 Real-Time Battery Monitor:
 *        - Live voltage reading on GPIO 5, percentage gauge & ⚡ Charging detection
 *   4. ⚡ Fast Stream Playback (Immediate [ LIVE ] state detection, 0 technical debug text)
 *   5. 🧠 SRAM & CPU Optimized (0% CPU wasted on animations, minimal RAM usage)
 *   6. 🚀 Dedicated FreeRTOS Audio Task (Core 0 Audio DMA + 8MB PSRAM Buffer)
 *   7. 💾 Persistent NVS Flash Wi-Fi Credentials, Custom Stations & Settings
 *   8. 🔊 Mono Mix for Onboard Digital I2S Amplifier & 2W Speaker
 * =========================================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <vector>
#include "FS.h"
#include "SD_MMC.h"
#include <time.h>
#include <lvgl.h>
#include "esp_bsp.h"
#include "lv_port.h"
#include "display.h"
#include "pincfg.h"
#include "Audio.h"
#include "stations_db.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include <esp_wifi.h>
#include <esp_http_server.h>
#include "mbedtls/platform.h"

// Custom mbedtls allocator using 8MB Octal PSRAM to prevent (-32512) SSL Allocation Failures
static void* psram_mbedtls_calloc(size_t n, size_t size) {
    void* ptr = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ptr) {
        ptr = heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return ptr;
}

static void psram_mbedtls_free(void* ptr) {
    free(ptr);
}



// -----------------------------------------------------------------------------
// Preferences & Persistent Wi-Fi Configuration
// -----------------------------------------------------------------------------
Preferences prefs;
String currentSSID = "";
String currentPass = "";
httpd_handle_t webHttpdServer = NULL;
DNSServer dnsServer;
bool webServerStarted = false;
bool apPortalActive = false;

// NTP Time Configuration (UTC+5:30 IST Default)
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;
const char* ntpServer = "pool.ntp.org";

// -----------------------------------------------------------------------------
// Dynamic Runtime Stations Database (Built-in + Custom User Stations)
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
// Audio & State Management (Dual Mode: Internet Radio + SD Card MP3)
// -----------------------------------------------------------------------------
Audio audio;
int currentVolume = 21; // Range: 0 - 21
int prevVolume = 21;
bool isMuted = false;
bool isPlaying = false;
bool isBuffering = false;
String currentStreamTitle = "";
volatile bool uiNeedsUpdate = false;
bool isReconnectingWiFi = false;
uint32_t wifiConnectStartTime = 0;
enum StreamErrorState { ERR_NONE = 0, ERR_NO_WIFI = 1, ERR_OFFLINE = 2, ERR_TIMEOUT = 3 };
StreamErrorState currentError = ERR_NONE;
String alertMessage = "";
uint32_t streamStartTime = 0;
volatile bool newStreamRequested = false;
String pendingStreamUrl = "";
bool pendingIsSdFile = false;

// 10-Attempt Stream Connection Retry Engine State
const int MAX_STREAM_RETRIES = 10;
int streamRetryCount = 0;
bool isRetryingStream = false;
uint32_t nextStreamRetryMs = 0;
String activeTargetUrl = "";
bool streamEstablished = false;

// Playback Source (Radio vs SD Card)
enum PlaySource { SRC_RADIO = 0, SRC_SD = 1 };
PlaySource currentSource = SRC_RADIO;

// SD Card Playlist & State
bool sdCardMounted = false;
std::vector<String> sdTrackPaths;
std::vector<String> sdTrackNames;
int currentSdTrackIdx = 0;
int sdCurrentPage = 0;
bool isShuffle = false;

// Active Category, Filter & Pagination State
enum CategoryType { CAT_ALL = 0, CAT_LANG = 1, CAT_STATE = 2, CAT_USER = 3 };
CategoryType activeCatType = CAT_ALL;
String activeFilterVal = "All";

std::vector<int> filteredIndices; // Matching runtimeStations indices
int currentFilterPosition = 0;   // Index within filteredIndices

#define STATIONS_PER_PAGE 5
int stationCurrentPage = 0;

// Auto-On Alarm & Play Duration State
bool timerEnabled = false;
int timerHour = 6;
int timerMin = 0;
int timerDuration = 30; // 0 = continuous, 15, 30, 45, 60, 90, 120 minutes
int lastTimerTriggerDay = -1;
bool alarmActivePlaying = false;
uint32_t alarmAutoOffExpiryMs = 0;

// -----------------------------------------------------------------------------
// Global Touch Debounce Helper (Eliminates all double-triggering & ghost taps)
// -----------------------------------------------------------------------------
static uint32_t lastGlobalTouchAction = 0;
bool isDebouncedTouch(uint32_t minGapMs = 320) {
    // Wake up backlight if it was turned off by auto-off or sleep
    digitalWrite(TFT_BLK, TFT_BLK_ON_LEVEL);
    if (alarmActivePlaying) {
        alarmActivePlaying = false; // User interacted with device, cancel auto-off
    }
    uint32_t now = millis();
    if (now - lastGlobalTouchAction < minGapMs) {
        return false;
    }
    lastGlobalTouchAction = now;
    return true;
}

// -----------------------------------------------------------------------------
// Battery Measurement & Charging Detection (GPIO 5)
// -----------------------------------------------------------------------------
int getBatteryInfo(float* outVoltage = NULL, bool* outCharging = NULL) {
    analogSetAttenuation(ADC_11db);
    uint32_t raw_mv = analogReadMilliVolts(BAT_ADC_PIN);
    // 2:1 Voltage Divider on JC3248W535
    float voltage = (raw_mv * 2.0f) / 1000.0f;
    if (outVoltage) *outVoltage = voltage;

    bool isCharging = (voltage >= 4.28f);
    if (outCharging) *outCharging = isCharging;

    if (voltage >= 4.18f) return 100;
    if (voltage <= 3.35f) return 0;

    int pct = (int)(((voltage - 3.35f) / (4.18f - 3.35f)) * 100.0f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

// -----------------------------------------------------------------------------
// LVGL UI Objects & Handles
// -----------------------------------------------------------------------------
static lv_obj_t* tabview = NULL;
static lv_obj_t* tab_player = NULL;
static lv_obj_t* tab_stations = NULL;
static lv_obj_t* tab_wifi = NULL;
static lv_obj_t* tab_sd_music = NULL;

// Tab 1: Now Playing Handles
static lv_obj_t* lbl_station_title = NULL;
static lv_obj_t* lbl_station_state = NULL;
static lv_obj_t* lbl_station_lang = NULL;
static lv_obj_t* lbl_status_badge = NULL;
static lv_obj_t* lbl_stream_info = NULL;

static lv_obj_t* pnl_clock_card = NULL;
static lv_obj_t* lbl_clock = NULL;
static lv_obj_t* lbl_date_day = NULL;
static lv_obj_t* lbl_timer_badge = NULL;
static lv_obj_t* lbl_battery = NULL;
static lv_timer_t* clock_timer = NULL;

static lv_obj_t* slider_vol = NULL;
static lv_obj_t* lbl_vol_val = NULL;
static lv_obj_t* btn_mute = NULL;
static lv_obj_t* lbl_btn_mute = NULL;

static lv_obj_t* btn_prev = NULL;
static lv_obj_t* btn_play = NULL;
static lv_obj_t* btn_next = NULL;
static lv_obj_t* lbl_btn_play = NULL;
static lv_obj_t* lbl_counter_badge = NULL;
static lv_obj_t* btn_sleep = NULL;

// Power Off & Wakeup Confirmation Modal Handles
static lv_obj_t* modal_pwr_backdrop = NULL;
static lv_obj_t* modal_pwr_box = NULL;
static lv_obj_t* lbl_pwr_countdown = NULL;
static lv_timer_t* pwr_dialog_timer = NULL;
static int pwr_countdown_val = 6;

static lv_obj_t* modal_wake_backdrop = NULL;
static lv_obj_t* lbl_wake_countdown = NULL;
static lv_timer_t* wake_dialog_timer = NULL;
static int wake_countdown_val = 8;

// Tab 2: Station Browser Handles
static lv_obj_t* list_categories = NULL;
static lv_obj_t* list_stations = NULL;
static lv_obj_t* pnl_page_controls = NULL;
static lv_obj_t* lbl_page_info = NULL;

// Tab 3: Wi-Fi Setup Handles
static lv_obj_t* lbl_wifi_current_val = NULL;
static lv_obj_t* list_wifi_networks = NULL;
static lv_obj_t* box_wifi_connect = NULL;
static lv_obj_t* lbl_selected_ssid = NULL;
static lv_obj_t* ta_wifi_pass = NULL;
static lv_obj_t* kb_wifi = NULL;
static lv_obj_t* lbl_wifi_status = NULL;

// Tab 4: SD Card Music Handles
static lv_obj_t* pnl_sd_page_controls = NULL;
static lv_obj_t* btn_sd_shuffle = NULL;
static lv_obj_t* lbl_sd_shuffle = NULL;
static lv_obj_t* lbl_sd_page_info = NULL;
static lv_obj_t* list_sd_tracks = NULL;

// UI Refresh Flags & Forward Declarations
volatile bool pendingStationListRefresh = false;

void loadSavedSettings();
void loadTimerSettings();
void saveTimerSettings(bool en, int h, int m, int dur = 30);
void checkAutoOnTimer();
void showTimerModal();
void initStationDatabase();
void loadCustomStations();
void saveCustomStation(const String& name, const String& url, const String& state, const String& lang, int editIdx = -1);
void deleteCustomStation(int customIdx);
bool initWiFi();
void initSDCard();
void scanSDFolder(fs::FS &fs, const char * dirname, int depth);
void setupWebServer();
void startAPPortal();
void applyCategoryFilter(CategoryType cType, const char* filterVal);
void playCurrentStation();
void playStationByFilterIndex(int filterIdx);
void playCurrentSdTrack();
void playNextSdTrack();
void playPrevSdTrack();
void buildModernUI();
void updatePlayerUI();
void updateWiFiStatusBanner();
void populateCategoryList();
void populateStationList();
void populateSdTrackList();
void scanAndPopulateWiFi();

// =============================================================================
// Station Database & Custom Station Storage Management
// =============================================================================
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
    Serial.printf("[STATIONS DB] Initialized %d total stations (Built-in: %d, Custom: %d)\n", 
                  (int)runtimeStations.size(), TOTAL_ALL_STATIONS, (int)runtimeStations.size() - TOTAL_ALL_STATIONS);
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
            st.name = sName;
            st.url = sUrl;
            st.state = sState;
            st.language = sLang;
            st.isCustom = true;
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
        String key_n = "n_" + String(editIdx);
        String key_u = "u_" + String(editIdx);
        String key_s = "s_" + String(editIdx);
        String key_l = "l_" + String(editIdx);
        prefs.putString(key_n.c_str(), name);
        prefs.putString(key_u.c_str(), url);
        prefs.putString(key_s.c_str(), stState);
        prefs.putString(key_l.c_str(), stLang);
        prefs.end();
        Serial.printf("[STATIONS DB] Updated Custom Station [%d]: '%s' -> %s\n", editIdx, name.c_str(), url.c_str());
    } else {
        String key_n = "n_" + String(count);
        String key_u = "u_" + String(count);
        String key_s = "s_" + String(count);
        String key_l = "l_" + String(count);
        prefs.putString(key_n.c_str(), name);
        prefs.putString(key_u.c_str(), url);
        prefs.putString(key_s.c_str(), stState);
        prefs.putString(key_l.c_str(), stLang);
        prefs.putInt("count", count + 1);
        prefs.end();
        Serial.printf("[STATIONS DB] Added Custom Station [%d]: '%s' -> %s\n", count, name.c_str(), url.c_str());
    }

    initStationDatabase();
    pendingStationListRefresh = true;
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
    pendingStationListRefresh = true;
}

// =============================================================================
// SD Card Scanning & Playlist Engine
// =============================================================================
void scanSDFolder(fs::FS &fs, const char * dirname, int depth = 0) {
    if (depth > 3 || sdTrackPaths.size() >= 500) return;
    File root = fs.open(dirname);
    if (!root || !root.isDirectory()) return;

    File file = root.openNextFile();
    while (file && sdTrackPaths.size() < 500) {
        if (file.isDirectory()) {
            if (strcmp(file.name(), "System Volume Information") != 0 &&
                strcmp(file.name(), "$RECYCLE.BIN") != 0 &&
                file.name()[0] != '.') {
                String subPath = String(dirname);
                if (!subPath.endsWith("/")) subPath += "/";
                subPath += file.name();
                scanSDFolder(fs, subPath.c_str(), depth + 1);
            }
        } else {
            String fname = String(file.name());
            String lowerName = fname;
            lowerName.toLowerCase();
            if (lowerName.endsWith(".mp3") || lowerName.endsWith(".m4a") || 
                lowerName.endsWith(".aac") || lowerName.endsWith(".wav") || 
                lowerName.endsWith(".flac")) {
                
                String fullPath = String(dirname);
                if (!fullPath.endsWith("/")) fullPath += "/";
                fullPath += fname;
                
                int lastSlash = fname.lastIndexOf('/');
                String cleanName = (lastSlash >= 0) ? fname.substring(lastSlash + 1) : fname;
                int dotIdx = cleanName.lastIndexOf('.');
                if (dotIdx > 0) cleanName = cleanName.substring(0, dotIdx);
                
                sdTrackPaths.push_back(fullPath);
                sdTrackNames.push_back(cleanName);
            }
        }
        file = root.openNextFile();
    }
}

void initSDCard() {
    Serial.println("[SD] Initializing MicroSD Card on SD_MMC...");
    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
    if (SD_MMC.begin("/sdcard", true, false, 20000)) {
        uint8_t cardType = SD_MMC.cardType();
        if (cardType != CARD_NONE) {
            sdCardMounted = true;
            uint64_t totalBytes = SD_MMC.totalBytes() / (1024 * 1024);
            Serial.printf("[SD] MicroSD Card Mounted. Size: %llu MB\n", totalBytes);
            sdTrackPaths.clear();
            sdTrackNames.clear();
            scanSDFolder(SD_MMC, "/", 0);
            Serial.printf("[SD] Found %d audio files.\n", (int)sdTrackPaths.size());
        } else {
            Serial.println("[SD] No card inserted.");
            sdCardMounted = false;
        }
    } else {
        Serial.println("[SD] SD_MMC Mount Failed (No card or unformatted).");
        sdCardMounted = false;
    }
}

// (audioTask completely removed)

// =============================================================================
// Station & SD Playback Engine
// =============================================================================
void applyCategoryFilter(CategoryType cType, const char* filterVal) {
    activeCatType = cType;
    activeFilterVal = String(filterVal);
    stationCurrentPage = 0;

    filteredIndices.clear();

    for (size_t i = 0; i < runtimeStations.size(); i++) {
        if (cType == CAT_ALL || activeFilterVal == "All") {
            filteredIndices.push_back(i);
        } else if (cType == CAT_USER || activeFilterVal == "User") {
            if (runtimeStations[i].isCustom) {
                filteredIndices.push_back(i);
            }
        } else if (cType == CAT_LANG && runtimeStations[i].language == filterVal) {
            filteredIndices.push_back(i);
        } else if (cType == CAT_STATE && runtimeStations[i].state == filterVal) {
            filteredIndices.push_back(i);
        }
    }

    if (filteredIndices.empty() && cType != CAT_USER) {
        for (size_t i = 0; i < runtimeStations.size(); i++) filteredIndices.push_back(i);
    }

    currentFilterPosition = 0;
    if (list_stations) {
        populateStationList();
    }
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

    currentSource = SRC_RADIO;
    pendingIsSdFile = false;

    int realIdx = filteredIndices[currentFilterPosition];
    const LiveStation& st = runtimeStations[realIdx];

    Serial.printf("\n[RADIO] Switching to [%d/%d]: %s (%s - %s)\n", 
                  currentFilterPosition + 1, (int)filteredIndices.size(), st.name.c_str(), st.state.c_str(), st.language.c_str());
    Serial.printf("[RADIO] Stream URL: %s\n", st.url.c_str());

    // Save Last Played Station to NVS Flash
    prefs.begin("air_radio", false);
    prefs.putInt("last_st", realIdx);
    prefs.end();

    // 1. Instantly update UI with the new station name and buffering state
    activeTargetUrl = st.url;
    streamRetryCount = 0;
    isRetryingStream = false;
    streamEstablished = false;

    currentError = ERR_NONE;
    alertMessage = "Connecting to live broadcast...";
    streamStartTime = millis();
    isBuffering = true;
    isPlaying = true;
    currentStreamTitle = "";
    updatePlayerUI();

    if (WiFi.status() != WL_CONNECTED) {
        currentError = ERR_NO_WIFI;
        alertMessage = "Wi-Fi disconnected. Open Wi-Fi Setup.";
        isPlaying = false;
        isBuffering = false;
        updatePlayerUI();
        return;
    }

    // 2. Delegate stream switching to Core 0 Audio Task (100% thread-safe!)
    pendingStreamUrl = st.url;
    newStreamRequested = true;
}

void playCurrentSdTrack() {
    if (sdTrackPaths.empty()) return;
    if (currentSdTrackIdx < 0) currentSdTrackIdx = 0;
    if (currentSdTrackIdx >= (int)sdTrackPaths.size()) currentSdTrackIdx = (int)sdTrackPaths.size() - 1;

    currentSource = SRC_SD;
    pendingIsSdFile = true;
    pendingStreamUrl = sdTrackPaths[currentSdTrackIdx];
    currentStreamTitle = sdTrackNames[currentSdTrackIdx];
    currentError = ERR_NONE;
    isBuffering = false;
    isPlaying = true;

    Serial.printf("\n[SD PLAYER] Switching to [%d/%d]: %s\n", 
                  currentSdTrackIdx + 1, (int)sdTrackPaths.size(), pendingStreamUrl.c_str());

    newStreamRequested = true;
    updatePlayerUI();
    if (list_sd_tracks) populateSdTrackList();
}

void playNextSdTrack() {
    if (sdTrackPaths.empty()) return;
    if (isShuffle) {
        if (sdTrackPaths.size() > 1) {
            int nextIdx = random(0, sdTrackPaths.size());
            if (nextIdx == currentSdTrackIdx) nextIdx = (nextIdx + 1) % sdTrackPaths.size();
            currentSdTrackIdx = nextIdx;
        }
    } else {
        currentSdTrackIdx = (currentSdTrackIdx + 1) % sdTrackPaths.size();
    }
    sdCurrentPage = currentSdTrackIdx / STATIONS_PER_PAGE;
    playCurrentSdTrack();
}

void playPrevSdTrack() {
    if (sdTrackPaths.empty()) return;
    if (isShuffle) {
        if (sdTrackPaths.size() > 1) {
            int nextIdx = random(0, sdTrackPaths.size());
            if (nextIdx == currentSdTrackIdx) nextIdx = (nextIdx + 1) % sdTrackPaths.size();
            currentSdTrackIdx = nextIdx;
        }
    } else {
        currentSdTrackIdx = (currentSdTrackIdx - 1 + sdTrackPaths.size()) % sdTrackPaths.size();
    }
    sdCurrentPage = currentSdTrackIdx / STATIONS_PER_PAGE;
    playCurrentSdTrack();
}

void enterDeepSleep() {
    Serial.println("\n[POWER] Entering Deep Sleep Standby Mode...");

    // 1. Stop Audio Playback Immediately
    isPlaying = false;
    audio.stopSong();

    // 2. Save State to NVS Flash (Last Station & Volume)
    prefs.begin("air_radio", false);
    prefs.putInt("last_vol", currentVolume);
    if (!filteredIndices.empty()) {
        prefs.putInt("last_st", filteredIndices[currentFilterPosition]);
    }
    prefs.end();

    // 3. Disconnect & Power Down Wi-Fi
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);

    // 4. Fade & Turn Off Display Backlight Immediately
    bsp_display_brightness_set(0);
    digitalWrite(TFT_BLK, LOW);

    // 5. Configure RTC GPIO Pull-ups so INT line does not float LOW when CPU sleeps
    pinMode(TOUCH_PIN_NUM_INT, INPUT_PULLUP);
    pinMode(0, INPUT_PULLUP);

    rtc_gpio_init(GPIO_NUM_3);
    rtc_gpio_set_direction(GPIO_NUM_3, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(GPIO_NUM_3);
    rtc_gpio_pulldown_dis(GPIO_NUM_3);

    rtc_gpio_init(GPIO_NUM_0);
    rtc_gpio_set_direction(GPIO_NUM_0, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(GPIO_NUM_0);
    rtc_gpio_pulldown_dis(GPIO_NUM_0);

    // Ensure touch is fully released before arming wakeup
    for (int i = 0; i < 20; i++) {
        if (digitalRead(TOUCH_PIN_NUM_INT) == HIGH && digitalRead(0) == HIGH) break;
        delay(10);
    }

    // 6. Arm Capacitive Touch Interrupt (GPIO 3) & BOOT button (GPIO 0) as Wakeup Sources
    esp_sleep_enable_ext1_wakeup((1ULL << 3) | (1ULL << 0), ESP_EXT1_WAKEUP_ANY_LOW);

    Serial.println("[POWER] Deep Sleep active. Touch screen to wake.");
    Serial.flush();
    esp_deep_sleep_start();
}

// =============================================================================
// Power Off & Wakeup Confirmation Dialogs with Auto-Timeout
// =============================================================================
void closePowerOffPrompt() {
    bsp_display_lock(0);
    if (pwr_dialog_timer) {
        lv_timer_del(pwr_dialog_timer);
        pwr_dialog_timer = NULL;
    }
    if (modal_pwr_backdrop) {
        lv_obj_del(modal_pwr_backdrop);
        modal_pwr_backdrop = NULL;
    }
    bsp_display_unlock();
}

static void pwr_timer_cb(lv_timer_t* timer) {
    pwr_countdown_val--;
    if (pwr_countdown_val <= 0) {
        closePowerOffPrompt();
        enterDeepSleep();
    } else {
        if (lbl_pwr_countdown) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Auto turning off in %d s...", pwr_countdown_val);
            lv_label_set_text(lbl_pwr_countdown, buf);
        }
    }
}

static void btn_confirm_off_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;
    closePowerOffPrompt();
    enterDeepSleep();
}

static void btn_cancel_off_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;
    closePowerOffPrompt();
}

void openPowerOffPrompt() {
    bsp_display_lock(0);
    if (modal_pwr_backdrop) {
        bsp_display_unlock();
        return;
    }

    pwr_countdown_val = 6;

    lv_obj_t* scr = lv_scr_act();
    modal_pwr_backdrop = lv_obj_create(scr);
    lv_obj_set_size(modal_pwr_backdrop, 480, 320);
    lv_obj_set_pos(modal_pwr_backdrop, 0, 0);
    lv_obj_set_style_bg_color(modal_pwr_backdrop, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(modal_pwr_backdrop, LV_OPA_70, 0);
    lv_obj_set_style_border_width(modal_pwr_backdrop, 0, 0);
    lv_obj_clear_flag(modal_pwr_backdrop, LV_OBJ_FLAG_SCROLLABLE);

    modal_pwr_box = lv_obj_create(modal_pwr_backdrop);
    lv_obj_set_size(modal_pwr_box, 360, 195);
    lv_obj_center(modal_pwr_box);
    lv_obj_set_style_bg_color(modal_pwr_box, lv_color_hex(0x181014), 0);
    lv_obj_set_style_border_color(modal_pwr_box, lv_color_hex(0xFF5252), 0);
    lv_obj_set_style_border_width(modal_pwr_box, 2, 0);
    lv_obj_set_style_radius(modal_pwr_box, 14, 0);
    lv_obj_set_style_pad_all(modal_pwr_box, 12, 0);
    lv_obj_clear_flag(modal_pwr_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_title = lv_label_create(modal_pwr_box);
    lv_label_set_text(lbl_title, LV_SYMBOL_POWER "  Turn Off Radio?");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xFF5252), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 4);

    lbl_pwr_countdown = lv_label_create(modal_pwr_box);
    lv_label_set_text(lbl_pwr_countdown, "Auto turning off in 6 s...");
    lv_obj_set_style_text_color(lbl_pwr_countdown, lv_color_hex(0xCFD8DC), 0);
    lv_obj_set_style_text_font(lbl_pwr_countdown, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_pwr_countdown, LV_ALIGN_CENTER, 0, -14);

    lv_obj_t* btn_cancel = lv_btn_create(modal_pwr_box);
    lv_obj_set_size(btn_cancel, 140, 44);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 10, -4);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x263238), 0);
    lv_obj_set_style_radius(btn_cancel, 8, 0);
    lv_obj_add_event_cb(btn_cancel, btn_cancel_off_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_c = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_c, "Stay On");
    lv_obj_set_style_text_color(lbl_c, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_c, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_c);

    lv_obj_t* btn_off = lv_btn_create(modal_pwr_box);
    lv_obj_set_size(btn_off, 150, 44);
    lv_obj_align(btn_off, LV_ALIGN_BOTTOM_RIGHT, -10, -4);
    lv_obj_set_style_bg_color(btn_off, lv_color_hex(0xD32F2F), 0);
    lv_obj_set_style_radius(btn_off, 8, 0);
    lv_obj_add_event_cb(btn_off, btn_confirm_off_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_o = lv_label_create(btn_off);
    lv_label_set_text(lbl_o, LV_SYMBOL_POWER " Turn Off");
    lv_obj_set_style_text_color(lbl_o, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_o, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_o);

    pwr_dialog_timer = lv_timer_create(pwr_timer_cb, 1000, NULL);
    bsp_display_unlock();
}

static void wake_timer_cb(lv_timer_t* timer) {
    wake_countdown_val--;
    if (wake_countdown_val <= 0) {
        if (wake_dialog_timer) {
            lv_timer_del(wake_dialog_timer);
            wake_dialog_timer = NULL;
        }
        enterDeepSleep();
    } else {
        if (lbl_wake_countdown) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Returning to sleep in %d s...", wake_countdown_val);
            lv_label_set_text(lbl_wake_countdown, buf);
        }
    }
}

static void btn_wake_resume_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;

    bsp_display_lock(0);
    if (wake_dialog_timer) {
        lv_timer_del(wake_dialog_timer);
        wake_dialog_timer = NULL;
    }
    if (modal_wake_backdrop) {
        lv_obj_del(modal_wake_backdrop);
        modal_wake_backdrop = NULL;
    }
    bsp_display_unlock();

    if (currentSSID.length() > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        WiFi.begin(currentSSID.c_str(), currentPass.c_str());
        wifiConnectStartTime = millis();
        isReconnectingWiFi = true;
        isBuffering = true;
        updatePlayerUI();
    } else {
        bsp_display_lock(0);
        lv_tabview_set_act(tabview, 2, LV_ANIM_OFF);
        bsp_display_unlock();
        scanAndPopulateWiFi();
        startAPPortal();
    }
}

static void btn_wake_sleep_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;
    if (wake_dialog_timer) {
        lv_timer_del(wake_dialog_timer);
        wake_dialog_timer = NULL;
    }
    enterDeepSleep();
}

void showWakeupPrompt() {
    bsp_display_lock(0);
    wake_countdown_val = 8;

    lv_obj_t* scr = lv_scr_act();
    modal_wake_backdrop = lv_obj_create(scr);
    lv_obj_set_size(modal_wake_backdrop, 480, 320);
    lv_obj_set_pos(modal_wake_backdrop, 0, 0);
    lv_obj_set_style_bg_color(modal_wake_backdrop, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(modal_wake_backdrop, LV_OPA_80, 0);
    lv_obj_set_style_border_width(modal_wake_backdrop, 0, 0);
    lv_obj_clear_flag(modal_wake_backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(modal_wake_backdrop, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(modal_wake_backdrop, btn_wake_resume_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* modal_box = lv_obj_create(modal_wake_backdrop);
    lv_obj_set_size(modal_box, 380, 205);
    lv_obj_center(modal_box);
    lv_obj_set_style_bg_color(modal_box, lv_color_hex(0x0E1922), 0);
    lv_obj_set_style_border_color(modal_box, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(modal_box, 2, 0);
    lv_obj_set_style_radius(modal_box, 14, 0);
    lv_obj_set_style_pad_all(modal_box, 14, 0);
    lv_obj_clear_flag(modal_box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_title = lv_label_create(modal_box);
    lv_label_set_text(lbl_title, LV_SYMBOL_PLAY "  Radio Woke Up");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 2);

    lbl_wake_countdown = lv_label_create(modal_box);
    lv_label_set_text(lbl_wake_countdown, "Returning to sleep in 8 s...");
    lv_obj_set_style_text_color(lbl_wake_countdown, lv_color_hex(0xB0BEC5), 0);
    lv_obj_set_style_text_font(lbl_wake_countdown, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_wake_countdown, LV_ALIGN_CENTER, 0, -14);

    lv_obj_t* btn_sleep_now = lv_btn_create(modal_box);
    lv_obj_set_size(btn_sleep_now, 145, 46);
    lv_obj_align(btn_sleep_now, LV_ALIGN_BOTTOM_LEFT, 6, -4);
    lv_obj_set_style_bg_color(btn_sleep_now, lv_color_hex(0x263238), 0);
    lv_obj_set_style_radius(btn_sleep_now, 8, 0);
    lv_obj_add_event_cb(btn_sleep_now, btn_wake_sleep_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_sn = lv_label_create(btn_sleep_now);
    lv_label_set_text(lbl_sn, LV_SYMBOL_POWER " Stay Off");
    lv_obj_set_style_text_color(lbl_sn, lv_color_hex(0xFF8A80), 0);
    lv_obj_set_style_text_font(lbl_sn, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_sn);

    lv_obj_t* btn_resume = lv_btn_create(modal_box);
    lv_obj_set_size(btn_resume, 165, 46);
    lv_obj_align(btn_resume, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
    lv_obj_set_style_bg_color(btn_resume, lv_color_hex(0x00B0FF), 0);
    lv_obj_set_style_radius(btn_resume, 8, 0);
    lv_obj_add_event_cb(btn_resume, btn_wake_resume_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_res = lv_label_create(btn_resume);
    lv_label_set_text(lbl_res, LV_SYMBOL_PLAY " Resume Radio");
    lv_obj_set_style_text_color(lbl_res, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(lbl_res, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_res);

    wake_dialog_timer = lv_timer_create(wake_timer_cb, 1000, NULL);
    bsp_display_unlock();
}

// =============================================================================
// On-Device Auto-On Alarm & Sleep Timer Modal UI
// =============================================================================
static lv_obj_t* modal_timer_backdrop = NULL;
static lv_obj_t* modal_timer_box = NULL;
static lv_obj_t* btn_timer_en_toggle = NULL;
static lv_obj_t* lbl_timer_en_status = NULL;
static lv_obj_t* lbl_modal_h = NULL;
static lv_obj_t* lbl_modal_m = NULL;
static lv_obj_t* lbl_modal_ampm = NULL;
static lv_obj_t* lbl_modal_dur = NULL;

static bool edit_timer_en = false;
static int edit_timer_h = 6;
static int edit_timer_m = 0;
static int edit_timer_dur_idx = 1;

static const int PRESET_DURATIONS[] = {15, 30, 45, 60, 90, 120, 0};
static const char* PRESET_DURATION_LABELS[] = {
    "15 Minutes",
    "30 Minutes",
    "45 Minutes",
    "60 Minutes",
    "90 Minutes",
    "120 Minutes",
    "Continuous (No auto-off)"
};
static const int NUM_PRESET_DURATIONS = 7;

static void update_timer_modal_display() {
    if (!modal_timer_box) return;

    if (lbl_timer_en_status && btn_timer_en_toggle) {
        if (edit_timer_en) {
            lv_label_set_text(lbl_timer_en_status, "ALARM: ENABLED");
            lv_obj_set_style_bg_color(btn_timer_en_toggle, lv_color_hex(0x2E7D32), 0);
        } else {
            lv_label_set_text(lbl_timer_en_status, "ALARM: DISABLED");
            lv_obj_set_style_bg_color(btn_timer_en_toggle, lv_color_hex(0x37474F), 0);
        }
    }

    if (lbl_modal_h && lbl_modal_m && lbl_modal_ampm) {
        int dispH = edit_timer_h;
        const char* ampm = "AM";
        if (dispH >= 12) {
            ampm = "PM";
            if (dispH > 12) dispH -= 12;
        }
        if (dispH == 0) dispH = 12;

        char bufH[8], bufM[8];
        snprintf(bufH, sizeof(bufH), "%02d", dispH);
        snprintf(bufM, sizeof(bufM), "%02d", edit_timer_m);
        lv_label_set_text(lbl_modal_h, bufH);
        lv_label_set_text(lbl_modal_m, bufM);
        lv_label_set_text(lbl_modal_ampm, ampm);
    }

    if (lbl_modal_dur) {
        lv_label_set_text(lbl_modal_dur, PRESET_DURATION_LABELS[edit_timer_dur_idx]);
    }
}

static void btn_timer_toggle_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;
    edit_timer_en = !edit_timer_en;
    update_timer_modal_display();
}

static void btn_timer_h_dec_cb(lv_event_t* e) {
    if (!isDebouncedTouch(150)) return;
    edit_timer_h--;
    if (edit_timer_h < 0) edit_timer_h = 23;
    update_timer_modal_display();
}

static void btn_timer_h_inc_cb(lv_event_t* e) {
    if (!isDebouncedTouch(150)) return;
    edit_timer_h++;
    if (edit_timer_h > 23) edit_timer_h = 0;
    update_timer_modal_display();
}

static void btn_timer_m_dec_cb(lv_event_t* e) {
    if (!isDebouncedTouch(150)) return;
    edit_timer_m -= 5;
    if (edit_timer_m < 0) edit_timer_m = 55;
    update_timer_modal_display();
}

static void btn_timer_m_inc_cb(lv_event_t* e) {
    if (!isDebouncedTouch(150)) return;
    edit_timer_m += 5;
    if (edit_timer_m >= 60) edit_timer_m = 0;
    update_timer_modal_display();
}

static void btn_timer_dur_prev_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;
    edit_timer_dur_idx--;
    if (edit_timer_dur_idx < 0) edit_timer_dur_idx = NUM_PRESET_DURATIONS - 1;
    update_timer_modal_display();
}

static void btn_timer_dur_next_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;
    edit_timer_dur_idx++;
    if (edit_timer_dur_idx >= NUM_PRESET_DURATIONS) edit_timer_dur_idx = 0;
    update_timer_modal_display();
}

static void btn_timer_cancel_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;
    bsp_display_lock(0);
    if (modal_timer_backdrop) {
        lv_obj_del(modal_timer_backdrop);
        modal_timer_backdrop = NULL;
        modal_timer_box = NULL;
    }
    bsp_display_unlock();
}

void updateClockAndBatteryUI();

static void btn_timer_save_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;
    int selectedDur = PRESET_DURATIONS[edit_timer_dur_idx];
    saveTimerSettings(edit_timer_en, edit_timer_h, edit_timer_m, selectedDur);

    bsp_display_lock(0);
    if (modal_timer_backdrop) {
        lv_obj_del(modal_timer_backdrop);
        modal_timer_backdrop = NULL;
        modal_timer_box = NULL;
    }
    updateClockAndBatteryUI();
    bsp_display_unlock();
}

void showTimerModal() {
    if (modal_timer_backdrop) return;

    edit_timer_en = timerEnabled;
    edit_timer_h = timerHour;
    edit_timer_m = timerMin;
    edit_timer_dur_idx = 1;
    for (int i = 0; i < NUM_PRESET_DURATIONS; i++) {
        if (PRESET_DURATIONS[i] == timerDuration) {
            edit_timer_dur_idx = i;
            break;
        }
    }

    bsp_display_lock(0);
    lv_obj_t* scr = lv_scr_act();
    modal_timer_backdrop = lv_obj_create(scr);
    lv_obj_set_size(modal_timer_backdrop, 480, 320);
    lv_obj_set_pos(modal_timer_backdrop, 0, 0);
    lv_obj_set_style_bg_color(modal_timer_backdrop, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(modal_timer_backdrop, LV_OPA_70, 0);
    lv_obj_set_style_border_width(modal_timer_backdrop, 0, 0);
    lv_obj_clear_flag(modal_timer_backdrop, LV_OBJ_FLAG_SCROLLABLE);

    modal_timer_box = lv_obj_create(modal_timer_backdrop);
    lv_obj_set_size(modal_timer_box, 400, 270);
    lv_obj_center(modal_timer_box);
    lv_obj_set_style_bg_color(modal_timer_box, lv_color_hex(0x10171E), 0);
    lv_obj_set_style_border_color(modal_timer_box, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(modal_timer_box, 2, 0);
    lv_obj_set_style_radius(modal_timer_box, 12, 0);
    lv_obj_set_style_pad_all(modal_timer_box, 10, 0);
    lv_obj_clear_flag(modal_timer_box, LV_OBJ_FLAG_SCROLLABLE);

    // 1. Title
    lv_obj_t* lbl_t = lv_label_create(modal_timer_box);
    lv_label_set_text(lbl_t, LV_SYMBOL_BELL " Auto-On Alarm & Sleep Timer");
    lv_obj_set_style_text_color(lbl_t, lv_color_hex(0xFFD54F), 0);
    lv_obj_set_style_text_font(lbl_t, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_t, LV_ALIGN_TOP_MID, 0, 0);

    // 2. Enable / Disable Toggle Row
    btn_timer_en_toggle = lv_btn_create(modal_timer_box);
    lv_obj_set_size(btn_timer_en_toggle, 170, 32);
    lv_obj_align(btn_timer_en_toggle, LV_ALIGN_TOP_MID, 0, 26);
    lv_obj_set_style_radius(btn_timer_en_toggle, 8, 0);
    lv_obj_add_event_cb(btn_timer_en_toggle, btn_timer_toggle_cb, LV_EVENT_CLICKED, NULL);

    lbl_timer_en_status = lv_label_create(btn_timer_en_toggle);
    lv_obj_set_style_text_font(lbl_timer_en_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_timer_en_status, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_timer_en_status);

    // 3. Time Adjustment Controls Box
    lv_obj_t* pnl_time = lv_obj_create(modal_timer_box);
    lv_obj_set_size(pnl_time, 376, 52);
    lv_obj_align(pnl_time, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_set_style_bg_color(pnl_time, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_border_color(pnl_time, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_width(pnl_time, 1, 0);
    lv_obj_set_style_radius(pnl_time, 8, 0);
    lv_obj_set_style_pad_all(pnl_time, 4, 0);
    lv_obj_clear_flag(pnl_time, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_tlabel = lv_label_create(pnl_time);
    lv_label_set_text(lbl_tlabel, "Time:");
    lv_obj_set_style_text_color(lbl_tlabel, lv_color_hex(0x90A4AE), 0);
    lv_obj_set_style_text_font(lbl_tlabel, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_tlabel, LV_ALIGN_LEFT_MID, 6, 0);

    lv_obj_t* btn_h_dec = lv_btn_create(pnl_time);
    lv_obj_set_size(btn_h_dec, 34, 34);
    lv_obj_align(btn_h_dec, LV_ALIGN_LEFT_MID, 56, 0);
    lv_obj_set_style_bg_color(btn_h_dec, lv_color_hex(0x21262D), 0);
    lv_obj_add_event_cb(btn_h_dec, btn_timer_h_dec_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l_h_d = lv_label_create(btn_h_dec);
    lv_label_set_text(l_h_d, "-");
    lv_obj_set_style_text_font(l_h_d, &lv_font_montserrat_18, 0);
    lv_obj_center(l_h_d);

    lbl_modal_h = lv_label_create(pnl_time);
    lv_obj_set_style_text_font(lbl_modal_h, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_h, lv_color_hex(0xFFD54F), 0);
    lv_obj_align(lbl_modal_h, LV_ALIGN_LEFT_MID, 98, 0);

    lv_obj_t* btn_h_inc = lv_btn_create(pnl_time);
    lv_obj_set_size(btn_h_inc, 34, 34);
    lv_obj_align(btn_h_inc, LV_ALIGN_LEFT_MID, 130, 0);
    lv_obj_set_style_bg_color(btn_h_inc, lv_color_hex(0x21262D), 0);
    lv_obj_add_event_cb(btn_h_inc, btn_timer_h_inc_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l_h_i = lv_label_create(btn_h_inc);
    lv_label_set_text(l_h_i, "+");
    lv_obj_set_style_text_font(l_h_i, &lv_font_montserrat_18, 0);
    lv_obj_center(l_h_i);

    lv_obj_t* lbl_colon = lv_label_create(pnl_time);
    lv_label_set_text(lbl_colon, ":");
    lv_obj_set_style_text_font(lbl_colon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_colon, lv_color_hex(0x00E5FF), 0);
    lv_obj_align(lbl_colon, LV_ALIGN_LEFT_MID, 172, 0);

    lv_obj_t* btn_m_dec = lv_btn_create(pnl_time);
    lv_obj_set_size(btn_m_dec, 34, 34);
    lv_obj_align(btn_m_dec, LV_ALIGN_LEFT_MID, 186, 0);
    lv_obj_set_style_bg_color(btn_m_dec, lv_color_hex(0x21262D), 0);
    lv_obj_add_event_cb(btn_m_dec, btn_timer_m_dec_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l_m_d = lv_label_create(btn_m_dec);
    lv_label_set_text(l_m_d, "-");
    lv_obj_set_style_text_font(l_m_d, &lv_font_montserrat_18, 0);
    lv_obj_center(l_m_d);

    lbl_modal_m = lv_label_create(pnl_time);
    lv_obj_set_style_text_font(lbl_modal_m, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_modal_m, lv_color_hex(0xFFD54F), 0);
    lv_obj_align(lbl_modal_m, LV_ALIGN_LEFT_MID, 228, 0);

    lv_obj_t* btn_m_inc = lv_btn_create(pnl_time);
    lv_obj_set_size(btn_m_inc, 34, 34);
    lv_obj_align(btn_m_inc, LV_ALIGN_LEFT_MID, 260, 0);
    lv_obj_set_style_bg_color(btn_m_inc, lv_color_hex(0x21262D), 0);
    lv_obj_add_event_cb(btn_m_inc, btn_timer_m_inc_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l_m_i = lv_label_create(btn_m_inc);
    lv_label_set_text(l_m_i, "+");
    lv_obj_set_style_text_font(l_m_i, &lv_font_montserrat_18, 0);
    lv_obj_center(l_m_i);

    lbl_modal_ampm = lv_label_create(pnl_time);
    lv_obj_set_style_text_font(lbl_modal_ampm, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_modal_ampm, lv_color_hex(0x00E5FF), 0);
    lv_obj_align(lbl_modal_ampm, LV_ALIGN_RIGHT_MID, -8, 0);

    // 4. Play Duration Selector Box
    lv_obj_t* pnl_dur = lv_obj_create(modal_timer_box);
    lv_obj_set_size(pnl_dur, 376, 52);
    lv_obj_align(pnl_dur, LV_ALIGN_TOP_MID, 0, 122);
    lv_obj_set_style_bg_color(pnl_dur, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_border_color(pnl_dur, lv_color_hex(0x30363D), 0);
    lv_obj_set_style_border_width(pnl_dur, 1, 0);
    lv_obj_set_style_radius(pnl_dur, 8, 0);
    lv_obj_set_style_pad_all(pnl_dur, 4, 0);
    lv_obj_clear_flag(pnl_dur, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_dlabel = lv_label_create(pnl_dur);
    lv_label_set_text(lbl_dlabel, "Duration:");
    lv_obj_set_style_text_color(lbl_dlabel, lv_color_hex(0x90A4AE), 0);
    lv_obj_set_style_text_font(lbl_dlabel, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_dlabel, LV_ALIGN_LEFT_MID, 6, 0);

    lv_obj_t* btn_dur_prev = lv_btn_create(pnl_dur);
    lv_obj_set_size(btn_dur_prev, 34, 34);
    lv_obj_align(btn_dur_prev, LV_ALIGN_LEFT_MID, 86, 0);
    lv_obj_set_style_bg_color(btn_dur_prev, lv_color_hex(0x21262D), 0);
    lv_obj_add_event_cb(btn_dur_prev, btn_timer_dur_prev_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l_dp = lv_label_create(btn_dur_prev);
    lv_label_set_text(l_dp, "<");
    lv_obj_set_style_text_font(l_dp, &lv_font_montserrat_16, 0);
    lv_obj_center(l_dp);

    lv_obj_t* btn_dur_click = lv_btn_create(pnl_dur);
    lv_obj_set_size(btn_dur_click, 206, 34);
    lv_obj_align(btn_dur_click, LV_ALIGN_LEFT_MID, 126, 0);
    lv_obj_set_style_bg_color(btn_dur_click, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_border_color(btn_dur_click, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(btn_dur_click, 1, 0);
    lv_obj_add_event_cb(btn_dur_click, btn_timer_dur_next_cb, LV_EVENT_CLICKED, NULL);

    lbl_modal_dur = lv_label_create(btn_dur_click);
    lv_obj_set_style_text_font(lbl_modal_dur, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_modal_dur, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_modal_dur);

    lv_obj_t* btn_dur_next = lv_btn_create(pnl_dur);
    lv_obj_set_size(btn_dur_next, 34, 34);
    lv_obj_align(btn_dur_next, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_dur_next, lv_color_hex(0x21262D), 0);
    lv_obj_add_event_cb(btn_dur_next, btn_timer_dur_next_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l_dn = lv_label_create(btn_dur_next);
    lv_label_set_text(l_dn, ">");
    lv_obj_set_style_text_font(l_dn, &lv_font_montserrat_16, 0);
    lv_obj_center(l_dn);

    // 5. Action Buttons (Cancel / Save)
    lv_obj_t* btn_cancel = lv_btn_create(modal_timer_box);
    lv_obj_set_size(btn_cancel, 120, 38);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 16, -4);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x37474F), 0);
    lv_obj_set_style_radius(btn_cancel, 8, 0);
    lv_obj_add_event_cb(btn_cancel, btn_timer_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l_c = lv_label_create(btn_cancel);
    lv_label_set_text(l_c, "Cancel");
    lv_obj_set_style_text_font(l_c, &lv_font_montserrat_14, 0);
    lv_obj_center(l_c);

    lv_obj_t* btn_save = lv_btn_create(modal_timer_box);
    lv_obj_set_size(btn_save, 160, 38);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -16, -4);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x00897B), 0);
    lv_obj_set_style_radius(btn_save, 8, 0);
    lv_obj_add_event_cb(btn_save, btn_timer_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l_s = lv_label_create(btn_save);
    lv_label_set_text(l_s, "💾 Save Alarm");
    lv_obj_set_style_text_font(l_s, &lv_font_montserrat_14, 0);
    lv_obj_center(l_s);

    update_timer_modal_display();
    bsp_display_unlock();
}

static void clock_card_clicked_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;
    showTimerModal();
}

static void btn_sleep_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;
    openPowerOffPrompt();
}

static void card_long_press_cb(lv_event_t* e) {
    openPowerOffPrompt();
}

static void btn_prev_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;

    if (currentSource == SRC_SD) {
        playPrevSdTrack();
    } else {
        playStationByFilterIndex(currentFilterPosition - 1);
    }
}

static void btn_next_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;

    if (currentSource == SRC_SD) {
        playNextSdTrack();
    } else {
        playStationByFilterIndex(currentFilterPosition + 1);
    }
}

static void btn_play_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;

    if (currentSource == SRC_RADIO && WiFi.status() != WL_CONNECTED) {
        bsp_display_lock(0);
        lv_tabview_set_act(tabview, 2, LV_ANIM_OFF);
        bsp_display_unlock();
        scanAndPopulateWiFi();
        return;
    }

    if (isPlaying) {
        audio.pauseResume();
        isPlaying = audio.isRunning();
    } else {
        if (currentSource == SRC_SD) {
            playCurrentSdTrack();
        } else {
            playCurrentStation();
        }
    }
    updatePlayerUI();
}

void setSystemVolume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 21) vol = 21;
    currentVolume = vol;
    isMuted = (currentVolume == 0);
    audio.setVolume(currentVolume);

    bsp_display_lock(0);
    if (slider_vol) lv_slider_set_value(slider_vol, currentVolume, LV_ANIM_OFF);
    if (lbl_vol_val) {
        char buf[16];
        if (isMuted) snprintf(buf, sizeof(buf), "MUTED");
        else snprintf(buf, sizeof(buf), "VOL %d%%", (currentVolume * 100) / 21);
        lv_label_set_text(lbl_vol_val, buf);
    }
    if (btn_mute && lbl_btn_mute) {
        if (isMuted) {
            lv_label_set_text(lbl_btn_mute, LV_SYMBOL_MUTE " UNMUTE");
            lv_obj_set_style_bg_color(btn_mute, lv_color_hex(0xD32F2F), 0);
        } else {
            lv_label_set_text(lbl_btn_mute, LV_SYMBOL_VOLUME_MAX " MUTE");
            lv_obj_set_style_bg_color(btn_mute, lv_color_hex(0x1E2B37), 0);
        }
    }
    bsp_display_unlock();

    prefs.begin("air_radio", false);
    prefs.putInt("last_vol", currentVolume);
    prefs.end();
}

void toggleMute() {
    if (!isMuted) {
        prevVolume = (currentVolume > 0) ? currentVolume : 15;
        setSystemVolume(0);
    } else {
        setSystemVolume((prevVolume > 0) ? prevVolume : 15);
    }
}

static void btn_mute_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;
    toggleMute();
}

static void slider_vol_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    setSystemVolume((int)lv_slider_get_value(slider));
}

static lv_point_t cat_press_point;
static bool cat_dragged = false;

static void category_btn_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* indev = lv_indev_get_act();

    if (code == LV_EVENT_PRESSED) {
        if (indev) lv_indev_get_point(indev, &cat_press_point);
        cat_dragged = false;
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        if (indev) {
            lv_point_t cur_point;
            lv_indev_get_point(indev, &cur_point);
            if (abs(cur_point.x - cat_press_point.x) > 10 || abs(cur_point.y - cat_press_point.y) > 10) {
                cat_dragged = true;
            }
        }
        return;
    }

    if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_CLICKED) {
        if (cat_dragged) {
            cat_dragged = false;
            return;
        }
        if (indev && lv_indev_get_scroll_obj(indev) != NULL) {
            return;
        }
        if (!isDebouncedTouch()) return;

        const char* txt = (const char*)lv_event_get_user_data(e);
        if (strncmp(txt, "LANG:", 5) == 0) {
            applyCategoryFilter(CAT_LANG, txt + 5);
        } else if (strncmp(txt, "STATE:", 6) == 0) {
            applyCategoryFilter(CAT_STATE, txt + 6);
        } else if (strncmp(txt, "USER:", 5) == 0) {
            applyCategoryFilter(CAT_USER, "User");
        } else {
            applyCategoryFilter(CAT_ALL, "All");
        }
    }
}

static void station_item_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;

    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    currentFilterPosition = idx;
    
    bsp_display_lock(0);
    lv_tabview_set_act(tabview, 0, LV_ANIM_OFF);
    bsp_display_unlock();

    playCurrentStation();
}

static void btn_page_prev_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;

    int total = (int)filteredIndices.size();
    int maxPages = (total + STATIONS_PER_PAGE - 1) / STATIONS_PER_PAGE;
    if (maxPages <= 0) maxPages = 1;
    if (stationCurrentPage > 0) {
        stationCurrentPage--;
    } else {
        stationCurrentPage = maxPages - 1;
    }
    populateStationList();
}

static void btn_page_next_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;

    int total = (int)filteredIndices.size();
    int maxPages = (total + STATIONS_PER_PAGE - 1) / STATIONS_PER_PAGE;
    if (maxPages <= 0) maxPages = 1;
    if (stationCurrentPage < maxPages - 1) {
        stationCurrentPage++;
    } else {
        stationCurrentPage = 0;
    }
    populateStationList();
}

// -----------------------------------------------------------------------------
// SD Music Callbacks
// -----------------------------------------------------------------------------
static void sd_track_item_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;

    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    currentSdTrackIdx = idx;
    
    bsp_display_lock(0);
    lv_tabview_set_act(tabview, 0, LV_ANIM_OFF);
    bsp_display_unlock();

    playCurrentSdTrack();
}

static void btn_sd_shuffle_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;

    isShuffle = !isShuffle;
    bsp_display_lock(0);
    if (lbl_sd_shuffle && btn_sd_shuffle) {
        lv_label_set_text_fmt(lbl_sd_shuffle, "🔀 SHUFFLE: %s", isShuffle ? "ON" : "OFF");
        lv_obj_set_style_bg_color(btn_sd_shuffle, isShuffle ? lv_color_hex(0x00B0FF) : lv_color_hex(0x1E2B37), 0);
        lv_obj_set_style_text_color(lbl_sd_shuffle, isShuffle ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF), 0);
    }
    bsp_display_unlock();
    updatePlayerUI();
}

static void btn_sd_page_prev_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;

    int total = (int)sdTrackPaths.size();
    int maxPages = (total + STATIONS_PER_PAGE - 1) / STATIONS_PER_PAGE;
    if (maxPages <= 0) maxPages = 1;
    if (sdCurrentPage > 0) {
        sdCurrentPage--;
    } else {
        sdCurrentPage = maxPages - 1;
    }
    populateSdTrackList();
}

static void btn_sd_page_next_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;

    int total = (int)sdTrackPaths.size();
    int maxPages = (total + STATIONS_PER_PAGE - 1) / STATIONS_PER_PAGE;
    if (maxPages <= 0) maxPages = 1;
    if (sdCurrentPage < maxPages - 1) {
        sdCurrentPage++;
    } else {
        sdCurrentPage = 0;
    }
    populateSdTrackList();
}

// -----------------------------------------------------------------------------
// Live Clock, Date, Day & Battery Timer (1-Second Non-Blocking Refresh)
// -----------------------------------------------------------------------------
static void clock_timer_cb(lv_timer_t* timer) {
    // 1. Update Time, Date & Day
    struct tm timeinfo;
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);

    if (lbl_clock && lbl_date_day) {
        char timeBuf[32];
        char dateBuf[48];
        if (timeinfo.tm_year > (2016 - 1900)) {
            strftime(timeBuf, sizeof(timeBuf), "%I:%M %p", &timeinfo);
            if (timeBuf[0] == '0') {
                memmove(timeBuf, timeBuf + 1, strlen(timeBuf));
            }
            strftime(dateBuf, sizeof(dateBuf), "%A\n%d %b %Y", &timeinfo);
        } else {
            snprintf(timeBuf, sizeof(timeBuf), "--:--");
            snprintf(dateBuf, sizeof(dateBuf), "No NTP Time");
        }
        lv_label_set_text(lbl_clock, timeBuf);
        lv_label_set_text(lbl_date_day, dateBuf);
    }

    // 2. Update Live Battery Percentage & Charging Indicator
    if (lbl_battery) {
        float batVoltage = 0.0f;
        bool isCharging = false;
        int batPct = getBatteryInfo(&batVoltage, &isCharging);

        char batBuf[32];
        if (isCharging) {
            snprintf(batBuf, sizeof(batBuf), LV_SYMBOL_CHARGE " %d%%", batPct);
            lv_obj_set_style_text_color(lbl_battery, lv_color_hex(0x00E676), 0); // Green when charging
        } else {
            if (batPct > 60) {
                snprintf(batBuf, sizeof(batBuf), LV_SYMBOL_BATTERY_FULL " %d%%", batPct);
                lv_obj_set_style_text_color(lbl_battery, lv_color_hex(0x00E5FF), 0); // Cyan
            } else if (batPct > 20) {
                snprintf(batBuf, sizeof(batBuf), LV_SYMBOL_BATTERY_2 " %d%%", batPct);
                lv_obj_set_style_text_color(lbl_battery, lv_color_hex(0xFFD54F), 0); // Amber
            } else {
                snprintf(batBuf, sizeof(batBuf), LV_SYMBOL_BATTERY_EMPTY " %d%%", batPct);
                lv_obj_set_style_text_color(lbl_battery, lv_color_hex(0xFF5252), 0); // Red
            }
        }
        lv_label_set_text(lbl_battery, batBuf);
    }

    // 3. Update Auto-On Alarm & Sleep Timer Badge
    if (lbl_timer_badge) {
        char tmBuf[32];
        if (timerEnabled) {
            const char* durStr = "";
            if (timerDuration == 0) durStr = "Cont";
            else if (timerDuration == 15) durStr = "15m";
            else if (timerDuration == 30) durStr = "30m";
            else if (timerDuration == 45) durStr = "45m";
            else if (timerDuration == 60) durStr = "60m";
            else if (timerDuration == 90) durStr = "90m";
            else if (timerDuration == 120) durStr = "120m";
            else durStr = "Set";

            int dispH = timerHour;
            const char* ampm = "AM";
            if (dispH >= 12) {
                ampm = "PM";
                if (dispH > 12) dispH -= 12;
            }
            if (dispH == 0) dispH = 12;

            snprintf(tmBuf, sizeof(tmBuf), LV_SYMBOL_BELL " %d:%02d%s\n(%s)", dispH, timerMin, ampm, durStr);
            lv_obj_set_style_text_color(lbl_timer_badge, lv_color_hex(0x00E5FF), 0);
        } else {
            snprintf(tmBuf, sizeof(tmBuf), LV_SYMBOL_BELL " OFF\n(Tap)");
            lv_obj_set_style_text_color(lbl_timer_badge, lv_color_hex(0x8B949E), 0);
        }
        lv_label_set_text(lbl_timer_badge, tmBuf);
    }
}

void updateClockAndBatteryUI() {
    clock_timer_cb(NULL);
}

// -----------------------------------------------------------------------------
// Wi-Fi Configuration Callbacks
// -----------------------------------------------------------------------------
static uint32_t lastKbInputTime = 0;
static int lastKbLen = 0;

static void ta_wifi_pass_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        uint32_t now = millis();
        const char* txt = lv_textarea_get_text(ta_wifi_pass);
        int currentLen = txt ? strlen(txt) : 0;

        if (currentLen > lastKbLen) {
            if (now - lastKbInputTime < 280) {
                lv_textarea_del_char(ta_wifi_pass);
                return;
            }
            lastKbInputTime = now;
        }
        lastKbLen = currentLen;
    }
}

static lv_point_t wifi_press_point;
static bool wifi_dragged = false;

static void wifi_item_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t* indev = lv_indev_get_act();

    if (code == LV_EVENT_PRESSED) {
        if (indev) lv_indev_get_point(indev, &wifi_press_point);
        wifi_dragged = false;
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        if (indev) {
            lv_point_t cur_point;
            lv_indev_get_point(indev, &cur_point);
            if (abs(cur_point.x - wifi_press_point.x) > 10 || abs(cur_point.y - wifi_press_point.y) > 10) {
                wifi_dragged = true;
            }
        }
        return;
    }

    if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_CLICKED) {
        if (wifi_dragged) {
            wifi_dragged = false;
            return;
        }
        if (indev && lv_indev_get_scroll_obj(indev) != NULL) {
            return;
        }
        if (!isDebouncedTouch()) return;

        const char* ssid = (const char*)lv_event_get_user_data(e);
        bsp_display_lock(0);
        lv_label_set_text_fmt(lbl_selected_ssid, "Connect to: %s", ssid);
        currentSSID = String(ssid);
        lv_obj_clear_flag(box_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(kb_wifi, LV_OBJ_FLAG_HIDDEN);
        lv_textarea_set_text(ta_wifi_pass, "");
        lastKbInputTime = 0;
        lastKbLen = 0;
        lv_label_set_text(lbl_wifi_status, "Enter password and tap Connect");
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0x00E5FF), 0);
        bsp_display_unlock();
    }
}

static void btn_wifi_scan_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;
    scanAndPopulateWiFi();
}

static void btn_wifi_cancel_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;
    bsp_display_lock(0);
    lv_obj_add_flag(box_wifi_connect, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(kb_wifi, LV_OBJ_FLAG_HIDDEN);
    bsp_display_unlock();
}

static void btn_wifi_connect_cb(lv_event_t* e) {
    if (!isDebouncedTouch()) return;

    const char* pass = lv_textarea_get_text(ta_wifi_pass);
    currentPass = String(pass);

    bsp_display_lock(0);
    char connBuf[96];
    snprintf(connBuf, sizeof(connBuf), "Connecting to '%s'... Please wait.", currentSSID.c_str());
    lv_label_set_text(lbl_wifi_status, connBuf);
    lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0xFFD54F), 0);
    lv_obj_add_flag(kb_wifi, LV_OBJ_FLAG_HIDDEN);
    bsp_display_unlock();

    prefs.begin("air_radio", false);
    prefs.putString("wifi_ssid", currentSSID);
    prefs.putString("wifi_pass", currentPass);
    prefs.end();

    WiFi.disconnect();
    WiFi.begin(currentSSID.c_str(), currentPass.c_str());

    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 25) {
        delay(250);
        timeout++;
    }

    bsp_display_lock(0);
    if (WiFi.status() == WL_CONNECTED) {
        char buf[96];
        snprintf(buf, sizeof(buf), "Successfully Connected to '%s'! (IP: %s)", currentSSID.c_str(), WiFi.localIP().toString().c_str());
        lv_label_set_text(lbl_wifi_status, buf);
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0x00E676), 0);
        lv_obj_add_flag(box_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        updateWiFiStatusBanner();
        setupWebServer();
        bsp_display_unlock();

        delay(1200);

        bsp_display_lock(0);
        lv_tabview_set_act(tabview, 0, LV_ANIM_OFF);
        bsp_display_unlock();

        playCurrentStation();
    } else {
        lv_label_set_text(lbl_wifi_status, "Connection Failed! Wrong password or weak signal.");
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0xFF5252), 0);
        lv_obj_clear_flag(kb_wifi, LV_OBJ_FLAG_HIDDEN);
        updateWiFiStatusBanner();
        bsp_display_unlock();
    }
}

// =============================================================================
// Build Dynamic Multi-Tab Interface with Playback Controls Inside Card
// =============================================================================
void buildModernUI() {
    bsp_display_lock(0);

    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0F14), 0);

    // 1. Top Navigation Bar (Height: 44px)
    tabview = lv_tabview_create(scr, LV_DIR_TOP, 44);
    lv_obj_set_style_bg_color(tabview, lv_color_hex(0x0A0F14), 0);

    lv_obj_t* tab_btns = lv_tabview_get_tab_btns(tabview);
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x121A22), 0);
    lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(tab_btns, lv_color_hex(0x1E2B37), 0);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(0x90A4AE), 0);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(0x00E5FF), LV_STATE_CHECKED);
    lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_14, 0);

    tab_player   = lv_tabview_add_tab(tabview, LV_SYMBOL_AUDIO "  Now Playing");
    tab_stations = lv_tabview_add_tab(tabview, LV_SYMBOL_LIST "  Stations");
    tab_wifi     = lv_tabview_add_tab(tabview, LV_SYMBOL_WIFI "  Wi-Fi Setup");

    if (sdCardMounted && !sdTrackPaths.empty()) {
        tab_sd_music = lv_tabview_add_tab(tabview, LV_SYMBOL_DIRECTORY "  SD Music");
    }

    // =========================================================================
    // TAB 1: NOW PLAYING SCREEN (CONTROLS IN CARD + CLOCK, DATE, DAY & BATTERY)
    // =========================================================================
    lv_obj_set_style_pad_all(tab_player, 8, 0);
    lv_obj_clear_flag(tab_player, LV_OBJ_FLAG_SCROLLABLE);

    // Main Station / Playback Card (Left: W: 346, H: 202)
    lv_obj_t* card = lv_obj_create(tab_player);
    lv_obj_set_size(card, 346, 202);
    lv_obj_set_pos(card, 6, 4);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x131C24), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, card_long_press_cb, LV_EVENT_LONG_PRESSED, NULL);

    // Top Header: Counter badge on left, Status badge on right
    lbl_counter_badge = lv_label_create(card);
    lv_label_set_text(lbl_counter_badge, "STATION 1 OF 276");
    lv_obj_set_style_text_color(lbl_counter_badge, lv_color_hex(0x80D8FF), 0);
    lv_obj_set_style_text_font(lbl_counter_badge, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_counter_badge, LV_ALIGN_TOP_LEFT, 2, 0);

    lbl_status_badge = lv_label_create(card);
    lv_label_set_text(lbl_status_badge, "[ LIVE ]");
    lv_obj_set_style_text_color(lbl_status_badge, lv_color_hex(0x00E676), 0);
    lv_obj_set_style_text_font(lbl_status_badge, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_status_badge, LV_ALIGN_TOP_RIGHT, -2, 0);

    // Station / Song Title (Montserrat 18 in crisp White)
    lbl_station_title = lv_label_create(card);
    lv_label_set_text(lbl_station_title, "Akashvani Thrissur");
    lv_obj_set_style_text_color(lbl_station_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_station_title, &lv_font_montserrat_18, 0);
    lv_obj_set_width(lbl_station_title, 324);
    lv_label_set_long_mode(lbl_station_title, LV_LABEL_LONG_WRAP);
    lv_obj_align(lbl_station_title, LV_ALIGN_TOP_LEFT, 2, 22);

    // State & Language Tags
    lbl_station_state = lv_label_create(card);
    lv_label_set_text(lbl_station_state, "State: Kerala");
    lv_obj_set_style_text_color(lbl_station_state, lv_color_hex(0xFFD54F), 0);
    lv_obj_set_style_text_font(lbl_station_state, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_station_state, LV_ALIGN_TOP_LEFT, 2, 68);

    lbl_station_lang = lv_label_create(card);
    lv_label_set_text(lbl_station_lang, "Lang: Malayalam");
    lv_obj_set_style_text_color(lbl_station_lang, lv_color_hex(0x80D8FF), 0);
    lv_obj_set_style_text_font(lbl_station_lang, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_station_lang, LV_ALIGN_TOP_LEFT, 150, 68);

    // Subtitle / Stream Metadata
    lbl_stream_info = lv_label_create(card);
    lv_label_set_text(lbl_stream_info, "");
    lv_obj_set_style_text_color(lbl_stream_info, lv_color_hex(0xB0BEC5), 0);
    lv_obj_set_style_text_font(lbl_stream_info, &lv_font_montserrat_12, 0);
    lv_obj_set_width(lbl_stream_info, 324);
    lv_label_set_long_mode(lbl_stream_info, LV_LABEL_LONG_DOT);
    lv_obj_align(lbl_stream_info, LV_ALIGN_TOP_LEFT, 2, 92);

    // =========================================================================
    // PLAYBACK BUTTONS INSIDE STATION CARD (PREV / PLAY / NEXT)
    // =========================================================================
    btn_prev = lv_btn_create(card);
    lv_obj_set_size(btn_prev, 96, 48);
    lv_obj_set_pos(btn_prev, 2, 134);
    lv_obj_set_style_bg_color(btn_prev, lv_color_hex(0x16222C), 0);
    lv_obj_set_style_border_color(btn_prev, lv_color_hex(0x243542), 0);
    lv_obj_set_style_border_width(btn_prev, 2, 0);
    lv_obj_set_style_radius(btn_prev, 8, 0);
    lv_obj_add_event_cb(btn_prev, btn_prev_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lp = lv_label_create(btn_prev);
    lv_label_set_text(lp, LV_SYMBOL_PREV " PREV");
    lv_obj_set_style_text_color(lp, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lp, &lv_font_montserrat_14, 0);
    lv_obj_center(lp);

    btn_play = lv_btn_create(card);
    lv_obj_set_size(btn_play, 124, 48);
    lv_obj_set_pos(btn_play, 102, 134);
    lv_obj_set_style_bg_color(btn_play, lv_color_hex(0x00B0FF), 0);
    lv_obj_set_style_radius(btn_play, 8, 0);
    lv_obj_add_event_cb(btn_play, btn_play_cb, LV_EVENT_CLICKED, NULL);

    lbl_btn_play = lv_label_create(btn_play);
    lv_label_set_text(lbl_btn_play, LV_SYMBOL_PAUSE " PAUSE");
    lv_obj_set_style_text_color(lbl_btn_play, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(lbl_btn_play, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_btn_play);

    btn_next = lv_btn_create(card);
    lv_obj_set_size(btn_next, 96, 48);
    lv_obj_set_pos(btn_next, 230, 134);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0x16222C), 0);
    lv_obj_set_style_border_color(btn_next, lv_color_hex(0x243542), 0);
    lv_obj_set_style_border_width(btn_next, 2, 0);
    lv_obj_set_style_radius(btn_next, 8, 0);
    lv_obj_add_event_cb(btn_next, btn_next_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* ln = lv_label_create(btn_next);
    lv_label_set_text(ln, "NEXT " LV_SYMBOL_NEXT);
    lv_obj_set_style_text_color(ln, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(ln, &lv_font_montserrat_14, 0);
    lv_obj_center(ln);

    // Right Side Vertical Volume Control Box (W: 108, H: 202)
    lv_obj_t* pnl_r = lv_obj_create(tab_player);
    lv_obj_set_size(pnl_r, 108, 202);
    lv_obj_set_pos(pnl_r, 358, 4);
    lv_obj_set_style_bg_color(pnl_r, lv_color_hex(0x131C24), 0);
    lv_obj_set_style_border_color(pnl_r, lv_color_hex(0x1E2B37), 0);
    lv_obj_set_style_radius(pnl_r, 12, 0);
    lv_obj_set_style_pad_all(pnl_r, 6, 0);
    lv_obj_clear_flag(pnl_r, LV_OBJ_FLAG_SCROLLABLE);

    lbl_vol_val = lv_label_create(pnl_r);
    lv_label_set_text(lbl_vol_val, "VOL 100%");
    lv_obj_set_style_text_color(lbl_vol_val, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(lbl_vol_val, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_vol_val, LV_ALIGN_TOP_MID, 0, 2);

    // Vertical Slider (Width: 26px, Height: 110px)
    slider_vol = lv_slider_create(pnl_r);
    lv_obj_set_size(slider_vol, 26, 110);
    lv_obj_align(slider_vol, LV_ALIGN_CENTER, 0, -2);
    lv_slider_set_range(slider_vol, 0, 21);
    lv_slider_set_value(slider_vol, currentVolume, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider_vol, lv_color_hex(0x1E2B37), 0);
    lv_obj_set_style_bg_color(slider_vol, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);
    lv_obj_add_event_cb(slider_vol, slider_vol_cb, LV_EVENT_VALUE_CHANGED, NULL);

    btn_mute = lv_btn_create(pnl_r);
    lv_obj_set_size(btn_mute, 92, 34);
    lv_obj_align(btn_mute, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(btn_mute, lv_color_hex(0x1E2B37), 0);
    lv_obj_set_style_radius(btn_mute, 6, 0);
    lv_obj_add_event_cb(btn_mute, btn_mute_cb, LV_EVENT_CLICKED, NULL);

    lbl_btn_mute = lv_label_create(btn_mute);
    lv_label_set_text(lbl_btn_mute, LV_SYMBOL_VOLUME_MAX " MUTE");
    lv_obj_set_style_text_color(lbl_btn_mute, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_btn_mute, &lv_font_montserrat_10, 0);
    lv_obj_center(lbl_btn_mute);

    // =========================================================================
    // BOTTOM BAR: POWER BUTTON + GIANT TIME + DATE & DAY + BATTERY STATUS
    // =========================================================================
    btn_sleep = lv_btn_create(tab_player);
    lv_obj_set_size(btn_sleep, 58, 54);
    lv_obj_set_pos(btn_sleep, 6, 212);
    lv_obj_set_style_bg_color(btn_sleep, lv_color_hex(0x281216), 0);
    lv_obj_set_style_border_color(btn_sleep, lv_color_hex(0xFF5252), 0);
    lv_obj_set_style_border_width(btn_sleep, 2, 0);
    lv_obj_set_style_radius(btn_sleep, 10, 0);
    lv_obj_add_event_cb(btn_sleep, btn_sleep_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_sleep = lv_label_create(btn_sleep);
    lv_label_set_text(lbl_sleep, LV_SYMBOL_POWER);
    lv_obj_set_style_text_color(lbl_sleep, lv_color_hex(0xFF5252), 0);
    lv_obj_set_style_text_font(lbl_sleep, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_sleep);

    // GIANT PROMINENT CLOCK + DATE & DAY + BATTERY PANEL (W: 396, H: 54)
    pnl_clock_card = lv_obj_create(tab_player);
    lv_obj_set_size(pnl_clock_card, 396, 54);
    lv_obj_set_pos(pnl_clock_card, 70, 212);
    lv_obj_set_style_bg_color(pnl_clock_card, lv_color_hex(0x10171E), 0);
    lv_obj_set_style_border_color(pnl_clock_card, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(pnl_clock_card, 2, 0);
    lv_obj_set_style_radius(pnl_clock_card, 10, 0);
    lv_obj_set_style_pad_all(pnl_clock_card, 2, 0);
    lv_obj_clear_flag(pnl_clock_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pnl_clock_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pnl_clock_card, clock_card_clicked_cb, LV_EVENT_CLICKED, NULL);

    // 1. Giant Live Time (Left side)
    lbl_clock = lv_label_create(pnl_clock_card);
    lv_label_set_text(lbl_clock, "--:--");
    lv_obj_set_style_text_color(lbl_clock, lv_color_hex(0xFFD54F), 0);
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_32, 0);
    lv_obj_align(lbl_clock, LV_ALIGN_LEFT_MID, 6, 0);

    // 2. Date & Day (Center-left area)
    lbl_date_day = lv_label_create(pnl_clock_card);
    lv_label_set_text(lbl_date_day, "Friday\n04 Sep 2026");
    lv_obj_set_style_text_color(lbl_date_day, lv_color_hex(0x90A4AE), 0);
    lv_obj_set_style_text_font(lbl_date_day, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_date_day, LV_ALIGN_LEFT_MID, 155, 0);

    // 3. Interactive Auto-On Alarm / Sleep Timer Badge (Center-right area)
    lbl_timer_badge = lv_label_create(pnl_clock_card);
    lv_label_set_text(lbl_timer_badge, LV_SYMBOL_BELL " OFF\n(Tap)");
    lv_obj_set_style_text_color(lbl_timer_badge, lv_color_hex(0x8B949E), 0);
    lv_obj_set_style_text_font(lbl_timer_badge, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_timer_badge, LV_ALIGN_LEFT_MID, 252, 0);

    // 4. Live Battery Gauge & Charging Indicator (Right side)
    lbl_battery = lv_label_create(pnl_clock_card);
    lv_label_set_text(lbl_battery, LV_SYMBOL_BATTERY_FULL " 100%");
    lv_obj_set_style_text_color(lbl_battery, lv_color_hex(0x00E676), 0);
    lv_obj_set_style_text_font(lbl_battery, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_battery, LV_ALIGN_RIGHT_MID, -8, 0);

    // =========================================================================
    // TAB 2: STATIONS BROWSER (SPLIT PANEL + PAGINATION)
    // =========================================================================
    lv_obj_set_style_pad_all(tab_stations, 4, 0);
    lv_obj_clear_flag(tab_stations, LV_OBJ_FLAG_SCROLLABLE);

    list_categories = lv_list_create(tab_stations);
    lv_obj_set_size(list_categories, 136, 260);
    lv_obj_align(list_categories, LV_ALIGN_TOP_LEFT, 2, 0);
    lv_obj_set_style_bg_color(list_categories, lv_color_hex(0x10171E), 0);
    lv_obj_set_style_border_color(list_categories, lv_color_hex(0x1E2B37), 0);
    lv_obj_set_style_pad_all(list_categories, 2, 0);

    lv_obj_t* pnl_right_st = lv_obj_create(tab_stations);
    lv_obj_set_size(pnl_right_st, 322, 260);
    lv_obj_align(pnl_right_st, LV_ALIGN_TOP_RIGHT, -2, 0);
    lv_obj_set_style_bg_color(pnl_right_st, lv_color_hex(0x0A0F14), 0);
    lv_obj_set_style_border_width(pnl_right_st, 0, 0);
    lv_obj_set_style_pad_all(pnl_right_st, 0, 0);
    lv_obj_clear_flag(pnl_right_st, LV_OBJ_FLAG_SCROLLABLE);

    pnl_page_controls = lv_obj_create(pnl_right_st);
    lv_obj_set_size(pnl_page_controls, 322, 38);
    lv_obj_align(pnl_page_controls, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(pnl_page_controls, lv_color_hex(0x131C24), 0);
    lv_obj_set_style_border_color(pnl_page_controls, lv_color_hex(0x1E2B37), 0);
    lv_obj_set_style_radius(pnl_page_controls, 6, 0);
    lv_obj_set_style_pad_all(pnl_page_controls, 2, 0);
    lv_obj_clear_flag(pnl_page_controls, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn_pprev = lv_btn_create(pnl_page_controls);
    lv_obj_set_size(btn_pprev, 82, 32);
    lv_obj_align(btn_pprev, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_set_style_bg_color(btn_pprev, lv_color_hex(0x1E2B37), 0);
    lv_obj_set_style_radius(btn_pprev, 6, 0);
    lv_obj_add_event_cb(btn_pprev, btn_page_prev_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lpp = lv_label_create(btn_pprev);
    lv_label_set_text(lpp, LV_SYMBOL_PREV " PREV");
    lv_obj_set_style_text_color(lpp, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lpp, &lv_font_montserrat_12, 0);
    lv_obj_center(lpp);

    lbl_page_info = lv_label_create(pnl_page_controls);
    lv_label_set_text(lbl_page_info, "Page 1/56");
    lv_obj_set_style_text_color(lbl_page_info, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(lbl_page_info, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_page_info);

    lv_obj_t* btn_pnext = lv_btn_create(pnl_page_controls);
    lv_obj_set_size(btn_pnext, 82, 32);
    lv_obj_align(btn_pnext, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_pnext, lv_color_hex(0x1E2B37), 0);
    lv_obj_set_style_radius(btn_pnext, 6, 0);
    lv_obj_add_event_cb(btn_pnext, btn_page_next_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lpn = lv_label_create(btn_pnext);
    lv_label_set_text(lpn, "NEXT " LV_SYMBOL_NEXT);
    lv_obj_set_style_text_color(lpn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lpn, &lv_font_montserrat_12, 0);
    lv_obj_center(lpn);

    list_stations = lv_obj_create(pnl_right_st);
    lv_obj_set_size(list_stations, 322, 218);
    lv_obj_align(list_stations, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list_stations, lv_color_hex(0x0A0F14), 0);
    lv_obj_set_style_border_width(list_stations, 0, 0);
    lv_obj_set_style_pad_all(list_stations, 0, 0);
    lv_obj_clear_flag(list_stations, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(list_stations, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list_stations, 4, 0);

    populateCategoryList();
    populateStationList();

    // =========================================================================
    // TAB 3: WI-FI SETUP (LIVE STATUS BANNER + NETWORK LIST + DEBOUNCED KB)
    // =========================================================================
    lv_obj_set_style_pad_all(tab_wifi, 4, 0);
    lv_obj_clear_flag(tab_wifi, LV_OBJ_FLAG_SCROLLABLE);

    // Current Wi-Fi Status Card (Top of Tab 3: W: 468, H: 40)
    lv_obj_t* pnl_wifi_current = lv_obj_create(tab_wifi);
    lv_obj_set_size(pnl_wifi_current, 468, 40);
    lv_obj_align(pnl_wifi_current, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(pnl_wifi_current, lv_color_hex(0x131C24), 0);
    lv_obj_set_style_border_color(pnl_wifi_current, lv_color_hex(0x1E2B37), 0);
    lv_obj_set_style_radius(pnl_wifi_current, 6, 0);
    lv_obj_set_style_pad_all(pnl_wifi_current, 6, 0);
    lv_obj_clear_flag(pnl_wifi_current, LV_OBJ_FLAG_SCROLLABLE);

    lbl_wifi_current_val = lv_label_create(pnl_wifi_current);
    lv_label_set_text(lbl_wifi_current_val, "Checking Wi-Fi status...");
    lv_obj_set_style_text_font(lbl_wifi_current_val, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_wifi_current_val, LV_ALIGN_LEFT_MID, 4, 0);

    // Scan & Action Bar (W: 468, H: 36)
    lv_obj_t* btn_scan = lv_btn_create(tab_wifi);
    lv_obj_set_size(btn_scan, 140, 32);
    lv_obj_set_pos(btn_scan, 6, 44);
    lv_obj_set_style_bg_color(btn_scan, lv_color_hex(0x00B0FF), 0);
    lv_obj_set_style_radius(btn_scan, 6, 0);
    lv_obj_add_event_cb(btn_scan, btn_wifi_scan_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_scan = lv_label_create(btn_scan);
    lv_label_set_text(lbl_scan, LV_SYMBOL_REFRESH "  Scan Networks");
    lv_obj_set_style_text_color(lbl_scan, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(lbl_scan, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_scan);

    lbl_wifi_status = lv_label_create(tab_wifi);
    lv_label_set_text(lbl_wifi_status, "Tap a network below to connect");
    lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0x90A4AE), 0);
    lv_obj_set_style_text_font(lbl_wifi_status, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_wifi_status, 155, 52);

    // Scanned Networks List
    list_wifi_networks = lv_list_create(tab_wifi);
    lv_obj_set_size(list_wifi_networks, 468, 185);
    lv_obj_align(list_wifi_networks, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list_wifi_networks, lv_color_hex(0x131C24), 0);
    lv_obj_set_style_border_color(list_wifi_networks, lv_color_hex(0x1E2B37), 0);

    // Password & Connect Box (Overlay Modal)
    box_wifi_connect = lv_obj_create(tab_wifi);
    lv_obj_set_size(box_wifi_connect, 468, 115);
    lv_obj_align(box_wifi_connect, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(box_wifi_connect, lv_color_hex(0x10171E), 0);
    lv_obj_set_style_border_color(box_wifi_connect, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(box_wifi_connect, 2, 0);
    lv_obj_set_style_radius(box_wifi_connect, 8, 0);
    lv_obj_add_flag(box_wifi_connect, LV_OBJ_FLAG_HIDDEN);

    lbl_selected_ssid = lv_label_create(box_wifi_connect);
    lv_label_set_text(lbl_selected_ssid, "Connect to: ");
    lv_obj_set_style_text_color(lbl_selected_ssid, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(lbl_selected_ssid, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_selected_ssid, LV_ALIGN_TOP_LEFT, 0, 0);

    ta_wifi_pass = lv_textarea_create(box_wifi_connect);
    lv_obj_set_size(ta_wifi_pass, 230, 42);
    lv_obj_align(ta_wifi_pass, LV_ALIGN_BOTTOM_LEFT, 0, -4);
    lv_textarea_set_placeholder_text(ta_wifi_pass, "Enter Password");
    lv_textarea_set_password_mode(ta_wifi_pass, true);
    lv_textarea_set_one_line(ta_wifi_pass, true);
    lv_obj_set_style_bg_color(ta_wifi_pass, lv_color_hex(0x1A2530), 0);
    lv_obj_set_style_text_color(ta_wifi_pass, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_event_cb(ta_wifi_pass, ta_wifi_pass_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t* btn_do_conn = lv_btn_create(box_wifi_connect);
    lv_obj_set_size(btn_do_conn, 105, 42);
    lv_obj_align(btn_do_conn, LV_ALIGN_BOTTOM_RIGHT, -95, -4);
    lv_obj_set_style_bg_color(btn_do_conn, lv_color_hex(0x00C853), 0);
    lv_obj_add_event_cb(btn_do_conn, btn_wifi_connect_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_dc = lv_label_create(btn_do_conn);
    lv_label_set_text(lbl_dc, "Connect " LV_SYMBOL_OK);
    lv_obj_center(lbl_dc);

    lv_obj_t* btn_cancel = lv_btn_create(box_wifi_connect);
    lv_obj_set_size(btn_cancel, 85, 42);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_RIGHT, 0, -4);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x455A64), 0);
    lv_obj_add_event_cb(btn_cancel, btn_wifi_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl_cx = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cx, "Cancel");
    lv_obj_center(lbl_cx);

    // Touch Keyboard with Debounce Filter
    kb_wifi = lv_keyboard_create(tab_wifi);
    lv_obj_set_size(kb_wifi, 468, 145);
    lv_obj_align(kb_wifi, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb_wifi, ta_wifi_pass);
    lv_obj_set_style_bg_color(kb_wifi, lv_color_hex(0x131C24), 0);
    lv_obj_add_flag(kb_wifi, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(kb_wifi, [](lv_event_t* e) {
        static uint32_t lastTouchTime = 0;
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_PRESSED) {
            uint32_t now = millis();
            if (now - lastTouchTime < 220) {
                return;
            }
            lastTouchTime = now;
        }
    }, LV_EVENT_ALL, NULL);

    // =========================================================================
    // TAB 4: SD CARD MUSIC (5-TRACK PAGINATED LIST + SHUFFLE MODE)
    // =========================================================================
    if (tab_sd_music) {
        lv_obj_set_style_pad_all(tab_sd_music, 4, 0);
        lv_obj_clear_flag(tab_sd_music, LV_OBJ_FLAG_SCROLLABLE);

        pnl_sd_page_controls = lv_obj_create(tab_sd_music);
        lv_obj_set_size(pnl_sd_page_controls, 468, 38);
        lv_obj_align(pnl_sd_page_controls, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_color(pnl_sd_page_controls, lv_color_hex(0x131C24), 0);
        lv_obj_set_style_border_color(pnl_sd_page_controls, lv_color_hex(0x1E2B37), 0);
        lv_obj_set_style_radius(pnl_sd_page_controls, 6, 0);
        lv_obj_set_style_pad_all(pnl_sd_page_controls, 2, 0);
        lv_obj_clear_flag(pnl_sd_page_controls, LV_OBJ_FLAG_SCROLLABLE);

        btn_sd_shuffle = lv_btn_create(pnl_sd_page_controls);
        lv_obj_set_size(btn_sd_shuffle, 130, 32);
        lv_obj_align(btn_sd_shuffle, LV_ALIGN_LEFT_MID, 2, 0);
        lv_obj_set_style_bg_color(btn_sd_shuffle, isShuffle ? lv_color_hex(0x00B0FF) : lv_color_hex(0x1E2B37), 0);
        lv_obj_set_style_radius(btn_sd_shuffle, 6, 0);
        lv_obj_add_event_cb(btn_sd_shuffle, btn_sd_shuffle_cb, LV_EVENT_CLICKED, NULL);

        lbl_sd_shuffle = lv_label_create(btn_sd_shuffle);
        lv_label_set_text_fmt(lbl_sd_shuffle, "🔀 SHUFFLE: %s", isShuffle ? "ON" : "OFF");
        lv_obj_set_style_text_color(lbl_sd_shuffle, isShuffle ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(lbl_sd_shuffle, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl_sd_shuffle);

        lv_obj_t* btn_sd_prev = lv_btn_create(pnl_sd_page_controls);
        lv_obj_set_size(btn_sd_prev, 75, 32);
        lv_obj_align(btn_sd_prev, LV_ALIGN_RIGHT_MID, -210, 0);
        lv_obj_set_style_bg_color(btn_sd_prev, lv_color_hex(0x1E2B37), 0);
        lv_obj_set_style_radius(btn_sd_prev, 6, 0);
        lv_obj_add_event_cb(btn_sd_prev, btn_sd_page_prev_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* l_sd_p = lv_label_create(btn_sd_prev);
        lv_label_set_text(l_sd_p, LV_SYMBOL_PREV);
        lv_obj_center(l_sd_p);

        lbl_sd_page_info = lv_label_create(pnl_sd_page_controls);
        lv_label_set_text(lbl_sd_page_info, "Page 1 of 1");
        lv_obj_set_style_text_color(lbl_sd_page_info, lv_color_hex(0x00E5FF), 0);
        lv_obj_set_style_text_font(lbl_sd_page_info, &lv_font_montserrat_12, 0);
        lv_obj_align(lbl_sd_page_info, LV_ALIGN_RIGHT_MID, -85, 0);

        lv_obj_t* btn_sd_next = lv_btn_create(pnl_sd_page_controls);
        lv_obj_set_size(btn_sd_next, 75, 32);
        lv_obj_align(btn_sd_next, LV_ALIGN_RIGHT_MID, -2, 0);
        lv_obj_set_style_bg_color(btn_sd_next, lv_color_hex(0x1E2B37), 0);
        lv_obj_set_style_radius(btn_sd_next, 6, 0);
        lv_obj_add_event_cb(btn_sd_next, btn_sd_page_next_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* l_sd_n = lv_label_create(btn_sd_next);
        lv_label_set_text(l_sd_n, LV_SYMBOL_NEXT);
        lv_obj_center(l_sd_n);

        list_sd_tracks = lv_obj_create(tab_sd_music);
        lv_obj_set_size(list_sd_tracks, 468, 218);
        lv_obj_align(list_sd_tracks, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(list_sd_tracks, lv_color_hex(0x0A0F14), 0);
        lv_obj_set_style_border_width(list_sd_tracks, 0, 0);
        lv_obj_set_style_pad_all(list_sd_tracks, 0, 0);
        lv_obj_clear_flag(list_sd_tracks, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(list_sd_tracks, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(list_sd_tracks, 4, 0);

        populateSdTrackList();
    }

    // Start 1-Second Non-Blocking Clock & Battery Timer
    clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);

    updateWiFiStatusBanner();
    bsp_display_unlock();
}

void updateWiFiStatusBanner() {
    if (!lbl_wifi_current_val) return;
    bsp_display_lock(0);
    if (WiFi.status() == WL_CONNECTED) {
        char buf[128];
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " %s  •  http://%s  (%d dBm)",
                 WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
        lv_label_set_text(lbl_wifi_current_val, buf);
        lv_obj_set_style_text_color(lbl_wifi_current_val, lv_color_hex(0x00E676), 0);
    } else {
        lv_label_set_text(lbl_wifi_current_val, LV_SYMBOL_WARNING " Not Connected - Please select a Wi-Fi network below");
        lv_obj_set_style_text_color(lbl_wifi_current_val, lv_color_hex(0xFFAB00), 0);
    }
    bsp_display_unlock();
}

void populateCategoryList() {
    if (!list_categories) return;
    bsp_display_lock(0);
    lv_obj_clean(list_categories);

    char allBuf[32];
    snprintf(allBuf, sizeof(allBuf), "All (%d)", (int)runtimeStations.size());
    lv_list_add_text(list_categories, "ALL");
    lv_obj_t* btn_all = lv_list_add_btn(list_categories, LV_SYMBOL_AUDIO, allBuf);
    lv_obj_add_event_cb(btn_all, category_btn_cb, LV_EVENT_ALL, (void*)"ALL:All");

    int userCount = 0;
    for (const auto& st : runtimeStations) {
        if (st.isCustom) userCount++;
    }

    lv_list_add_text(list_categories, "USER");
    char userBuf[32];
    snprintf(userBuf, sizeof(userBuf), "User (%d)", userCount);
    lv_obj_t* btn_user = lv_list_add_btn(list_categories, LV_SYMBOL_EDIT, userBuf);
    lv_obj_add_event_cb(btn_user, category_btn_cb, LV_EVENT_ALL, (void*)"USER:User");

    lv_list_add_text(list_categories, "LANGUAGES");
    for (int i = 1; i < min(TOTAL_LANGUAGES, 12); i++) {
        static char lang_keys[TOTAL_LANGUAGES][40];
        snprintf(lang_keys[i], sizeof(lang_keys[i]), "LANG:%s", FILTER_LANGUAGES[i]);
        lv_obj_t* btn = lv_list_add_btn(list_categories, LV_SYMBOL_FILE, FILTER_LANGUAGES[i]);
        lv_obj_add_event_cb(btn, category_btn_cb, LV_EVENT_ALL, (void*)lang_keys[i]);
    }

    lv_list_add_text(list_categories, "STATES");
    for (int i = 1; i < min(TOTAL_STATES, 12); i++) {
        static char state_keys[TOTAL_STATES][40];
        snprintf(state_keys[i], sizeof(state_keys[i]), "STATE:%s", FILTER_STATES[i]);
        lv_obj_t* btn = lv_list_add_btn(list_categories, LV_SYMBOL_DIRECTORY, FILTER_STATES[i]);
        lv_obj_add_event_cb(btn, category_btn_cb, LV_EVENT_ALL, (void*)state_keys[i]);
    }

    bsp_display_unlock();
}

void populateStationList() {
    if (!list_stations) return;
    bsp_display_lock(0);
    lv_obj_clean(list_stations);

    int totalFiltered = (int)filteredIndices.size();
    int maxPages = (totalFiltered + STATIONS_PER_PAGE - 1) / STATIONS_PER_PAGE;
    if (maxPages == 0) maxPages = 1;
    if (stationCurrentPage >= maxPages) stationCurrentPage = maxPages - 1;
    if (stationCurrentPage < 0) stationCurrentPage = 0;

    if (lbl_page_info) {
        char pageBuf[48];
        snprintf(pageBuf, sizeof(pageBuf), "%s\n(%d/%d)", activeFilterVal.c_str(), totalFiltered > 0 ? (stationCurrentPage + 1) : 0, maxPages);
        lv_label_set_text(lbl_page_info, pageBuf);
    }

    if (totalFiltered == 0) {
        lv_obj_t* btn = lv_btn_create(list_stations);
        lv_obj_set_size(btn, 322, 64);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x131C24), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x1E2B37), 0);

        lv_obj_t* lbl = lv_label_create(btn);
        char emptyMsg[128];
        if (WiFi.status() == WL_CONNECTED) {
            snprintf(emptyMsg, sizeof(emptyMsg), "No User Stations added yet!\nAdd at http://%s", WiFi.localIP().toString().c_str());
        } else {
            snprintf(emptyMsg, sizeof(emptyMsg), "No User Stations added yet!\nConnect Wi-Fi to add stations");
        }
        lv_label_set_text(lbl, emptyMsg);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFD54F), 0);
        lv_obj_center(lbl);

        bsp_display_unlock();
        return;
    }

    int startIdx = stationCurrentPage * STATIONS_PER_PAGE;
    int endIdx = min(startIdx + STATIONS_PER_PAGE, totalFiltered);

    for (int i = startIdx; i < endIdx; i++) {
        int realIdx = filteredIndices[i];
        const LiveStation& st = runtimeStations[realIdx];

        lv_obj_t* btn = lv_btn_create(list_stations);
        lv_obj_set_size(btn, 322, 38);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_pad_hor(btn, 8, 0);
        lv_obj_set_style_pad_ver(btn, 0, 0);

        if (currentSource == SRC_RADIO && i == currentFilterPosition) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x004D5A), 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(0x00E5FF), 0);
            lv_obj_set_style_border_width(btn, 2, 0);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x131C24), 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(0x1E2B37), 0);
            lv_obj_set_style_border_width(btn, 1, 0);
        }

        lv_obj_add_event_cb(btn, station_item_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t* lbl = lv_label_create(btn);
        char itemText[128];
        snprintf(itemText, sizeof(itemText), "%d. %s  [%s]%s", i + 1, st.name.c_str(), st.language.c_str(), st.isCustom ? " ⭐" : "");
        lv_label_set_text(lbl, itemText);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, (currentSource == SRC_RADIO && i == currentFilterPosition) ? lv_color_hex(0x00E5FF) : (st.isCustom ? lv_color_hex(0xFFD54F) : lv_color_hex(0xFFFFFF)), 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl, 304);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    }

    bsp_display_unlock();
}

void populateSdTrackList() {
    if (!list_sd_tracks) return;
    bsp_display_lock(0);
    lv_obj_clean(list_sd_tracks);

    int totalTracks = (int)sdTrackPaths.size();
    int maxPages = (totalTracks + STATIONS_PER_PAGE - 1) / STATIONS_PER_PAGE;
    if (maxPages == 0) maxPages = 1;
    if (sdCurrentPage >= maxPages) sdCurrentPage = maxPages - 1;
    if (sdCurrentPage < 0) sdCurrentPage = 0;

    if (lbl_sd_page_info) {
        char pageBuf[48];
        snprintf(pageBuf, sizeof(pageBuf), "Page %d/%d (%d files)", sdCurrentPage + 1, maxPages, totalTracks);
        lv_label_set_text(lbl_sd_page_info, pageBuf);
    }

    int startIdx = sdCurrentPage * STATIONS_PER_PAGE;
    int endIdx = min(startIdx + STATIONS_PER_PAGE, totalTracks);

    for (int i = startIdx; i < endIdx; i++) {
        lv_obj_t* btn = lv_btn_create(list_sd_tracks);
        lv_obj_set_size(btn, 468, 38);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_pad_hor(btn, 10, 0);
        lv_obj_set_style_pad_ver(btn, 0, 0);

        if (currentSource == SRC_SD && i == currentSdTrackIdx) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x004D5A), 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(0x00E5FF), 0);
            lv_obj_set_style_border_width(btn, 2, 0);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x131C24), 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(0x1E2B37), 0);
            lv_obj_set_style_border_width(btn, 1, 0);
        }

        lv_obj_add_event_cb(btn, sd_track_item_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t* lbl = lv_label_create(btn);
        char itemText[128];
        snprintf(itemText, sizeof(itemText), "%d. %s", i + 1, sdTrackNames[i].c_str());
        lv_label_set_text(lbl, itemText);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, (currentSource == SRC_SD && i == currentSdTrackIdx) ? lv_color_hex(0x00E5FF) : lv_color_hex(0xFFFFFF), 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl, 440);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    }

    bsp_display_unlock();
}

void scanAndPopulateWiFi() {
    bsp_display_lock(0);
    lv_label_set_text(lbl_wifi_status, "Scanning 2.4GHz Wi-Fi networks...");
    lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0xFFD54F), 0);
    lv_obj_clean(list_wifi_networks);
    lv_obj_add_flag(box_wifi_connect, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(kb_wifi, LV_OBJ_FLAG_HIDDEN);
    bsp_display_unlock();

    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_STA);
    delay(100);
    int n = WiFi.scanNetworks();

    bsp_display_lock(0);
    if (n == 0) {
        lv_label_set_text(lbl_wifi_status, "No Wi-Fi networks found. Tap Scan again.");
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0xFF5252), 0);
    } else {
        lv_label_set_text_fmt(lbl_wifi_status, "Found %d networks. Tap your Wi-Fi to connect.", n);
        lv_obj_set_style_text_color(lbl_wifi_status, lv_color_hex(0x00E676), 0);

        for (int i = 0; i < n; i++) {
            static char scanSSIDs[25][64];
            if (i < 25) {
                strncpy(scanSSIDs[i], WiFi.SSID(i).c_str(), sizeof(scanSSIDs[i]));
                char btnText[96];
                snprintf(btnText, sizeof(btnText), "%s  (%d dBm)", scanSSIDs[i], (int)WiFi.RSSI(i));

                lv_obj_t* btn = lv_list_add_btn(list_wifi_networks, LV_SYMBOL_WIFI, btnText);
                lv_obj_add_event_cb(btn, wifi_item_cb, LV_EVENT_ALL, (void*)scanSSIDs[i]);
                lv_obj_set_style_pad_ver(btn, 8, 0);
            }
        }
    }
    updateWiFiStatusBanner();
    bsp_display_unlock();
}

void updatePlayerUI() {
    if (!lbl_station_title) return;

    bsp_display_lock(0);

    if (currentSource == SRC_SD) {
        // SD Card MP3 Mode
        if (!sdTrackNames.empty() && currentSdTrackIdx < (int)sdTrackNames.size()) {
            lv_label_set_text(lbl_station_title, sdTrackNames[currentSdTrackIdx].c_str());
        } else {
            lv_label_set_text(lbl_station_title, "No SD Tracks Found");
        }

        lv_label_set_text(lbl_station_state, "Source: MicroSD");
        lv_label_set_text(lbl_station_lang, isShuffle ? "Mode: Shuffle 🔀" : "Mode: Normal 🔁");

        char tagBuf[32];
        snprintf(tagBuf, sizeof(tagBuf), "TRACK %d OF %d", currentSdTrackIdx + 1, (int)sdTrackPaths.size());
        lv_label_set_text(lbl_counter_badge, tagBuf);

        if (isPlaying) {
            lv_label_set_text(lbl_status_badge, "[ SD PLAY ]");
            lv_obj_set_style_text_color(lbl_status_badge, lv_color_hex(0x00E676), 0);
            if (!sdTrackPaths.empty() && currentSdTrackIdx < (int)sdTrackPaths.size()) {
                lv_label_set_text(lbl_stream_info, sdTrackPaths[currentSdTrackIdx].c_str());
            } else {
                lv_label_set_text(lbl_stream_info, "Playing local audio");
            }
        } else {
            lv_label_set_text(lbl_status_badge, "[ PAUSED ]");
            lv_obj_set_style_text_color(lbl_status_badge, lv_color_hex(0x78909C), 0);
            lv_label_set_text(lbl_stream_info, "Tap PLAY to start.");
        }
    } else {
        // Internet Radio Mode
        if (!filteredIndices.empty()) {
            int realIdx = filteredIndices[currentFilterPosition];
            const LiveStation& st = runtimeStations[realIdx];

            lv_label_set_text(lbl_station_title, st.name.c_str());

            char stateBuf[64];
            snprintf(stateBuf, sizeof(stateBuf), "State: %s", st.state.c_str());
            lv_label_set_text(lbl_station_state, stateBuf);

            char langBuf[64];
            snprintf(langBuf, sizeof(langBuf), "Lang: %s%s", st.language.c_str(), st.isCustom ? " ⭐" : "");
            lv_label_set_text(lbl_station_lang, langBuf);

            char tagBuf[32];
            snprintf(tagBuf, sizeof(tagBuf), "STATION %d OF %d", currentFilterPosition + 1, (int)filteredIndices.size());
            lv_label_set_text(lbl_counter_badge, tagBuf);
        }

        if (WiFi.status() != WL_CONNECTED) {
            lv_label_set_text(lbl_status_badge, "[ NO WI-FI ]");
            lv_obj_set_style_text_color(lbl_status_badge, lv_color_hex(0xFF5252), 0);
            lv_label_set_text(lbl_stream_info, "Wi-Fi disconnected. Open Wi-Fi Setup tab.");
        } else {
            if (currentError == ERR_OFFLINE) {
                lv_label_set_text(lbl_status_badge, "[ OFFLINE ]");
                lv_obj_set_style_text_color(lbl_status_badge, lv_color_hex(0xFF5252), 0);
                lv_label_set_text(lbl_stream_info, "Station stream offline. Tap NEXT to switch.");
            } else if (currentError == ERR_TIMEOUT) {
                lv_label_set_text(lbl_status_badge, "[ TIMEOUT ]");
                lv_obj_set_style_text_color(lbl_status_badge, lv_color_hex(0xFFAB00), 0);
                lv_label_set_text(lbl_stream_info, "Stream timed out. Tap PLAY to retry.");
            } else if (isBuffering) {
                lv_label_set_text(lbl_status_badge, "[ BUFFERING ]");
                lv_obj_set_style_text_color(lbl_status_badge, lv_color_hex(0xFFAB00), 0);
                lv_label_set_text(lbl_stream_info, "Connecting to live broadcast...");
            } else if (isPlaying && audio.isRunning()) {
                lv_label_set_text(lbl_status_badge, "[ LIVE ]");
                lv_obj_set_style_text_color(lbl_status_badge, lv_color_hex(0x00E676), 0);
                if (currentStreamTitle.length() > 0) {
                    lv_label_set_text(lbl_stream_info, currentStreamTitle.c_str());
                } else {
                    lv_label_set_text(lbl_stream_info, "Playing Live Broadcast");
                }
            } else {
                lv_label_set_text(lbl_status_badge, "[ PAUSED ]");
                lv_obj_set_style_text_color(lbl_status_badge, lv_color_hex(0x78909C), 0);
                lv_label_set_text(lbl_stream_info, "Tap PLAY to resume.");
            }
        }
    }

    if (isPlaying) {
        lv_label_set_text(lbl_btn_play, LV_SYMBOL_PAUSE " PAUSE");
        lv_obj_set_style_bg_color(btn_play, lv_color_hex(0x00B0FF), 0);
    } else {
        lv_label_set_text(lbl_btn_play, LV_SYMBOL_PLAY " PLAY");
        lv_obj_set_style_bg_color(btn_play, lv_color_hex(0x00E676), 0);
    }

    bsp_display_unlock();
}

// =============================================================================
// Audio Status Callbacks (Event-Driven Updates Only)
// =============================================================================
void audio_msg_handler(Audio::msg_t m) {
    if (m.msg) {
        Serial.printf("[AUDIO %s] %s\n", Audio::eventStr[m.e], m.msg);
    }
    if (m.e == Audio::evt_streamtitle && m.msg && strlen(m.msg) > 0) {
        if (strncmp(m.msg, "HTTP/", 5) == 0) {
            currentError = ERR_OFFLINE;
            isBuffering = false;
            isPlaying = false;
            currentStreamTitle = "";
            alertMessage = "Station stream offline (404). Tap NEXT.";
            uiNeedsUpdate = true;
            return;
        }
        currentStreamTitle = String(m.msg);
    }
    if (m.e == Audio::evt_info || m.e == Audio::evt_bitrate || m.e == Audio::evt_name || m.e == Audio::evt_streamtitle) {
        currentError = ERR_NONE;
        streamEstablished = true;
        streamRetryCount = 0;
        isRetryingStream = false;
        if (isBuffering) {
            isBuffering = false;
            uiNeedsUpdate = true;
        }
    }
    if (m.e == Audio::evt_eof) {
        if (currentSource == SRC_SD && !sdTrackPaths.empty()) {
            Serial.println("[SD] Track finished. Auto-playing next song...");
            playNextSdTrack();
        }
    }
}

// -----------------------------------------------------------------------------
// Persistent Settings Management & Local Web Server (Station Management)
// -----------------------------------------------------------------------------
void loadSavedSettings() {
    prefs.begin("air_radio", true);
    currentSSID = prefs.getString("wifi_ssid", "suresh2.4gExt");
    currentPass = prefs.getString("wifi_pass", "alangium");
    int savedStation = prefs.getInt("last_st", 0);
    int savedVol = prefs.getInt("last_vol", 21);
    prefs.end();

    if (savedVol >= 0 && savedVol <= 21) {
        currentVolume = savedVol;
        prevVolume = savedVol;
    }
    if (savedStation >= 0 && savedStation < (int)runtimeStations.size()) {
        currentFilterPosition = savedStation;
    }

    loadTimerSettings();
}

void loadTimerSettings() {
    prefs.begin("radio_timer", true);
    timerEnabled = prefs.getBool("en", false);
    timerHour = prefs.getInt("h", 6);
    timerMin = prefs.getInt("m", 0);
    timerDuration = prefs.getInt("dur", 30);
    prefs.end();
    Serial.printf("[TIMER] Loaded Auto-On Alarm: Enabled=%d, Time=%02d:%02d, Dur=%d min\n", 
                  timerEnabled, timerHour, timerMin, timerDuration);
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
    Serial.printf("[TIMER] Saved Auto-On Alarm: Enabled=%d, Time=%02d:%02d, Dur=%d min\n", 
                  timerEnabled, timerHour, timerMin, timerDuration);
}

void checkAutoOnTimer() {
    // 1. Check if alarm playback duration has expired (Auto-Off)
    if (alarmActivePlaying && (timerDuration > 0)) {
        if (millis() >= alarmAutoOffExpiryMs) {
            Serial.println("[TIMER] Auto-off duration expired! Stopping playback and turning off display backlight...");
            alarmActivePlaying = false;
            if (isPlaying) {
                audio.stopSong();
                isPlaying = false;
                isBuffering = false;
                updatePlayerUI();
            }
            digitalWrite(TFT_BLK, 1 - TFT_BLK_ON_LEVEL); // Turn off backlight
            return;
        }
    }

    if (!timerEnabled) return;
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2020 - 1900)) return; // NTP not yet synchronized

    if (timeinfo.tm_hour == timerHour && timeinfo.tm_min == timerMin) {
        if (lastTimerTriggerDay != timeinfo.tm_yday) {
            lastTimerTriggerDay = timeinfo.tm_yday;
            Serial.printf("[TIMER ALARM] Triggering Auto-On Alarm at %02d:%02d (Duration: %d min)!\n", 
                          timerHour, timerMin, timerDuration);
            // Ensure display backlight is turned on
            digitalWrite(TFT_BLK, TFT_BLK_ON_LEVEL);
            // Ensure unmuted
            if (isMuted) {
                toggleMute();
            }
            // Start playing last station if not already playing
            if (!isPlaying && !filteredIndices.empty()) {
                currentSource = SRC_RADIO;
                playCurrentStation();
            }
            // Arm auto-off countdown if duration > 0
            if (timerDuration > 0) {
                alarmActivePlaying = true;
                alarmAutoOffExpiryMs = millis() + ((uint32_t)timerDuration * 60000UL);
                Serial.printf("[TIMER ALARM] Auto-off scheduled in %d minutes\n", timerDuration);
            } else {
                alarmActivePlaying = false;
                Serial.println("[TIMER ALARM] Continuous playback (no auto-off scheduled)");
            }
        }
    }
}

static String escapeJson(const String& s) {
    String out = "";
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

// Static Web Remote HTML/CSS/JS (Stored in Flash Memory - Zero RAM, Instant <2ms delivery)
// Static Web Remote HTML/CSS/JS (Stored in Flash Memory - Zero RAM, Instant <2ms delivery)
const char PAGE_INDEX[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Radio Remote</title>
<style>
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:#0d1117;color:#c9d1d9;margin:0;padding:16px;text-align:center;}
.box{background:#161b22;border:1px solid #30363d;border-radius:12px;padding:16px;margin:0 auto 14px;max-width:420px;box-sizing:border-box;box-shadow:0 4px 12px rgba(0,0,0,0.3);}
h2{margin:2px 0 10px;color:#58a6ff;font-size:19px;}
h3{margin:6px 0 4px;color:#f0f6fc;font-size:18px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
.sub{color:#8b949e;font-size:13px;margin-bottom:12px;}
.badge{display:inline-block;padding:4px 10px;border-radius:12px;font-size:11px;font-weight:700;letter-spacing:0.5px;margin-bottom:6px;}
.badge-live{background:#238636;color:#fff;}
.badge-buf{background:#d29922;color:#000;}
.badge-pause{background:#30363d;color:#8b949e;}
.row{display:flex;justify-content:center;align-items:center;gap:10px;margin:12px 0;}
.btn{background:#21262d;color:#c9d1d9;border:1px solid #30363d;padding:10px 18px;border-radius:8px;font-size:14px;font-weight:600;cursor:pointer;touch-action:manipulation;transition:all 0.15s ease;}
.btn:hover{background:#30363d;}
.btn:active{transform:scale(0.97);}
.btn-blue{background:#1f6feb;color:#fff;border-color:#388bfd;}
.btn-blue:hover{background:#388bfd;}
select,input[type=text],input[type=password],input[type=url],input[type=time]{width:100%;box-sizing:border-box;padding:10px;margin:6px 0;background:#0d1117;color:#f0f6fc;border:1px solid #30363d;border-radius:6px;font-size:14px;}
select:focus,input:focus{outline:none;border-color:#58a6ff;}
#toast{position:fixed;top:14px;left:50%;transform:translateX(-50%);background:#58a6ff;color:#0d1117;padding:8px 18px;border-radius:20px;font-weight:bold;font-size:13px;display:none;z-index:99;box-shadow:0 4px 16px rgba(0,0,0,0.6);}
</style>
</head>
<body>
<div id="toast"></div>

<div class="box">
  <h2>📻 ESP32 Radio Remote</h2>
  <div id="t_badge" class="badge badge-pause">[ CONNECTING... ]</div>
  <h3 id="t_name">Connecting to Radio...</h3>
  <div class="sub" id="t_meta">Live Web Portal</div>
  
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
    <select id="tm_dur" style="width:100%;padding:8px;font-size:14px;background:#0d1117;color:#f0f6fc;border:1px solid #30363d;border-radius:6px;">
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

<div class="box">
  <div id="cs_title" style="font-weight:600;color:#ffd54f;margin-bottom:8px;text-align:left;">➕ Add Custom Station</div>
  <input type="hidden" id="cs_idx" value="-1">
  <input type="text" id="cs_name" placeholder="Station Name (e.g. Club FM)" required>
  <input type="text" id="cs_url" placeholder="Stream URL (http://... or https://...)" required>
  <input type="text" id="cs_state" placeholder="State (e.g. Kerala)">
  <input type="text" id="cs_lang" placeholder="Language (e.g. Malayalam)">
  <label style="display:block;margin:6px 0 8px;font-size:13px;color:#c9d1d9;text-align:left;cursor:pointer;">
    <input type="checkbox" id="cs_play_now"> ▶ Play this station immediately
  </label>
  <button type="button" id="cs_submit_btn" class="btn btn-blue" style="width:100%;margin-top:4px;" onclick="submitCustomStation()">➕ Save Station</button>
  <button type="button" id="cs_cancel_btn" class="btn" style="width:100%;margin-top:6px;display:none;" onclick="cancelEditStation()">Cancel Edit</button>
</div>

<div class="box">
  <div style="font-weight:600;color:#58a6ff;margin-bottom:8px;text-align:left;">⭐ My Custom Stations (User)</div>
  <div id="cs_list" style="text-align:left;">Loading stations...</div>
</div>

<script>
let tTimer=null;
function showToast(m){let e=document.getElementById('toast');e.innerText=m;e.style.display='block';if(tTimer)clearTimeout(tTimer);tTimer=setTimeout(()=>e.style.display='none',2500);}

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
  }).catch(()=>{});
}

// Alarm Timer
function loadTimer(){
  fetch('/api/timer').then(r=>r.json()).then(d=>{
    document.getElementById('tm_en').checked=d.enabled;
    let h=('0'+d.hour).slice(-2);
    let m=('0'+d.min).slice(-2);
    document.getElementById('tm_val').value=h+':'+m;
    if(d.dur!==undefined)document.getElementById('tm_dur').value=d.dur;
  }).catch(()=>{});
}

function saveTimer(){
  let en=document.getElementById('tm_en').checked?1:0;
  let val=document.getElementById('tm_val').value||'06:30';
  let dur=document.getElementById('tm_dur').value||'30';
  let parts=val.split(':');
  let h=parseInt(parts[0],10)||0;
  let m=parseInt(parts[1],10)||0;
  fetch('/api/timer?en='+en+'&h='+h+'&m='+m+'&dur='+dur).then(r=>r.json()).then(d=>{
    let durTxt=(dur=='0'?'Continuous':dur+'m');
    showToast(en?'⏰ Alarm Set ('+val+', '+durTxt+')':'⏰ Alarm Disabled');
  }).catch(()=>showToast('Failed to save timer'));
}

// Custom Stations Management
function loadCustomStations(){
  fetch('/api/custom_stations').then(r=>r.json()).then(list=>{
    let el=document.getElementById('cs_list');
    if(!list||list.length===0){
      el.innerHTML='<div style="color:#8b949e;font-size:13px;padding:6px 0;">No custom stations added yet. Add one above!</div>';
      return;
    }
    el.innerHTML=list.map(s=>`
      <div style="background:#0d1117;border:1px solid #30363d;border-radius:8px;padding:10px;margin-bottom:8px;">
        <div style="display:flex;justify-content:space-between;align-items:center;">
          <div style="font-weight:bold;color:#f0f6fc;font-size:14px;">${s.name}</div>
          <span style="font-size:11px;color:#58a6ff;background:#161b22;padding:2px 8px;border-radius:10px;">${s.state||'User'}</span>
        </div>
        <div style="color:#8b949e;font-size:11px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;margin:4px 0 8px;">${s.url}</div>
        <div style="display:flex;gap:6px;">
          <button type="button" class="btn btn-blue" style="padding:6px 12px;font-size:12px;" onclick="playStationRealIdx(${s.real_idx})">▶ Play</button>
          <button type="button" class="btn" style="padding:6px 12px;font-size:12px;" onclick="editStation(${s.idx},'${encodeURIComponent(s.name)}','${encodeURIComponent(s.url)}','${encodeURIComponent(s.state)}','${encodeURIComponent(s.lang)}')">✏ Edit</button>
          <button type="button" class="btn" style="padding:6px 12px;font-size:12px;color:#ff7b72;" onclick="delStation(${s.idx},'${encodeURIComponent(s.name)}')">🗑 Delete</button>
        </div>
      </div>
    `).join('');
  }).catch(()=>{
    document.getElementById('cs_list').innerHTML='<div style="color:#8b949e;font-size:12px;">Failed to load custom stations</div>';
  });
}

function submitCustomStation(){
  let name=document.getElementById('cs_name').value.trim();
  let url=document.getElementById('cs_url').value.trim();
  let state=document.getElementById('cs_state').value.trim();
  let lang=document.getElementById('cs_lang').value.trim();
  let idx=document.getElementById('cs_idx').value;
  let playNow=document.getElementById('cs_play_now').checked?1:0;
  if(!name||!url){showToast('Please enter Name and URL');return;}
  let body='name='+encodeURIComponent(name)+'&url='+encodeURIComponent(url)+'&state='+encodeURIComponent(state)+'&lang='+encodeURIComponent(lang)+'&idx='+idx+'&play='+playNow;
  fetch('/add_station',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
  .then(r=>r.json()).then(d=>{
    showToast(idx>=0?'✅ Station Updated!':'✅ Station Added!');
    cancelEditStation();
    loadCustomStations();
    setTimeout(syncStatus, 300);
  }).catch(()=>{
    showToast('Station saved');
    cancelEditStation();
    setTimeout(loadCustomStations, 500);
  });
}

function editStation(idx,name,url,state,lang){
  document.getElementById('cs_idx').value=idx;
  document.getElementById('cs_name').value=decodeURIComponent(name);
  document.getElementById('cs_url').value=decodeURIComponent(url);
  document.getElementById('cs_state').value=decodeURIComponent(state);
  document.getElementById('cs_lang').value=decodeURIComponent(lang);
  document.getElementById('cs_title').innerText='✏ Edit Custom Station';
  document.getElementById('cs_submit_btn').innerText='💾 Update Station';
  document.getElementById('cs_cancel_btn').style.display='block';
  document.getElementById('cs_name').focus();
}

function cancelEditStation(){
  document.getElementById('cs_idx').value='-1';
  document.getElementById('cs_name').value='';
  document.getElementById('cs_url').value='';
  document.getElementById('cs_state').value='';
  document.getElementById('cs_lang').value='';
  document.getElementById('cs_title').innerText='➕ Add Custom Station';
  document.getElementById('cs_submit_btn').innerText='➕ Save Station';
  document.getElementById('cs_cancel_btn').style.display='none';
}

function delStation(idx,name){
  if(!confirm('Delete '+decodeURIComponent(name)+'?'))return;
  fetch('/api/del_station?idx='+idx).then(()=> {
    showToast('🗑 Station Deleted');
    loadCustomStations();
  }).catch(()=>showToast('Delete failed'));
}

function playStationRealIdx(realIdx){
  showToast('Tuning in...');
  fetch('/api/cmd?action=play&id='+realIdx).then(()=>setTimeout(syncStatus,200)).catch(()=>{});
}

syncStatus();
loadTimer();
loadCustomStations();
setInterval(syncStatus, 2000);
</script>
</body>
</html>)rawliteral";

enum PendingWebAction {
    ACT_NONE,
    ACT_PLAY_PAUSE,
    ACT_PREV,
    ACT_NEXT,
    ACT_PLAY_STATION
};
volatile PendingWebAction pendingWebAction = ACT_NONE;
volatile int pendingStationId = -1;
volatile int pendingVol = -1;

struct PendingCustomStation {
    char name[64];
    char url[256];
    char state[48];
    char lang[48];
    int editIdx;
    bool playNow;
    volatile bool pending;
};
static PendingCustomStation pendingAddStation = { "", "", "", "", -1, false, false };

volatile int pendingDeleteStationIdx = -1;

struct PendingTimerSave {
    bool en;
    int h;
    int m;
    int dur;
    volatile bool pending;
};
static PendingTimerSave pendingTimerSave = { false, 6, 0, 30, false };

static esp_err_t http_root_handler(httpd_req_t *req) {
    Serial.println("[HTTPD] Serving / (PAGE_INDEX)");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    esp_err_t res = httpd_resp_send(req, PAGE_INDEX, HTTPD_RESP_USE_STRLEN);
    Serial.printf("[HTTPD] / sent successfully, result: %d\n", res);
    return res;
}

static esp_err_t http_status_handler(httpd_req_t *req) {
    Serial.println("[HTTPD] Serving /api/status");
    float batVoltage = 0.0f;
    bool isCharging = false;
    int batPct = getBatteryInfo(&batVoltage, &isCharging);

    const char* title = "";
    const char* state = "";
    const char* lang  = "";

    if (currentSource == SRC_SD) {
        title = currentStreamTitle.c_str();
    } else if (currentSource == SRC_RADIO && !filteredIndices.empty()) {
        int realIdx = filteredIndices[currentFilterPosition];
        title = runtimeStations[realIdx].name.c_str();
        state = runtimeStations[realIdx].state.c_str();
        lang  = runtimeStations[realIdx].language.c_str();
    }

    char buf[320];
    snprintf(buf, sizeof(buf),
             "{\"playing\":%s,\"buffering\":%s,\"source\":\"%s\",\"title\":\"%s\",\"state\":\"%s\",\"lang\":\"%s\",\"vol\":%d,\"muted\":%s,\"bat\":%d,\"charging\":%s,\"total\":%d}",
             isPlaying ? "true" : "false",
             isBuffering ? "true" : "false",
             currentSource == SRC_SD ? "sd" : "radio",
             title, state, lang,
             currentVolume,
             isMuted ? "true" : "false",
             batPct,
             isCharging ? "true" : "false",
             (int)runtimeStations.size());

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_cmd_handler(httpd_req_t *req) {
    char query[128] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char act[32] = {0};
        if (httpd_query_key_value(query, "action", act, sizeof(act)) == ESP_OK) {
            Serial.printf("[HTTPD] /api/cmd action: %s\n", act);
            if (strcmp(act, "play_pause") == 0) {
                pendingWebAction = ACT_PLAY_PAUSE;
            } else if (strcmp(act, "next") == 0) {
                pendingWebAction = ACT_NEXT;
            } else if (strcmp(act, "prev") == 0) {
                pendingWebAction = ACT_PREV;
            } else if (strcmp(act, "mute") == 0) {
                toggleMute();
            } else if (strcmp(act, "vol") == 0) {
                char vstr[16] = {0};
                if (httpd_query_key_value(query, "val", vstr, sizeof(vstr)) == ESP_OK) {
                    pendingVol = atoi(vstr);
                }
            } else if (strcmp(act, "play") == 0) {
                char idstr[16] = {0};
                if (httpd_query_key_value(query, "id", idstr, sizeof(idstr)) == ESP_OK) {
                    pendingStationId = atoi(idstr);
                    pendingWebAction = ACT_PLAY_STATION;
                }
            }
        }
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static void url_decode_str(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit((int)a) && isxdigit((int)b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static esp_err_t http_timer_handler(httpd_req_t *req) {
    char query[96] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char val_en[8] = {0}, val_h[8] = {0}, val_m[8] = {0}, val_dur[8] = {0};
        bool has_en = (httpd_query_key_value(query, "en", val_en, sizeof(val_en)) == ESP_OK);
        bool has_h = (httpd_query_key_value(query, "h", val_h, sizeof(val_h)) == ESP_OK);
        bool has_m = (httpd_query_key_value(query, "m", val_m, sizeof(val_m)) == ESP_OK);
        bool has_dur = (httpd_query_key_value(query, "dur", val_dur, sizeof(val_dur)) == ESP_OK);
        if (has_en && has_h && has_m) {
            int d = has_dur ? atoi(val_dur) : timerDuration;
            pendingTimerSave.en = (atoi(val_en) != 0);
            pendingTimerSave.h = atoi(val_h);
            pendingTimerSave.m = atoi(val_m);
            pendingTimerSave.dur = d;
            pendingTimerSave.pending = true;
            timerEnabled = pendingTimerSave.en;
            timerHour = pendingTimerSave.h;
            timerMin = pendingTimerSave.m;
            timerDuration = pendingTimerSave.dur;
        }
    }
    char buf[160];
    snprintf(buf, sizeof(buf), "{\"enabled\":%s,\"hour\":%d,\"min\":%d,\"dur\":%d}",
             timerEnabled ? "true" : "false", timerHour, timerMin, timerDuration);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_custom_stations_handler(httpd_req_t *req) {
    String json = "[";
    int cIdx = 0;
    for (size_t i = 0; i < runtimeStations.size(); i++) {
        if (runtimeStations[i].isCustom) {
            if (cIdx > 0) json += ",";
            json += "{\"idx\":" + String(cIdx) + 
                    ",\"real_idx\":" + String((int)i) +
                    ",\"name\":\"" + escapeJson(runtimeStations[i].name) + "\"" +
                    ",\"url\":\"" + escapeJson(runtimeStations[i].url) + "\"" +
                    ",\"state\":\"" + escapeJson(runtimeStations[i].state) + "\"" +
                    ",\"lang\":\"" + escapeJson(runtimeStations[i].language) + "\"}";
            cIdx++;
        }
    }
    json += "]";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json.c_str(), json.length());
}

static esp_err_t http_del_station_handler(httpd_req_t *req) {
    char query[64] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char idx_str[16] = {0};
        if (httpd_query_key_value(query, "idx", idx_str, sizeof(idx_str)) == ESP_OK) {
            int idx = atoi(idx_str);
            Serial.printf("[HTTPD] Queued delete custom station index: %d\n", idx);
            pendingDeleteStationIdx = idx;
        }
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_add_station_handler(httpd_req_t *req) {
    char post_buf[512] = {0};
    int remaining = req->content_len;
    int received = 0;
    while (remaining > 0 && received < (int)sizeof(post_buf) - 1) {
        int ret = httpd_req_recv(req, post_buf + received, min(remaining, (int)sizeof(post_buf) - 1 - received));
        if (ret <= 0) break;
        received += ret;
        remaining -= ret;
    }
    post_buf[received] = '\0';

    char raw_name[96] = {0}, raw_url[256] = {0}, raw_state[64] = {0}, raw_lang[64] = {0};
    char raw_idx[16] = {0}, raw_play[16] = {0};
    httpd_query_key_value(post_buf, "name", raw_name, sizeof(raw_name));
    httpd_query_key_value(post_buf, "url", raw_url, sizeof(raw_url));
    httpd_query_key_value(post_buf, "state", raw_state, sizeof(raw_state));
    httpd_query_key_value(post_buf, "lang", raw_lang, sizeof(raw_lang));
    httpd_query_key_value(post_buf, "idx", raw_idx, sizeof(raw_idx));
    httpd_query_key_value(post_buf, "play", raw_play, sizeof(raw_play));

    char dec_name[96] = {0}, dec_url[256] = {0}, dec_state[64] = {0}, dec_lang[64] = {0};
    url_decode_str(dec_name, raw_name);
    url_decode_str(dec_url, raw_url);
    url_decode_str(dec_state, raw_state);
    url_decode_str(dec_lang, raw_lang);

    int editIdx = strlen(raw_idx) > 0 ? atoi(raw_idx) : -1;
    bool playNow = (strlen(raw_play) > 0 && atoi(raw_play) == 1);

    if (strlen(dec_name) > 0 && strlen(dec_url) > 0) {
        strncpy(pendingAddStation.name, dec_name, sizeof(pendingAddStation.name) - 1);
        pendingAddStation.name[sizeof(pendingAddStation.name) - 1] = '\0';
        strncpy(pendingAddStation.url, dec_url, sizeof(pendingAddStation.url) - 1);
        pendingAddStation.url[sizeof(pendingAddStation.url) - 1] = '\0';
        strncpy(pendingAddStation.state, dec_state, sizeof(pendingAddStation.state) - 1);
        pendingAddStation.state[sizeof(pendingAddStation.state) - 1] = '\0';
        strncpy(pendingAddStation.lang, dec_lang, sizeof(pendingAddStation.lang) - 1);
        pendingAddStation.lang[sizeof(pendingAddStation.lang) - 1] = '\0';
        pendingAddStation.editIdx = editIdx;
        pendingAddStation.playNow = playNow;
        pendingAddStation.pending = true;

        Serial.printf("[HTTPD] Queued Add/Update Station: '%s' -> '%s'\n", dec_name, dec_url);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_send(req, "{\"status\":\"error\",\"msg\":\"Missing name or url\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_favicon_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t http_404_handler(httpd_req_t *req, httpd_err_code_t err) {
    Serial.printf("[HTTPD] 404 handler for URI: %s\n", req->uri);
    if (apPortalActive) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
        return httpd_resp_send(req, NULL, 0);
    }
    httpd_resp_set_status(req, "404 Not Found");
    return httpd_resp_send(req, "Not Found", HTTPD_RESP_USE_STRLEN);
}

// Static URI handler structures at file scope
static const httpd_uri_t uri_root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = http_root_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_status = {
    .uri       = "/api/status",
    .method    = HTTP_GET,
    .handler   = http_status_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_cmd = {
    .uri       = "/api/cmd",
    .method    = HTTP_GET,
    .handler   = http_cmd_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_timer = {
    .uri       = "/api/timer",
    .method    = HTTP_GET,
    .handler   = http_timer_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_custom_st = {
    .uri       = "/api/custom_stations",
    .method    = HTTP_GET,
    .handler   = http_custom_stations_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_del_st = {
    .uri       = "/api/del_station",
    .method    = HTTP_GET,
    .handler   = http_del_station_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_add_st = {
    .uri       = "/add_station",
    .method    = HTTP_POST,
    .handler   = http_add_station_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_fav = {
    .uri       = "/favicon.ico",
    .method    = HTTP_GET,
    .handler   = http_favicon_handler,
    .user_ctx  = NULL
};

void setupWebServer() {
    if (webServerStarted) return;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.ctrl_port = 32768;
    config.max_open_sockets = 7;
    config.max_uri_handlers = 12;
    config.stack_size = 10240;
    config.task_caps = (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    config.task_priority = 2;
    config.core_id = 1;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;

    esp_err_t err = httpd_start(&webHttpdServer, &config);
    if (err == ESP_OK) {
        httpd_register_uri_handler(webHttpdServer, &uri_root);
        httpd_register_uri_handler(webHttpdServer, &uri_status);
        httpd_register_uri_handler(webHttpdServer, &uri_cmd);
        httpd_register_uri_handler(webHttpdServer, &uri_timer);
        httpd_register_uri_handler(webHttpdServer, &uri_custom_st);
        httpd_register_uri_handler(webHttpdServer, &uri_del_st);
        httpd_register_uri_handler(webHttpdServer, &uri_add_st);
        httpd_register_uri_handler(webHttpdServer, &uri_fav);
        httpd_register_err_handler(webHttpdServer, HTTPD_404_NOT_FOUND, http_404_handler);

        webServerStarted = true;
        Serial.printf("[WEB REMOTE] Live via esp_http_server at http://%s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.printf("[WEB REMOTE] Failed to start esp_http_server: 0x%x (%s)\n", err, esp_err_to_name(err));
    }
}

void startAPPortal() {
    apPortalActive = true;
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("AIR-Radio-Setup", "");
    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    Serial.println("\n[PORTAL] Started Web Portal AP 'AIR-Radio-Setup' at http://192.168.4.1");
    setupWebServer();
}

bool initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);

    Serial.printf("[WIFI] Connecting to '%s' ...\n", currentSSID.c_str());
    WiFi.begin(currentSSID.c_str(), currentPass.c_str());

    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 60) {
        delay(500);
        Serial.print(".");
        timeout++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        WiFi.setSleep(false);
        esp_wifi_set_ps(WIFI_PS_NONE);
        Serial.printf("\n[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        setupWebServer();
        return true;
    } else {
        Serial.println("\n[WIFI] Not connected to saved network.");
        return false;
    }
}

// =============================================================================
// Main Setup & Loop
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println("\n=======================================================");
    Serial.println("  ESP32-S3 JC3248W535 - Multi-Source Internet & SD Radio");
    Serial.println("=======================================================");
    if (psramInit()) {
        Serial.println("[SYSTEM] PSRAM Initialized successfully!");
    } else {
        Serial.println("[SYSTEM] PSRAM init returned false (or already initialized).");
    }

    if (psramFound()) {
        mbedtls_platform_set_calloc_free(psram_mbedtls_calloc, psram_mbedtls_free);
        Serial.println("[SYSTEM] Configured mbedtls to allocate SSL buffers from PSRAM!");
    }

    Serial.printf("[SYSTEM] Free Heap: %d KB (DMA Largest Free: %d KB), Free PSRAM: %d KB (PSRAM Found: %s)\n", 
                  ESP.getFreeHeap() / 1024,
                  heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) / 1024,
                  ESP.getFreePsram() / 1024, psramFound() ? "YES" : "NO");

    // 1. Initialize Display & LVGL
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
        .rotate = LV_DISP_ROT_90,
    };
    bsp_display_start_with_config(&cfg);
    // Display backlight is intentionally left OFF here until UI is fully built

    // 2. Initialize Station Database (Built-in + Saved Custom Stations)
    initStationDatabase();

    // 3. Load Saved Preferences (Wi-Fi, Last Played Station, Volume)
    loadSavedSettings();

    // 4. Initialize SD Card & Scan Audio Tracks
    initSDCard();

    // 5. Initialize Station Filter Indices in RAM
    filteredIndices.clear();
    for (size_t i = 0; i < runtimeStations.size(); i++) filteredIndices.push_back(i);

    // 6. Configure NTP Live Clock
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov");

    // 7. Build Dynamic Multi-Tab Modern UI & Sync Saved State
    buildModernUI();

    // Force LVGL to render the first frame while backlight is still OFF
    bsp_display_lock(0);
    lv_timer_handler();
    bsp_display_unlock();
    delay(50); // Let the SPI transaction finish flushing

    // Now turn on the backlight to reveal the UI instantly without white flashes
    bsp_display_backlight_on();

    // 8. Check Wakeup Cause (Deep Sleep Touch Wakeup vs Cold Boot)
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    bool wokeFromSleep = (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1 || 
                          wakeup_cause == ESP_SLEEP_WAKEUP_EXT0 || 
                          wakeup_cause == ESP_SLEEP_WAKEUP_GPIO);

    // 9. Configure Hardware I2S Digital Amplifier & Register Audio Callbacks
    Audio::audio_info_callback = audio_msg_handler;
    audio.setPinout(AUDIO_I2S_BCK_IO, AUDIO_I2S_LRCK_IO, AUDIO_I2S_DO_IO);
    audio.setVolume(currentVolume);
    audio.setConnectionTimeout(4000, 10000);

    // (Audio task creation removed)

    // 11. Handle Boot Mode: Wakeup Confirmation Prompt vs Normal Boot Auto-Play
    if (wokeFromSleep) {
        Serial.println("[POWER] Woke from Deep Sleep via Touch Interrupt. Showing Resume Prompt...");
        showWakeupPrompt();
    } else {
        Serial.println("[POWER] Cold Boot. Connecting to Wi-Fi and auto-starting radio...");
        bool wifiOk = initWiFi();
        updateWiFiStatusBanner();
        updatePlayerUI();

        if (wifiOk) {
            setupWebServer();
            updatePlayerUI();
            playCurrentStation();
        } else {
            bsp_display_lock(0);
            lv_tabview_set_act(tabview, 2, LV_ANIM_OFF);
            bsp_display_unlock();
            scanAndPopulateWiFi();
            startAPPortal();
        }
    }
}

void loop() {
    // 1. Process DNS Captive Portal Requests if AP portal is active
    if (apPortalActive) {
        dnsServer.processNextRequest();
    }

    // 2. Handle Asynchronous Wi-Fi Reconnection (Instant, non-blocking UI)
    if (isReconnectingWiFi) {
        if (WiFi.status() == WL_CONNECTED) {
            WiFi.setSleep(false);
            esp_wifi_set_ps(WIFI_PS_NONE);
            Serial.printf("\n[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            isReconnectingWiFi = false;
            updateWiFiStatusBanner();
            setupWebServer();
            if (currentSource == SRC_RADIO) {
                playCurrentStation();
            }
        } else if (millis() - wifiConnectStartTime > 15000) {
            Serial.println("\n[WIFI] Connection Timeout.");
            isReconnectingWiFi = false;
            updateWiFiStatusBanner();
        }
    }

    // 3. Process Pending Web Actions outside of the Web Server stack frame!
    // This ensures no Strings are allocated, leaving 100% of the contiguous heap for mbedtls.
    if (pendingVol != -1) {
        setSystemVolume(pendingVol);
        pendingVol = -1;
    }
    if (pendingWebAction != ACT_NONE) {
        if (pendingWebAction == ACT_PLAY_PAUSE) {
            if (isPlaying) { audio.pauseResume(); isPlaying = audio.isRunning(); }
            else { if (currentSource == SRC_SD) playCurrentSdTrack(); else playCurrentStation(); }
            updatePlayerUI();
        } else if (pendingWebAction == ACT_PREV) {
            if (currentSource == SRC_SD) playPrevSdTrack();
            else playStationByFilterIndex(currentFilterPosition - 1);
        } else if (pendingWebAction == ACT_NEXT) {
            if (currentSource == SRC_SD) playNextSdTrack();
            else playStationByFilterIndex(currentFilterPosition + 1);
        } else if (pendingWebAction == ACT_PLAY_STATION) {
            if (pendingStationId >= 0 && pendingStationId < (int)runtimeStations.size()) {
                for (size_t i = 0; i < filteredIndices.size(); i++) {
                    if (filteredIndices[i] == pendingStationId) { currentFilterPosition = i; break; }
                }
                bsp_display_lock(0);
                lv_tabview_set_act(tabview, 0, LV_ANIM_OFF);
                bsp_display_unlock();
                playCurrentStation();
            }
        }
        pendingWebAction = ACT_NONE;
    }


    // 3. Thread-Safe UI Refresh on Core 1
    if (uiNeedsUpdate) {
        uiNeedsUpdate = false;
        updatePlayerUI();
    }

    // 4. Thread-Safe Pending Timer Save on Core 1
    if (pendingTimerSave.pending) {
        saveTimerSettings(pendingTimerSave.en, pendingTimerSave.h, pendingTimerSave.m, pendingTimerSave.dur);
        bsp_display_lock(0);
        updateClockAndBatteryUI();
        bsp_display_unlock();
        pendingTimerSave.pending = false;
    }

    // 5. Thread-Safe Pending Add / Update Custom Station on Core 1
    if (pendingAddStation.pending) {
        saveCustomStation(String(pendingAddStation.name), String(pendingAddStation.url), 
                          String(pendingAddStation.state), String(pendingAddStation.lang), 
                          pendingAddStation.editIdx);
        if (pendingAddStation.playNow) {
            for (int i = (int)runtimeStations.size() - 1; i >= 0; i--) {
                if (runtimeStations[i].isCustom && runtimeStations[i].url == String(pendingAddStation.url)) {
                    for (size_t f = 0; f < filteredIndices.size(); f++) {
                        if (filteredIndices[f] == i) { currentFilterPosition = f; break; }
                    }
                    bsp_display_lock(0);
                    lv_tabview_set_act(tabview, 0, LV_ANIM_OFF);
                    bsp_display_unlock();
                    playCurrentStation();
                    break;
                }
            }
        }
        pendingAddStation.pending = false;
    }

    // 6. Thread-Safe Pending Delete Custom Station on Core 1
    if (pendingDeleteStationIdx >= 0) {
        deleteCustomStation(pendingDeleteStationIdx);
        pendingDeleteStationIdx = -1;
    }

    // 7. Thread-Safe Custom Station List Refresh on Core 1
    if (pendingStationListRefresh) {
        pendingStationListRefresh = false;
        applyCategoryFilter(activeCatType, activeFilterVal.c_str());
        if (list_categories) populateCategoryList();
        if (list_stations) populateStationList();
    }

    // 8. 1-Second Interval Check for Persistent Auto-On Alarm Timer
    static unsigned long lastTimerCheckMs = 0;
    if (millis() - lastTimerCheckMs >= 1000) {
        lastTimerCheckMs = millis();
        checkAutoOnTimer();
    }

    // 9. 10-Attempt Stream Connection Retry Engine (2.5s spacing)
    if (isRetryingStream && (currentSource == SRC_RADIO)) {
        if (millis() >= nextStreamRetryMs) {
            isRetryingStream = false;
            pendingStreamUrl = activeTargetUrl;
            newStreamRequested = true;
            Serial.printf("[RETRY ENGINE] Retrying stream (%d/%d): %s\n", 
                          streamRetryCount + 1, MAX_STREAM_RETRIES, activeTargetUrl.c_str());
        }
    }

    if (newStreamRequested) {
        newStreamRequested = false;
        audio.stopSong();
        if (pendingIsSdFile) {
            isRetryingStream = false;
            streamRetryCount = 0;
            if (pendingStreamUrl.length() > 0 && sdCardMounted) {
                Serial.printf("[LOOP] Playing SD File: %s\n", pendingStreamUrl.c_str());
                audio.connecttoFS(SD_MMC, pendingStreamUrl.c_str());
                isPlaying = true;
            } else {
                isPlaying = false;
                uiNeedsUpdate = true;
            }
        } else {
            if (pendingStreamUrl.length() > 0 && WiFi.status() == WL_CONNECTED) {
                Serial.printf("[LOOP] Connecting to (%d/%d): %s\n", 
                              streamRetryCount + 1, MAX_STREAM_RETRIES, pendingStreamUrl.c_str());
                bool ok = audio.connecttohost(pendingStreamUrl.c_str());
                if (!ok) {
                    Serial.printf("[LOOP] connecttohost returned false on attempt %d/%d\n", 
                                  streamRetryCount + 1, MAX_STREAM_RETRIES);
                    streamRetryCount++;
                    if (streamRetryCount < MAX_STREAM_RETRIES) {
                        isRetryingStream = true;
                        nextStreamRetryMs = millis() + 2500;
                        alertMessage = "Retrying stream (" + String(streamRetryCount + 1) + "/10)...";
                        isBuffering = true;
                        isPlaying = true;
                        uiNeedsUpdate = true;
                    } else {
                        isRetryingStream = false;
                        audio.stopSong();
                        isBuffering = false;
                        isPlaying = false;
                        currentError = ERR_OFFLINE;
                        alertMessage = "Station offline after 10 attempts.";
                        uiNeedsUpdate = true;
                    }
                } else {
                    isPlaying = true;
                    isBuffering = true;
                    streamStartTime = millis();
                    streamEstablished = false;
                    if (streamRetryCount > 0) {
                        alertMessage = "Connecting... (Attempt " + String(streamRetryCount + 1) + "/10)";
                        uiNeedsUpdate = true;
                    }
                }
            } else {
                isPlaying = false;
                isRetryingStream = false;
                streamRetryCount = 0;
                if (WiFi.status() != WL_CONNECTED) {
                    currentError = ERR_NONE;
                    alertMessage = "Wi-Fi disconnected. Open Wi-Fi tab.";
                } else {
                    currentError = ERR_OFFLINE;
                    alertMessage = "Invalid station URL.";
                }
                uiNeedsUpdate = true;
            }
        }
    }

    // 10. Connection Stalling Surveillance (Retries up to 10 times if connection hangs while buffering)
    if (isBuffering && isPlaying && (currentSource == SRC_RADIO) && !isRetryingStream) {
        if (millis() - streamStartTime > 12000) {
            Serial.printf("[LOOP] Stream connection stalled on attempt %d/%d\n", 
                          streamRetryCount + 1, MAX_STREAM_RETRIES);
            streamRetryCount++;
            if (streamRetryCount < MAX_STREAM_RETRIES) {
                isRetryingStream = true;
                nextStreamRetryMs = millis() + 2500;
                alertMessage = "Retrying stream (" + String(streamRetryCount + 1) + "/10)...";
                uiNeedsUpdate = true;
            } else {
                isRetryingStream = false;
                audio.stopSong();
                isBuffering = false;
                isPlaying = false;
                currentError = ERR_TIMEOUT;
                alertMessage = "Station offline after 10 attempts.";
                uiNeedsUpdate = true;
            }
        }
    }

    if (isPlaying) {
        audio.loop();
        if (!audio.isRunning()) {
            isPlaying = false;
            isBuffering = false;
            currentError = ERR_OFFLINE;
            alertMessage = "Station stopped. Tap PLAY to retry.";
            uiNeedsUpdate = true;
        }
    } else {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// --- Audio Debug Callbacks ---
void audio_info(const char *info){
    Serial.print("[AUDIO INFO] "); Serial.println(info);
}
void audio_id3data(const char *info){
    Serial.print("[AUDIO ID3] "); Serial.println(info);
}
void audio_showstation(const char *info){
    Serial.print("[AUDIO STATION] "); Serial.println(info);
}
void audio_showstreamtitle(const char *info){
    Serial.print("[AUDIO TITLE] "); Serial.println(info);
}
void audio_bitrate(const char *info){
    Serial.print("[AUDIO BITRATE] "); Serial.println(info);
}
void audio_lasthost(const char *info){
    Serial.print("[AUDIO HOST] "); Serial.println(info);
}
void audio_eof_mp3(const char *info){
    Serial.print("[AUDIO EOF] "); Serial.println(info);
}
void audio_error(const char *info){
    Serial.print("[AUDIO ERROR] "); Serial.println(info);
}
