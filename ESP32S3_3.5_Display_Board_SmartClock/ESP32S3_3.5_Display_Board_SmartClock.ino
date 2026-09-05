/**
 * =========================================================================================
 * Project: ESP32-S3 JC3248W535 - Luxury Giant Vector Digital Clock & Desk Companion
 * Target Board: JC3248W535 (3.5" 320x480 IPS Capacitive Touch, 16MB Flash, 8MB PSRAM)
 * Features:
 *   - Tab 1: 🕒 Dominant 135px Tall Glowing Vector Digital Clock (Massive, Bold, Crystal Clear)
 *   - Top Ribbon: Clean Date, Weather Forecast Capsule (Temp, Condition, Humidity) & Wi-Fi Link
 *   - Bottom Ribbon: Smooth Continuous News Marquee Ticker
 *   - Tab 2: ⏱️ Pomodoro / Productivity Focus Timer
 *   - Tab 3: 📰 Breaking News Feed (Full Reader)
 *   - Tab 4: ⚙️ Settings & Clean Web Captive Portal
 * =========================================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <lvgl.h>
#include "esp_bsp.h"
#include "lv_port.h"
#include "display.h"
#include "pincfg.h"

// -----------------------------------------------------------------------------
// Persistent Storage (NVS) & SoftAP Web Portal
// -----------------------------------------------------------------------------
Preferences prefs;
String currentSSID = "";
String currentPass = "";

const char* DEFAULT_SSID = "suresh2.4gExt";
const char* DEFAULT_PASS = "alangium";

const char* AP_SSID = "ESP32-SmartClock-Setup";
WebServer server(80);
DNSServer dnsServer;
bool apPortalRunning = false;

// -----------------------------------------------------------------------------
// Timezone Configuration (Indian Standard Time: UTC + 5:30 = 19800 seconds)
// -----------------------------------------------------------------------------
const long  GMT_OFFSET_SEC = 19800;
const int   DAYLIGHT_OFFSET_SEC = 0;
const char* NTP_SERVER_1 = "pool.ntp.org";
const char* NTP_SERVER_2 = "time.nist.gov";

// -----------------------------------------------------------------------------
// Weather Coordinates & Online News Endpoints
// -----------------------------------------------------------------------------
const char* WEATHER_API_URL = "https://api.open-meteo.com/v1/forecast?latitude=10.5276&longitude=76.2144&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m&daily=temperature_2m_max,temperature_2m_min&timezone=auto";
const char* NEWS_API_URL = "https://saurav.tech/NewsAPI/top-headlines/category/general/in.json";

// -----------------------------------------------------------------------------
// Weather State
// -----------------------------------------------------------------------------
struct WeatherData {
    float temp = 0.0;
    float feelsLike = 0.0;
    int humidity = 0;
    float windSpeed = 0.0;
    float tempMax = 0.0;
    float tempMin = 0.0;
    int weatherCode = 0;
    String condition = "Loading...";
    bool valid = false;
};

WeatherData currentWeather;

// -----------------------------------------------------------------------------
// News Items Storage & Continuous Marquee String
// -----------------------------------------------------------------------------
const int MAX_NEWS_ITEMS = 6;
String newsHeadlines[MAX_NEWS_ITEMS] = {
    "Fetching latest breaking news headlines over Wi-Fi...",
    "All India Radio & Doordarshan regional digital streaming networks active",
    "Kerala high-tech digital science and innovation centers expanding",
    "ISRO advances Gaganyaan mission with successful booster trials",
    "India achieves record renewable solar and clean energy capacity",
    "High-speed 5G network rollout covers nationwide districts"
};

String marqueeTickerText = "";
int currentBrightness = 90; // Percentage 10 - 100

// -----------------------------------------------------------------------------
// Pomodoro / Focus Timer Variables
// -----------------------------------------------------------------------------
int pomodoroTotalSec = 25 * 60;
int pomodoroRemainSec = 25 * 60;
bool pomodoroRunning = false;
unsigned long lastPomoTick = 0;

// Daily Motivational Quotes
const int NUM_QUOTES = 5;
const char* DAILY_QUOTES[NUM_QUOTES] = {
    "\"The secret of getting ahead is getting started.\" - Mark Twain",
    "\"Focus on being productive instead of busy.\" - Tim Ferriss",
    "\"Small daily improvements over time lead to stunning results.\" - Robin Sharma",
    "\"Your time is limited, don't waste it living someone else's life.\" - Steve Jobs",
    "\"Do what you can, with what you have, where you are.\" - Theodore Roosevelt"
};

// -----------------------------------------------------------------------------
// Vector 7-Segment Digit Model & Definitions
// -----------------------------------------------------------------------------
// 7-segment bitmasks for numbers 0 - 9 (0bGFEDCBA)
const uint8_t DIGIT_MASKS[10] = {
    0x3F, // 0: A, B, C, D, E, F
    0x06, // 1: B, C
    0x5B, // 2: A, B, D, E, G
    0x4F, // 3: A, B, C, D, G
    0x66, // 4: B, C, F, G
    0x6D, // 5: A, C, D, F, G
    0x7D, // 6: A, C, D, E, F, G
    0x07, // 7: A, B, C
    0x7F, // 8: A, B, C, D, E, F, G
    0x6F  // 9: A, B, C, D, F, G
};

struct Digit7Seg {
    lv_obj_t* seg[7]; // A, B, C, D, E, F, G
};

static Digit7Seg digitH1; // Tens of Hour
static Digit7Seg digitH2; // Units of Hour
static Digit7Seg digitM1; // Tens of Min
static Digit7Seg digitM2; // Units of Min
static Digit7Seg digitS1; // Tens of Sec
static Digit7Seg digitS2; // Units of Sec

static lv_obj_t* colonDot1 = NULL;
static lv_obj_t* colonDot2 = NULL;
static lv_obj_t* lbl_ampm_badge = NULL;

const lv_color_t COLOR_SEG_ON  = lv_color_hex(0x00E5FF); // Glowing Vibrant Cyan
const lv_color_t COLOR_SEG_OFF = lv_color_hex(0x04080D); // Pure Deep Pitch Black Inactive
const lv_color_t COLOR_SEC_ON  = lv_color_hex(0xFFD600); // Warm Gold for Seconds
const lv_color_t COLOR_SEC_OFF = lv_color_hex(0x080702); // Pure Deep Inactive Gold

// -----------------------------------------------------------------------------
// LVGL UI Widget Handles
// -----------------------------------------------------------------------------
static lv_obj_t* tv_main = NULL;

// Tab 1: Clean Giant Clock Widgets
static lv_obj_t* lbl_top_date = NULL;
static lv_obj_t* lbl_top_weather = NULL;
static lv_obj_t* lbl_top_wifi = NULL;
static lv_obj_t* pnl_clock_stage = NULL;
static lv_obj_t* lbl_news_ticker = NULL;

// Tab 2: Pomodoro Focus Widgets
static lv_obj_t* lbl_pomo_time = NULL;
static lv_obj_t* bar_pomo_progress = NULL;
static lv_obj_t* btn_pomo_toggle = NULL;
static lv_obj_t* lbl_btn_pomo = NULL;
static lv_obj_t* lbl_pomo_status = NULL;
static lv_obj_t* lbl_quote_text = NULL;

// Tab 3: News Feed Widgets
static lv_obj_t* lbl_full_news = NULL;

// Tab 4: Settings & Clean Hardware Dashboard
static lv_obj_t* lbl_wifi_ssid_display = NULL;
static lv_obj_t* lbl_wifi_ip_display = NULL;
static lv_obj_t* lbl_wifi_rssi_display = NULL;
static lv_obj_t* btn_start_ap_portal = NULL;
static lv_obj_t* lbl_btn_start_ap = NULL;
static lv_obj_t* modal_ap_portal = NULL;
static lv_obj_t* lbl_ap_modal_info = NULL;

static lv_obj_t* lbl_sys_heap = NULL;
static lv_obj_t* lbl_sys_psram = NULL;
static lv_obj_t* lbl_sys_uptime = NULL;
static lv_obj_t* slider_bright = NULL;
static lv_obj_t* lbl_bright_val = NULL;

// Timing triggers
unsigned long lastClockUpdate = 0;
unsigned long lastWeatherFetch = 0;
unsigned long lastNewsFetch = 0;
unsigned long lastSystemStatUpdate = 0;
bool colonBlinkState = true;

// Forward Declarations
void loadSavedCredentials();
void initWiFi();
void syncNTPTime();
void fetchWeather();
void fetchNews();
void updateNewsDisplay();
void buildDashboardUI();
void updateClockDisplay();
void updateWeatherDisplay();
void updateSystemStats();
void assembleMarqueeText();
void startWebPortal();
void stopWebPortal();
void handleWebRoot();
void handleWebSave();
void createVectorDigit(lv_obj_t* parent, Digit7Seg* d, int x, int y, int w, int h, int t, bool isSec);
void setVectorDigitValue(Digit7Seg* d, int val, bool isSec);

// =============================================================================
// Helper: Map Weather Code to Human-Readable Condition
// =============================================================================
String getWeatherCondition(int code) {
    if (code == 0) return "Clear Sky";
    if (code == 1 || code == 2) return "Partly Cloudy";
    if (code == 3) return "Overcast";
    if (code == 45 || code == 48) return "Foggy";
    if (code >= 51 && code <= 55) return "Light Drizzle";
    if (code >= 61 && code <= 65) return "Rain Showers";
    if (code >= 71 && code <= 77) return "Snow Flurries";
    if (code >= 80 && code <= 82) return "Heavy Rain";
    if (code >= 95) return "Thunderstorm";
    return "Fair";
}

void assembleMarqueeText() {
    marqueeTickerText = "  >>> BREAKING NEWS: ";
    for (int i = 0; i < MAX_NEWS_ITEMS; i++) {
        marqueeTickerText += newsHeadlines[i];
        if (i < MAX_NEWS_ITEMS - 1) {
            marqueeTickerText += "   |*|   ";
        }
    }
    marqueeTickerText += "  <<<  ";
}

// =============================================================================
// High-Definition Vector 7-Segment Digit Builder (Crisp Rounded Segments)
// =============================================================================
void createVectorDigit(lv_obj_t* parent, Digit7Seg* d, int x, int y, int w, int h, int t, bool isSec) {
    int segH_Len = w - (2 * t);        // Length of horizontal segments (A, G, D)
    int segV_Len = (h - (3 * t)) / 2;  // Length of vertical segments (B, C, E, F)
    int rad = t / 2;
    lv_color_t initColor = isSec ? COLOR_SEC_OFF : COLOR_SEG_OFF;

    // Helper macro to create and style a single segment
    auto makeSeg = [&](int sx, int sy, int sw, int sh) -> lv_obj_t* {
        lv_obj_t* obj = lv_obj_create(parent);
        lv_obj_set_size(obj, sw, sh);
        lv_obj_set_pos(obj, sx, sy);
        lv_obj_set_style_bg_color(obj, initColor, 0);
        lv_obj_set_style_border_width(obj, 0, 0);
        lv_obj_set_style_radius(obj, rad, 0);
        lv_obj_set_style_pad_all(obj, 0, 0);
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        return obj;
    };

    // Segment A (Top Horizontal)
    d->seg[0] = makeSeg(x + t, y, segH_Len, t);

    // Segment B (Top-Right Vertical)
    d->seg[1] = makeSeg(x + w - t, y + t, t, segV_Len);

    // Segment C (Bottom-Right Vertical)
    d->seg[2] = makeSeg(x + w - t, y + (2 * t) + segV_Len, t, segV_Len);

    // Segment D (Bottom Horizontal)
    d->seg[3] = makeSeg(x + t, y + h - t, segH_Len, t);

    // Segment E (Bottom-Left Vertical)
    d->seg[4] = makeSeg(x, y + (2 * t) + segV_Len, t, segV_Len);

    // Segment F (Top-Left Vertical)
    d->seg[5] = makeSeg(x, y + t, t, segV_Len);

    // Segment G (Middle Horizontal)
    d->seg[6] = makeSeg(x + t, y + t + segV_Len, segH_Len, t);
}

void setVectorDigitValue(Digit7Seg* d, int val, bool isSec) {
    if (val < 0 || val > 9) return;
    uint8_t mask = DIGIT_MASKS[val];

    lv_color_t cOn  = isSec ? COLOR_SEC_ON  : COLOR_SEG_ON;
    lv_color_t cOff = isSec ? COLOR_SEC_OFF : COLOR_SEG_OFF;

    for (int i = 0; i < 7; i++) {
        bool on = (mask & (1 << i));
        lv_obj_set_style_bg_color(d->seg[i], on ? cOn : cOff, 0);
        lv_obj_set_style_shadow_width(d->seg[i], on ? (isSec ? 4 : 8) : 0, 0);
        lv_obj_set_style_shadow_color(d->seg[i], on ? cOn : cOff, 0);
        lv_obj_set_style_shadow_opa(d->seg[i], on ? LV_OPA_60 : LV_OPA_0, 0);
    }
}

// =============================================================================
// Phone Web Captive Portal
// =============================================================================
void startWebPortal() {
    if (apPortalRunning) return;

    Serial.println("[AP PORTAL] Starting Wi-Fi setup Access Point...");
    WiFi.disconnect();
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID);

    IPAddress myIP = WiFi.softAPIP();
    dnsServer.start(53, "*", myIP);

    server.on("/", handleWebRoot);
    server.on("/save", HTTP_POST, handleWebSave);
    server.onNotFound([]() {
        server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
        server.send(302, "text/plain", "");
    });
    server.begin();

    apPortalRunning = true;

    bsp_display_lock(0);
    if (modal_ap_portal) {
        lv_obj_clear_flag(modal_ap_portal, LV_OBJ_FLAG_HIDDEN);
        char modalBuf[256];
        snprintf(modalBuf, sizeof(modalBuf),
                 "1. On your phone/PC, connect to Wi-Fi:\n   '%s'\n\n2. Open browser and visit:\n   http://%s\n\n3. Select your home network & enter password.",
                 AP_SSID, myIP.toString().c_str());
        lv_label_set_text(lbl_ap_modal_info, modalBuf);
    }
    bsp_display_unlock();
}

void stopWebPortal() {
    if (!apPortalRunning) return;

    server.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    apPortalRunning = false;

    bsp_display_lock(0);
    if (modal_ap_portal) {
        lv_obj_add_flag(modal_ap_portal, LV_OBJ_FLAG_HIDDEN);
    }
    bsp_display_unlock();
}

void handleWebRoot() {
    int n = WiFi.scanNetworks();
    String optionsHtml = "";
    if (n == 0) {
        optionsHtml = "<option value=''>No networks found. Type manually below.</option>";
    } else {
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            int rssi = WiFi.RSSI(i);
            if (ssid.length() > 0) {
                optionsHtml += "<option value='" + ssid + "'>" + ssid + " (" + String(rssi) + " dBm)</option>";
            }
        }
    }

    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>ESP32 Smart Clock - Wi-Fi Setup</title>"
                  "<style>"
                  "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;background:#0d1117;color:#c9d1d9;padding:20px;text-align:center;margin:0;}"
                  ".card{background:#161b22;border:1px solid #30363d;border-radius:12px;padding:24px;max-width:380px;margin:20px auto;box-shadow:0 8px 24px rgba(0,0,0,0.5);}"
                  "h2{color:#58a6ff;margin-top:0;font-size:22px;}"
                  "p{color:#8b949e;font-size:14px;line-height:1.5;}"
                  "label{display:block;text-align:left;color:#f0f6fc;font-size:13px;font-weight:600;margin:12px 0 6px 0;}"
                  "select,input{width:100%;box-sizing:border-box;padding:12px;border:1px solid #30363d;background:#0d1117;color:#fff;border-radius:8px;font-size:15px;margin-bottom:12px;outline:none;}"
                  "select:focus,input:focus{border-color:#58a6ff;}"
                  ".btn{background:#238636;color:#fff;border:none;padding:14px;width:100%;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer;margin-top:12px;}"
                  ".btn:hover{background:#2ea043;}"
                  "</style></head><body>"
                  "<div class='card'>"
                  "<h2>📶 ESP32 Smart Clock</h2>"
                  "<p>Select your home or office 2.4GHz Wi-Fi network to connect your Smart Clock.</p>"
                  "<form action='/save' method='POST'>"
                  "<label>Select Discovered Network:</label>"
                  "<select name='ssid' id='ssid_select' onchange='document.getElementById(\"manual_ssid\").value=this.value;'>"
                  "<option value=''>-- Select Network --</option>" + optionsHtml + "</select>"
                  "<label>Or Type Wi-Fi SSID:</label>"
                  "<input type='text' name='manual_ssid' id='manual_ssid' placeholder='Network Name (SSID)'>"
                  "<label>Wi-Fi Password:</label>"
                  "<input type='password' name='pass' placeholder='Enter Wi-Fi Password' autofocus>"
                  "<button type='submit' class='btn'>Connect & Save</button>"
                  "</form></div></body></html>";

    server.send(200, "text/html", html);
}

void handleWebSave() {
    String selectedSSID = server.arg("manual_ssid");
    if (selectedSSID.length() == 0) {
        selectedSSID = server.arg("ssid");
    }
    String pass = server.arg("pass");

    String resHtml = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                     "<title>Connecting...</title>"
                     "<style>body{font-family:sans-serif;background:#0d1117;color:#fff;text-align:center;padding:40px;}"
                     ".box{background:#161b22;padding:30px;border-radius:12px;max-width:360px;margin:auto;border:1px solid #30363d;}"
                     "h2{color:#3fb950;}</style></head><body>"
                     "<div class='box'><h2>✔ Configuration Saved!</h2>"
                     "<p>Smart Clock is now connecting to <b>" + selectedSSID + "</b>.</p>"
                     "<p style='color:#8b949e;font-size:13px;'>You can now reconnect your phone to your regular Wi-Fi.</p>"
                     "</div></body></html>";

    server.send(200, "text/html", resHtml);
    delay(1000);

    stopWebPortal();

    prefs.begin("wifi_cfg", false);
    prefs.putString("ssid", selectedSSID);
    prefs.putString("pass", pass);
    prefs.end();

    currentSSID = selectedSSID;
    currentPass = pass;

    initWiFi();
    if (WiFi.status() == WL_CONNECTED) {
        syncNTPTime();
        fetchWeather();
        fetchNews();
    }
    updateSystemStats();
}

// =============================================================================
// Pomodoro Focus Timer Logic
// =============================================================================
static void btn_pomo_toggle_cb(lv_event_t* e) {
    pomodoroRunning = !pomodoroRunning;
    bsp_display_lock(0);
    if (pomodoroRunning) {
        lv_label_set_text(lbl_btn_pomo, LV_SYMBOL_PAUSE " PAUSE FOCUS");
        lv_obj_set_style_bg_color(btn_pomo_toggle, lv_color_hex(0xFF9100), 0);
        lv_label_set_text(lbl_pomo_status, "STAY FOCUSED! WORKING IN PROGRESS...");
        lv_obj_set_style_text_color(lbl_pomo_status, lv_color_hex(0x00E676), 0);
    } else {
        lv_label_set_text(lbl_btn_pomo, LV_SYMBOL_PLAY " START FOCUS (25 MIN)");
        lv_obj_set_style_bg_color(btn_pomo_toggle, lv_color_hex(0x00B0FF), 0);
        lv_label_set_text(lbl_pomo_status, "TIMER PAUSED");
        lv_obj_set_style_text_color(lbl_pomo_status, lv_color_hex(0xFFD600), 0);
    }
    bsp_display_unlock();
}

static void btn_pomo_reset_cb(lv_event_t* e) {
    pomodoroRunning = false;
    pomodoroRemainSec = pomodoroTotalSec;
    bsp_display_lock(0);
    lv_label_set_text(lbl_pomo_time, "25:00");
    lv_bar_set_value(bar_pomo_progress, 100, LV_ANIM_OFF);
    lv_label_set_text(lbl_btn_pomo, LV_SYMBOL_PLAY " START FOCUS (25 MIN)");
    lv_obj_set_style_bg_color(btn_pomo_toggle, lv_color_hex(0x00B0FF), 0);
    lv_label_set_text(lbl_pomo_status, "READY TO START");
    lv_obj_set_style_text_color(lbl_pomo_status, lv_color_hex(0x80D8FF), 0);
    bsp_display_unlock();
}

static void btn_pomo_add5_cb(lv_event_t* e) {
    pomodoroRemainSec += 300;
    pomodoroTotalSec += 300;
    int mins = pomodoroRemainSec / 60;
    int secs = pomodoroRemainSec % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);

    bsp_display_lock(0);
    lv_label_set_text(lbl_pomo_time, buf);
    bsp_display_unlock();
}

// =============================================================================
// LVGL Event Handlers
// =============================================================================
static void btn_refresh_news_cb(lv_event_t* e) {
    fetchNews();
}

static void btn_start_ap_portal_cb(lv_event_t* e) {
    startWebPortal();
}

static void btn_cancel_ap_portal_cb(lv_event_t* e) {
    stopWebPortal();
    initWiFi();
    updateSystemStats();
}

static void slider_bright_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    currentBrightness = (int)lv_slider_get_value(slider);
    bsp_display_brightness_set(currentBrightness);

    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", currentBrightness);
    lv_label_set_text(lbl_bright_val, buf);
}

// =============================================================================
// UI Construction (Luxury Giant Vector Clock Dashboard: 480x320)
// =============================================================================
void buildDashboardUI() {
    bsp_display_lock(0);

    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // 1. Main Tabview (Compact Tab Bar: H: 30)
    tv_main = lv_tabview_create(scr, LV_DIR_TOP, 30);
    lv_obj_set_size(tv_main, 480, 320);
    lv_obj_align(tv_main, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(tv_main, lv_color_black(), 0);

    lv_obj_t* tab_content = lv_tabview_get_content(tv_main);
    lv_obj_set_style_bg_color(tab_content, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(tab_content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tab_content, 0, 0);
    lv_obj_set_style_pad_all(tab_content, 0, 0);

    lv_obj_t* tab_btns = lv_tabview_get_tab_btns(tv_main);
    lv_obj_set_style_bg_color(tab_btns, lv_color_black(), 0);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(0x546E7A), 0);
    lv_obj_set_style_text_font(tab_btns, &lv_font_montserrat_12, 0);
    lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(tab_btns, lv_color_hex(0x101820), 0);
    lv_obj_set_style_border_width(tab_btns, 1, 0);

    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(tab_btns, LV_OPA_COVER, LV_PART_INDICATOR);

    // 4 Clean Tabs
    lv_obj_t* tab_clock = lv_tabview_add_tab(tv_main, LV_SYMBOL_BELL " CLOCK");
    lv_obj_t* tab_focus = lv_tabview_add_tab(tv_main, LV_SYMBOL_PLAY " FOCUS TIMER");
    lv_obj_t* tab_news = lv_tabview_add_tab(tv_main, LV_SYMBOL_LIST " NEWS FEED");
    lv_obj_t* tab_settings = lv_tabview_add_tab(tv_main, LV_SYMBOL_SETTINGS " SETTINGS");

    lv_obj_set_style_bg_color(tab_clock, lv_color_black(), 0);
    lv_obj_set_style_bg_color(tab_focus, lv_color_black(), 0);
    lv_obj_set_style_bg_color(tab_news, lv_color_black(), 0);
    lv_obj_set_style_bg_color(tab_settings, lv_color_black(), 0);

    lv_obj_set_style_pad_all(tab_clock, 0, 0);
    lv_obj_set_style_pad_all(tab_focus, 6, 0);
    lv_obj_set_style_pad_all(tab_news, 6, 0);
    lv_obj_set_style_pad_all(tab_settings, 6, 0);

    // =========================================================================
    // TAB 1: LUXURY GIANT VECTOR CLOCK DASHBOARD (Pure OLED Black Background)
    // =========================================================================
    // 1. Top Neat Info Capsule (W: 480, H: 26, Pos: X: 0, Y: 2)
    lv_obj_t* bar_top_info = lv_obj_create(tab_clock);
    lv_obj_set_size(bar_top_info, 480, 26);
    lv_obj_set_pos(bar_top_info, 0, 2);
    lv_obj_set_style_bg_color(bar_top_info, lv_color_black(), 0);
    lv_obj_set_style_border_width(bar_top_info, 0, 0);
    lv_obj_set_style_pad_all(bar_top_info, 2, 0);
    lv_obj_clear_flag(bar_top_info, LV_OBJ_FLAG_SCROLLABLE);

    lbl_top_date = lv_label_create(bar_top_info);
    lv_label_set_text(lbl_top_date, "Mon, Aug 31, 2026");
    lv_obj_set_style_text_color(lbl_top_date, lv_color_hex(0x80D8FF), 0);
    lv_obj_set_style_text_font(lbl_top_date, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_top_date, LV_ALIGN_LEFT_MID, 12, 0);

    lbl_top_weather = lv_label_create(bar_top_info);
    lv_label_set_text(lbl_top_weather, "--°C • --");
    lv_obj_set_style_text_color(lbl_top_weather, lv_color_hex(0xFFD600), 0);
    lv_obj_set_style_text_font(lbl_top_weather, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_top_weather, LV_ALIGN_CENTER, 30, 0);

    lbl_top_wifi = lv_label_create(bar_top_info);
    lv_label_set_text(lbl_top_wifi, LV_SYMBOL_WIFI " Online");
    lv_obj_set_style_text_color(lbl_top_wifi, lv_color_hex(0x00E676), 0);
    lv_obj_set_style_text_font(lbl_top_wifi, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_top_wifi, LV_ALIGN_RIGHT_MID, -12, 0);

    // 2. GIANT 140px VECTOR DIGITAL CLOCK STAGE (W: 480, H: 200, Pos: X: 0, Y: 28)
    pnl_clock_stage = lv_obj_create(tab_clock);
    lv_obj_set_size(pnl_clock_stage, 480, 200);
    lv_obj_set_pos(pnl_clock_stage, 0, 28);
    lv_obj_set_style_bg_color(pnl_clock_stage, lv_color_black(), 0);
    lv_obj_set_style_border_width(pnl_clock_stage, 0, 0);
    lv_obj_set_style_pad_all(pnl_clock_stage, 0, 0);
    lv_obj_clear_flag(pnl_clock_stage, LV_OBJ_FLAG_SCROLLABLE);

    // Construct 4 Massive Vector Digits (Width: 64px, Height: 142px, Thickness: 12px)
    // Digit 1: Tens of Hour (X: 20, Y: 28)
    createVectorDigit(pnl_clock_stage, &digitH1, 20, 28, 64, 142, 12, false);

    // Digit 2: Units of Hour (X: 96, Y: 28)
    createVectorDigit(pnl_clock_stage, &digitH2, 96, 28, 64, 142, 12, false);

    // Flashing Colon Dots (X: 174, Y: 68 / Y: 124)
    colonDot1 = lv_obj_create(pnl_clock_stage);
    lv_obj_set_size(colonDot1, 14, 14);
    lv_obj_set_pos(colonDot1, 174, 68);
    lv_obj_set_style_bg_color(colonDot1, COLOR_SEG_ON, 0);
    lv_obj_set_style_border_width(colonDot1, 0, 0);
    lv_obj_set_style_radius(colonDot1, 7, 0);

    colonDot2 = lv_obj_create(pnl_clock_stage);
    lv_obj_set_size(colonDot2, 14, 14);
    lv_obj_set_pos(colonDot2, 174, 124);
    lv_obj_set_style_bg_color(colonDot2, COLOR_SEG_ON, 0);
    lv_obj_set_style_border_width(colonDot2, 0, 0);
    lv_obj_set_style_radius(colonDot2, 7, 0);

    // Digit 3: Tens of Min (X: 202, Y: 28)
    createVectorDigit(pnl_clock_stage, &digitM1, 202, 28, 64, 142, 12, false);

    // Digit 4: Units of Min (X: 278, Y: 28)
    createVectorDigit(pnl_clock_stage, &digitM2, 278, 28, 64, 142, 12, false);

    // AM/PM Indicator
    lbl_ampm_badge = lv_label_create(pnl_clock_stage);
    lv_label_set_text(lbl_ampm_badge, "AM");
    lv_obj_set_style_text_color(lbl_ampm_badge, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(lbl_ampm_badge, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl_ampm_badge, 368, 32);

    // Construct 2 Smaller Gold Seconds Digits (Width: 38px, Height: 80px, Thickness: 8px)
    // Digit S1: Tens of Sec (X: 362, Y: 72)
    createVectorDigit(pnl_clock_stage, &digitS1, 362, 72, 38, 80, 8, true);

    // Digit S2: Units of Sec (X: 412, Y: 72)
    createVectorDigit(pnl_clock_stage, &digitS2, 412, 72, 38, 80, 8, true);

    // 3. Bottom Smooth Marquee News Ribbon (W: 480, H: 44, Pos: X: 0, Y: 236)
    lv_obj_t* bar_ticker = lv_obj_create(tab_clock);
    lv_obj_set_size(bar_ticker, 480, 44);
    lv_obj_set_pos(bar_ticker, 0, 236);
    lv_obj_set_style_bg_color(bar_ticker, lv_color_black(), 0);
    lv_obj_set_style_border_width(bar_ticker, 0, 0);
    lv_obj_set_style_pad_all(bar_ticker, 2, 0);
    lv_obj_clear_flag(bar_ticker, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_t_badge = lv_label_create(bar_ticker);
    lv_label_set_text(lbl_t_badge, LV_SYMBOL_BULLET " BREAKING NEWS");
    lv_obj_set_style_text_color(lbl_t_badge, lv_color_hex(0xFF1744), 0);
    lv_obj_set_style_text_font(lbl_t_badge, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_t_badge, LV_ALIGN_TOP_LEFT, 12, 0);

    lbl_news_ticker = lv_label_create(bar_ticker);
    lv_label_set_long_mode(lbl_news_ticker, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl_news_ticker, 456);
    lv_label_set_text(lbl_news_ticker, marqueeTickerText.c_str());
    lv_obj_set_style_text_color(lbl_news_ticker, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_news_ticker, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_news_ticker, LV_ALIGN_BOTTOM_LEFT, 12, -2);

    // =========================================================================
    // TAB 2: POMODORO FOCUS TIMER
    // =========================================================================
    lv_obj_t* card_pomo = lv_obj_create(tab_focus);
    lv_obj_set_size(card_pomo, 454, 165);
    lv_obj_set_pos(card_pomo, 4, 4);
    lv_obj_set_style_bg_color(card_pomo, lv_color_hex(0x0E1620), 0);
    lv_obj_set_style_border_color(card_pomo, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(card_pomo, 1, 0);
    lv_obj_set_style_radius(card_pomo, 12, 0);
    lv_obj_set_style_pad_all(card_pomo, 10, 0);
    lv_obj_clear_flag(card_pomo, LV_OBJ_FLAG_SCROLLABLE);

    lbl_pomo_time = lv_label_create(card_pomo);
    lv_label_set_text(lbl_pomo_time, "25:00");
    lv_obj_set_style_text_color(lbl_pomo_time, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_pomo_time, &lv_font_montserrat_48, 0);
    lv_obj_align(lbl_pomo_time, LV_ALIGN_TOP_MID, 0, 4);

    bar_pomo_progress = lv_bar_create(card_pomo);
    lv_obj_set_size(bar_pomo_progress, 420, 10);
    lv_obj_align(bar_pomo_progress, LV_ALIGN_TOP_MID, 0, 68);
    lv_bar_set_range(bar_pomo_progress, 0, 100);
    lv_bar_set_value(bar_pomo_progress, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_pomo_progress, lv_color_hex(0x1F2C38), 0);
    lv_obj_set_style_bg_color(bar_pomo_progress, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);

    lbl_pomo_status = lv_label_create(card_pomo);
    lv_label_set_text(lbl_pomo_status, "READY TO START WORK SESSION");
    lv_obj_set_style_text_color(lbl_pomo_status, lv_color_hex(0x80D8FF), 0);
    lv_obj_set_style_text_font(lbl_pomo_status, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_pomo_status, LV_ALIGN_TOP_MID, 0, 84);

    btn_pomo_toggle = lv_btn_create(card_pomo);
    lv_obj_set_size(btn_pomo_toggle, 190, 42);
    lv_obj_set_pos(btn_pomo_toggle, 10, 106);
    lv_obj_set_style_bg_color(btn_pomo_toggle, lv_color_hex(0x00B0FF), 0);
    lv_obj_set_style_radius(btn_pomo_toggle, 8, 0);
    lv_obj_add_event_cb(btn_pomo_toggle, btn_pomo_toggle_cb, LV_EVENT_CLICKED, NULL);

    lbl_btn_pomo = lv_label_create(btn_pomo_toggle);
    lv_label_set_text(lbl_btn_pomo, LV_SYMBOL_PLAY " START FOCUS");
    lv_obj_set_style_text_color(lbl_btn_pomo, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(lbl_btn_pomo, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_btn_pomo);

    lv_obj_t* btn_pomo_reset = lv_btn_create(card_pomo);
    lv_obj_set_size(btn_pomo_reset, 100, 42);
    lv_obj_set_pos(btn_pomo_reset, 210, 106);
    lv_obj_set_style_bg_color(btn_pomo_reset, lv_color_hex(0x233748), 0);
    lv_obj_set_style_radius(btn_pomo_reset, 8, 0);
    lv_obj_add_event_cb(btn_pomo_reset, btn_pomo_reset_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_bpr = lv_label_create(btn_pomo_reset);
    lv_label_set_text(lbl_bpr, LV_SYMBOL_REFRESH " RESET");
    lv_obj_set_style_text_color(lbl_bpr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_bpr, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_bpr);

    lv_obj_t* btn_pomo_add = lv_btn_create(card_pomo);
    lv_obj_set_size(btn_pomo_add, 110, 42);
    lv_obj_set_pos(btn_pomo_add, 320, 106);
    lv_obj_set_style_bg_color(btn_pomo_add, lv_color_hex(0x182430), 0);
    lv_obj_set_style_radius(btn_pomo_add, 8, 0);
    lv_obj_add_event_cb(btn_pomo_add, btn_pomo_add5_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_bpa = lv_label_create(btn_pomo_add);
    lv_label_set_text(lbl_bpa, "+5 MIN");
    lv_obj_set_style_text_color(lbl_bpa, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(lbl_bpa, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_bpa);

    // Quote Card
    lv_obj_t* card_quote = lv_obj_create(tab_focus);
    lv_obj_set_size(card_quote, 454, 85);
    lv_obj_set_pos(card_quote, 4, 175);
    lv_obj_set_style_bg_color(card_quote, lv_color_hex(0x0E1620), 0);
    lv_obj_set_style_border_color(card_quote, lv_color_hex(0x1A2530), 0);
    lv_obj_set_style_radius(card_quote, 10, 0);
    lv_obj_set_style_pad_all(card_quote, 8, 0);
    lv_obj_clear_flag(card_quote, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_qt_tag = lv_label_create(card_quote);
    lv_label_set_text(lbl_qt_tag, "💡 DAILY THOUGHT & WISDOM");
    lv_obj_set_style_text_color(lbl_qt_tag, lv_color_hex(0xFFD600), 0);
    lv_obj_set_style_text_font(lbl_qt_tag, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_qt_tag, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_quote_text = lv_label_create(card_quote);
    lv_label_set_text(lbl_quote_text, DAILY_QUOTES[0]);
    lv_obj_set_style_text_color(lbl_quote_text, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_quote_text, &lv_font_montserrat_12, 0);
    lv_obj_set_width(lbl_quote_text, 436);
    lv_label_set_long_mode(lbl_quote_text, LV_LABEL_LONG_WRAP);
    lv_obj_align(lbl_quote_text, LV_ALIGN_TOP_LEFT, 0, 20);

    // =========================================================================
    // TAB 3: FULL NEWS FEED READER
    // =========================================================================
    lv_obj_t* card_full_news = lv_obj_create(tab_news);
    lv_obj_set_size(card_full_news, 454, 200);
    lv_obj_set_pos(card_full_news, 4, 4);
    lv_obj_set_style_bg_color(card_full_news, lv_color_hex(0x0E1620), 0);
    lv_obj_set_style_border_color(card_full_news, lv_color_hex(0x1A2530), 0);
    lv_obj_set_style_radius(card_full_news, 12, 0);
    lv_obj_set_style_pad_all(card_full_news, 12, 0);

    lv_obj_t* lbl_fn_header = lv_label_create(card_full_news);
    lv_label_set_text(lbl_fn_header, "TOP BREAKING NEWS FEED (ONLINE)");
    lv_obj_set_style_text_color(lbl_fn_header, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(lbl_fn_header, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_fn_header, LV_ALIGN_TOP_LEFT, 0, 0);

    String allNewsText = "";
    for (int i = 0; i < MAX_NEWS_ITEMS; i++) {
        allNewsText += String(i + 1) + ". " + newsHeadlines[i] + "\n\n";
    }

    lbl_full_news = lv_label_create(card_full_news);
    lv_label_set_text(lbl_full_news, allNewsText.c_str());
    lv_obj_set_style_text_color(lbl_full_news, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_full_news, &lv_font_montserrat_12, 0);
    lv_obj_set_width(lbl_full_news, 426);
    lv_label_set_long_mode(lbl_full_news, LV_LABEL_LONG_WRAP);
    lv_obj_align(lbl_full_news, LV_ALIGN_TOP_LEFT, 0, 26);

    lv_obj_t* btn_ref_news = lv_btn_create(tab_news);
    lv_obj_set_size(btn_ref_news, 454, 40);
    lv_obj_set_pos(btn_ref_news, 4, 210);
    lv_obj_set_style_bg_color(btn_ref_news, lv_color_hex(0x00B0FF), 0);
    lv_obj_set_style_radius(btn_ref_news, 8, 0);
    lv_obj_add_event_cb(btn_ref_news, btn_refresh_news_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_brn = lv_label_create(btn_ref_news);
    lv_label_set_text(lbl_brn, LV_SYMBOL_REFRESH " REFRESH LATEST HEADLINES NOW");
    lv_obj_set_style_text_color(lbl_brn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(lbl_brn, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_brn);

    // =========================================================================
    // TAB 4: SETTINGS & CLEAN WI-FI STATUS CARD
    // =========================================================================
    lv_obj_t* card_wifi_info = lv_obj_create(tab_settings);
    lv_obj_set_size(card_wifi_info, 454, 125);
    lv_obj_set_pos(card_wifi_info, 4, 4);
    lv_obj_set_style_bg_color(card_wifi_info, lv_color_hex(0x0E1620), 0);
    lv_obj_set_style_border_color(card_wifi_info, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(card_wifi_info, 1, 0);
    lv_obj_set_style_radius(card_wifi_info, 12, 0);
    lv_obj_set_style_pad_all(card_wifi_info, 8, 0);
    lv_obj_clear_flag(card_wifi_info, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_wi_title = lv_label_create(card_wifi_info);
    lv_label_set_text(lbl_wi_title, LV_SYMBOL_WIFI " WI-FI NETWORK CONNECTION");
    lv_obj_set_style_text_color(lbl_wi_title, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(lbl_wi_title, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_wi_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_wifi_ssid_display = lv_label_create(card_wifi_info);
    lv_label_set_text(lbl_wifi_ssid_display, "SSID: Connecting...");
    lv_obj_set_style_text_color(lbl_wifi_ssid_display, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_wifi_ssid_display, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_wifi_ssid_display, LV_ALIGN_TOP_LEFT, 0, 20);

    lbl_wifi_ip_display = lv_label_create(card_wifi_info);
    lv_label_set_text(lbl_wifi_ip_display, "IP Address: 0.0.0.0");
    lv_obj_set_style_text_color(lbl_wifi_ip_display, lv_color_hex(0x80D8FF), 0);
    lv_obj_set_style_text_font(lbl_wifi_ip_display, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_wifi_ip_display, LV_ALIGN_TOP_LEFT, 0, 40);

    lbl_wifi_rssi_display = lv_label_create(card_wifi_info);
    lv_label_set_text(lbl_wifi_rssi_display, "Signal: -- dBm");
    lv_obj_set_style_text_color(lbl_wifi_rssi_display, lv_color_hex(0x00E676), 0);
    lv_obj_set_style_text_font(lbl_wifi_rssi_display, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_wifi_rssi_display, LV_ALIGN_TOP_RIGHT, 0, 20);

    btn_start_ap_portal = lv_btn_create(card_wifi_info);
    lv_obj_set_size(btn_start_ap_portal, 434, 34);
    lv_obj_align(btn_start_ap_portal, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_start_ap_portal, lv_color_hex(0x00B0FF), 0);
    lv_obj_set_style_radius(btn_start_ap_portal, 8, 0);
    lv_obj_add_event_cb(btn_start_ap_portal, btn_start_ap_portal_cb, LV_EVENT_CLICKED, NULL);

    lbl_btn_start_ap = lv_label_create(btn_start_ap_portal);
    lv_label_set_text(lbl_btn_start_ap, LV_SYMBOL_SETTINGS " SETUP WI-FI VIA PHONE / PC BROWSER");
    lv_obj_set_style_text_color(lbl_btn_start_ap, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(lbl_btn_start_ap, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_btn_start_ap);

    // Card 2: Hardware Health & Display Brightness
    lv_obj_t* card_sys = lv_obj_create(tab_settings);
    lv_obj_set_size(card_sys, 454, 120);
    lv_obj_set_pos(card_sys, 4, 134);
    lv_obj_set_style_bg_color(card_sys, lv_color_hex(0x0E1620), 0);
    lv_obj_set_style_border_color(card_sys, lv_color_hex(0x1A2530), 0);
    lv_obj_set_style_radius(card_sys, 12, 0);
    lv_obj_set_style_pad_all(card_sys, 8, 0);
    lv_obj_clear_flag(card_sys, LV_OBJ_FLAG_SCROLLABLE);

    lbl_sys_heap = lv_label_create(card_sys);
    lv_label_set_text(lbl_sys_heap, "Internal Heap: -- KB Free");
    lv_obj_set_style_text_color(lbl_sys_heap, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_sys_heap, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_sys_heap, LV_ALIGN_TOP_LEFT, 0, 0);

    lbl_sys_psram = lv_label_create(card_sys);
    lv_label_set_text(lbl_sys_psram, "Octal PSRAM: 8192 KB Total");
    lv_obj_set_style_text_color(lbl_sys_psram, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(lbl_sys_psram, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_sys_psram, LV_ALIGN_TOP_RIGHT, 0, 0);

    lbl_sys_uptime = lv_label_create(card_sys);
    lv_label_set_text(lbl_sys_uptime, "System Uptime: 00:00:00");
    lv_obj_set_style_text_color(lbl_sys_uptime, lv_color_hex(0xB0BEC5), 0);
    lv_obj_set_style_text_font(lbl_sys_uptime, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_sys_uptime, LV_ALIGN_TOP_LEFT, 0, 20);

    lv_obj_t* lbl_b_title = lv_label_create(card_sys);
    lv_label_set_text(lbl_b_title, LV_SYMBOL_EYE_OPEN " DISPLAY BACKLIGHT BRIGHTNESS");
    lv_obj_set_style_text_color(lbl_b_title, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(lbl_b_title, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_b_title, LV_ALIGN_BOTTOM_LEFT, 0, -22);

    lbl_bright_val = lv_label_create(card_sys);
    lv_label_set_text(lbl_bright_val, "90%");
    lv_obj_set_style_text_color(lbl_bright_val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_bright_val, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_bright_val, LV_ALIGN_BOTTOM_RIGHT, 0, -22);

    slider_bright = lv_slider_create(card_sys);
    lv_obj_set_size(slider_bright, 434, 16);
    lv_obj_align(slider_bright, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_slider_set_range(slider_bright, 10, 100);
    lv_slider_set_value(slider_bright, currentBrightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider_bright, lv_color_hex(0x1F2C38), 0);
    lv_obj_set_style_bg_color(slider_bright, lv_color_hex(0x00E5FF), LV_PART_INDICATOR);
    lv_obj_add_event_cb(slider_bright, slider_bright_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Modal Setup Dialog
    modal_ap_portal = lv_obj_create(scr);
    lv_obj_set_size(modal_ap_portal, 460, 280);
    lv_obj_center(modal_ap_portal);
    lv_obj_set_style_bg_color(modal_ap_portal, lv_color_hex(0x0A1118), 0);
    lv_obj_set_style_border_color(modal_ap_portal, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_border_width(modal_ap_portal, 2, 0);
    lv_obj_set_style_radius(modal_ap_portal, 16, 0);
    lv_obj_set_style_pad_all(modal_ap_portal, 14, 0);
    lv_obj_clear_flag(modal_ap_portal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(modal_ap_portal, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* lbl_m_title = lv_label_create(modal_ap_portal);
    lv_label_set_text(lbl_m_title, "📶 WI-FI PHONE SETUP ACTIVE");
    lv_obj_set_style_text_color(lbl_m_title, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(lbl_m_title, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_m_title, LV_ALIGN_TOP_MID, 0, 0);

    lbl_ap_modal_info = lv_label_create(modal_ap_portal);
    lv_label_set_text(lbl_ap_modal_info, "Waiting for phone connection...");
    lv_obj_set_style_text_color(lbl_ap_modal_info, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_ap_modal_info, &lv_font_montserrat_14, 0);
    lv_obj_set_width(lbl_ap_modal_info, 420);
    lv_label_set_long_mode(lbl_ap_modal_info, LV_LABEL_LONG_WRAP);
    lv_obj_align(lbl_ap_modal_info, LV_ALIGN_TOP_LEFT, 0, 32);

    lv_obj_t* btn_cancel_ap = lv_btn_create(modal_ap_portal);
    lv_obj_set_size(btn_cancel_ap, 420, 42);
    lv_obj_align(btn_cancel_ap, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_cancel_ap, lv_color_hex(0xD32F2F), 0);
    lv_obj_set_style_radius(btn_cancel_ap, 8, 0);
    lv_obj_add_event_cb(btn_cancel_ap, btn_cancel_ap_portal_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_bca = lv_label_create(btn_cancel_ap);
    lv_label_set_text(lbl_bca, LV_SYMBOL_CLOSE " CANCEL & CLOSE SETUP");
    lv_obj_set_style_text_color(lbl_bca, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_bca, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_bca);

    bsp_display_unlock();
}

// =============================================================================
// Vector Digital Clock Display Update (Runs Every 500ms)
// =============================================================================
void updateClockDisplay() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    int rawHour = timeinfo.tm_hour;
    int hour12 = rawHour % 12;
    if (hour12 == 0) hour12 = 12;
    const char* ampmStr = (rawHour >= 12) ? "PM" : "AM";
    int currentMins = timeinfo.tm_min;
    int currentSecs = timeinfo.tm_sec;

    int h1 = hour12 / 10;
    int h2 = hour12 % 10;
    int m1 = currentMins / 10;
    int m2 = currentMins % 10;
    int s1 = currentSecs / 10;
    int s2 = currentSecs % 10;

    bsp_display_lock(0);

    // 1. Update 135px Vector Digits
    setVectorDigitValue(&digitH1, h1, false);
    setVectorDigitValue(&digitH2, h2, false);
    setVectorDigitValue(&digitM1, m1, false);
    setVectorDigitValue(&digitM2, m2, false);
    setVectorDigitValue(&digitS1, s1, true);
    setVectorDigitValue(&digitS2, s2, true);

    // 2. Soft Colon Dots Blink
    colonBlinkState = !colonBlinkState;
    lv_color_t cColon = colonBlinkState ? COLOR_SEG_ON : COLOR_SEG_OFF;
    lv_obj_set_style_bg_color(colonDot1, cColon, 0);
    lv_obj_set_style_bg_color(colonDot2, cColon, 0);
    lv_obj_set_style_shadow_width(colonDot1, colonBlinkState ? 8 : 0, 0);
    lv_obj_set_style_shadow_color(colonDot1, cColon, 0);
    lv_obj_set_style_shadow_width(colonDot2, colonBlinkState ? 8 : 0, 0);
    lv_obj_set_style_shadow_color(colonDot2, cColon, 0);

    // 3. Update AM/PM
    lv_label_set_text(lbl_ampm_badge, ampmStr);

    // 4. Update Top Ribbon Date
    const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char topDateBuf[48];
    snprintf(topDateBuf, sizeof(topDateBuf), "%s, %s %02d, %d",
             days[timeinfo.tm_wday], months[timeinfo.tm_mon], timeinfo.tm_mday, timeinfo.tm_year + 1900);
    lv_label_set_text(lbl_top_date, topDateBuf);

    bsp_display_unlock();
}

// =============================================================================
// Live Weather Fetcher
// =============================================================================
void fetchWeather() {
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.println("[WEATHER] Fetching live forecast capsule...");

    HTTPClient http;
    http.begin(WEATHER_API_URL);
    http.setTimeout(5000);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            currentWeather.temp = doc["current"]["temperature_2m"];
            currentWeather.feelsLike = doc["current"]["apparent_temperature"];
            currentWeather.humidity = doc["current"]["relative_humidity_2m"];
            currentWeather.windSpeed = doc["current"]["wind_speed_10m"];
            currentWeather.weatherCode = doc["current"]["weather_code"];
            currentWeather.tempMax = doc["daily"]["temperature_2m_max"][0];
            currentWeather.tempMin = doc["daily"]["temperature_2m_min"][0];
            currentWeather.condition = getWeatherCondition(currentWeather.weatherCode);
            currentWeather.valid = true;

            Serial.printf("[WEATHER] Temp: %.1f C | Humidity: %d%% | %s\n",
                          currentWeather.temp, currentWeather.humidity, currentWeather.condition.c_str());

            updateWeatherDisplay();
        }
    }
    http.end();
}

void updateWeatherDisplay() {
    if (!lbl_top_weather || !currentWeather.valid) return;

    bsp_display_lock(0);
    char topWeatherBuf[64];
    snprintf(topWeatherBuf, sizeof(topWeatherBuf), "%.1f°C • %s (Hum: %d%%)",
             currentWeather.temp, currentWeather.condition.c_str(), currentWeather.humidity);
    lv_label_set_text(lbl_top_weather, topWeatherBuf);
    bsp_display_unlock();
}

// =============================================================================
// Live Online News Fetcher
// =============================================================================
void fetchNews() {
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.println("[NEWS] Fetching live top breaking news headlines...");

    HTTPClient http;
    http.begin(NEWS_API_URL);
    http.setTimeout(7000);

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error && doc["articles"].is<JsonArray>()) {
            JsonArray articles = doc["articles"];
            int count = 0;
            for (JsonObject item : articles) {
                const char* title = item["title"];
                if (title && strlen(title) > 5) {
                    String cleanTitle = String(title);
                    cleanTitle.replace("’", "'");
                    cleanTitle.replace("‘", "'");
                    cleanTitle.replace("“", "\"");
                    cleanTitle.replace("”", "\"");
                    cleanTitle.replace("—", "-");
                    cleanTitle.replace("–", "-");
                    newsHeadlines[count] = cleanTitle;
                    count++;
                    if (count >= MAX_NEWS_ITEMS) break;
                }
            }

            assembleMarqueeText();
            updateNewsDisplay();
            Serial.printf("[NEWS] Successfully loaded %d live headlines!\n", count);
        }
    }
    http.end();
}

void updateNewsDisplay() {
    if (!lbl_news_ticker) return;

    bsp_display_lock(0);
    lv_label_set_text(lbl_news_ticker, marqueeTickerText.c_str());

    if (lbl_full_news) {
        String allNewsText = "";
        for (int i = 0; i < MAX_NEWS_ITEMS; i++) {
            allNewsText += String(i + 1) + ". " + newsHeadlines[i] + "\n\n";
        }
        lv_label_set_text(lbl_full_news, allNewsText.c_str());
    }
    bsp_display_unlock();
}

// =============================================================================
// Pomodoro Tick Handler
// =============================================================================
void updatePomodoro() {
    if (!pomodoroRunning) return;

    if (pomodoroRemainSec > 0) {
        pomodoroRemainSec--;
    } else {
        pomodoroRunning = false;
        bsp_display_lock(0);
        lv_label_set_text(lbl_pomo_status, "🎉 FOCUS SESSION COMPLETE! TAKE A BREAK");
        lv_obj_set_style_text_color(lbl_pomo_status, lv_color_hex(0x00E676), 0);
        lv_label_set_text(lbl_btn_pomo, LV_SYMBOL_PLAY " START AGAIN");
        bsp_display_unlock();
        return;
    }

    int mins = pomodoroRemainSec / 60;
    int secs = pomodoroRemainSec % 60;
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mins, secs);

    int progress = (pomodoroRemainSec * 100) / pomodoroTotalSec;

    bsp_display_lock(0);
    lv_label_set_text(lbl_pomo_time, timeBuf);
    lv_bar_set_value(bar_pomo_progress, progress, LV_ANIM_ON);
    bsp_display_unlock();
}

// =============================================================================
// System Stats Monitor
// =============================================================================
void updateSystemStats() {
    if (!lbl_sys_heap) return;

    bsp_display_lock(0);

    char heapBuf[48];
    snprintf(heapBuf, sizeof(heapBuf), "Internal Heap: %d KB Free", ESP.getFreeHeap() / 1024);
    lv_label_set_text(lbl_sys_heap, heapBuf);

    char psramBuf[48];
    snprintf(psramBuf, sizeof(psramBuf), "Octal PSRAM: %d KB / %d KB Free",
             ESP.getFreePsram() / 1024, ESP.getPsramSize() / 1024);
    lv_label_set_text(lbl_sys_psram, psramBuf);

    if (WiFi.status() == WL_CONNECTED) {
        char sBuf[64];
        snprintf(sBuf, sizeof(sBuf), "SSID: %s", WiFi.SSID().c_str());
        lv_label_set_text(lbl_wifi_ssid_display, sBuf);

        char ipBuf[64];
        snprintf(ipBuf, sizeof(ipBuf), "IP Address: %s", WiFi.localIP().toString().c_str());
        lv_label_set_text(lbl_wifi_ip_display, ipBuf);

        char rBuf[32];
        snprintf(rBuf, sizeof(rBuf), "Signal: %d dBm", WiFi.RSSI());
        lv_label_set_text(lbl_wifi_rssi_display, rBuf);
        lv_obj_set_style_text_color(lbl_wifi_rssi_display, lv_color_hex(0x00E676), 0);

        if (lbl_top_wifi) {
            char topWBuf[32];
            snprintf(topWBuf, sizeof(topWBuf), LV_SYMBOL_WIFI " %s", WiFi.SSID().c_str());
            lv_label_set_text(lbl_top_wifi, topWBuf);
        }
    } else if (apPortalRunning) {
        lv_label_set_text(lbl_wifi_ssid_display, "SSID: [Hotspot Mode]");
        char ipBuf[64];
        snprintf(ipBuf, sizeof(ipBuf), "IP Address: %s (Setup Active)", WiFi.softAPIP().toString().c_str());
        lv_label_set_text(lbl_wifi_ip_display, ipBuf);
        lv_label_set_text(lbl_wifi_rssi_display, "Status: AP Portal");
        lv_obj_set_style_text_color(lbl_wifi_rssi_display, lv_color_hex(0xFFD600), 0);
    } else {
        lv_label_set_text(lbl_wifi_ssid_display, "SSID: Disconnected");
        lv_label_set_text(lbl_wifi_ip_display, "IP Address: Not Connected");
        lv_label_set_text(lbl_wifi_rssi_display, "Signal: No Link");
        lv_obj_set_style_text_color(lbl_wifi_rssi_display, lv_color_hex(0xFF1744), 0);
        if (lbl_top_wifi) lv_label_set_text(lbl_top_wifi, LV_SYMBOL_WARNING " Offline");
    }

    unsigned long sec = millis() / 1000;
    int hrs = sec / 3600;
    int mins = (sec % 3600) / 60;
    int secs = sec % 60;
    char upBuf[48];
    snprintf(upBuf, sizeof(upBuf), "System Uptime: %02d:%02d:%02d", hrs, mins, secs);
    lv_label_set_text(lbl_sys_uptime, upBuf);

    bsp_display_unlock();
}

// =============================================================================
// Wi-Fi Setup & Initialization
// =============================================================================
void loadSavedCredentials() {
    prefs.begin("wifi_cfg", true);
    currentSSID = prefs.getString("ssid", DEFAULT_SSID);
    currentPass = prefs.getString("pass", DEFAULT_PASS);
    prefs.end();

    Serial.printf("[NVS] Loaded Wi-Fi SSID: '%s'\n", currentSSID.c_str());
}

void initWiFi() {
    WiFi.mode(WIFI_STA);
    Serial.printf("[WIFI] Connecting to network '%s'...\n", currentSSID.c_str());

    WiFi.begin(currentSSID.c_str(), currentPass.c_str());

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
        delay(200);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() != WL_CONNECTED && currentSSID != DEFAULT_SSID) {
        Serial.printf("\n[WIFI] Trying fallback network '%s'...\n", DEFAULT_SSID);
        WiFi.begin(DEFAULT_SSID, DEFAULT_PASS);
        retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < 20) {
            delay(200);
            Serial.print(".");
            retry++;
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WIFI] Connected Successfully!");
        Serial.printf("[WIFI] SSID: %s | IP: %s | RSSI: %d dBm\n",
                      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
        Serial.println("\n[WIFI] Could not connect to saved network. Ready for user Web Setup.");
    }
}

void syncNTPTime() {
    Serial.println("[NTP] Initializing Time Synchronization...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);

    struct tm timeinfo;
    int retry = 0;
    while (!getLocalTime(&timeinfo) && retry++ < 10) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[NTP] Time Synced Successfully!");
}

// =============================================================================
// Setup Routine
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println("\n=======================================================");
    Serial.println(" ESP32-S3 JC3248W535 Luxury Giant Vector Digital Clock ");
    Serial.println("=======================================================");

    loadSavedCredentials();
    assembleMarqueeText();

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
        .rotate = LV_DISP_ROT_90, // Landscape 480x320
    };

    bsp_display_start_with_config(&cfg);
    bsp_display_brightness_set(currentBrightness);
    bsp_display_backlight_on();

    initWiFi();

    if (WiFi.status() == WL_CONNECTED) {
        syncNTPTime();
    }

    buildDashboardUI();

    if (WiFi.status() == WL_CONNECTED) {
        fetchWeather();
        fetchNews();
    }
    updateClockDisplay();
    updateSystemStats();
}

// =============================================================================
// Main Loop
// =============================================================================
void loop() {
    unsigned long now = millis();

    if (apPortalRunning) {
        dnsServer.processNextRequest();
        server.handleClient();
    }

    // 1. Pomodoro Focus Timer Tick (Every 1000ms)
    if (now - lastPomoTick >= 1000) {
        lastPomoTick = now;
        updatePomodoro();
    }

    // 2. Update Clock every 500ms
    if (now - lastClockUpdate >= 500) {
        lastClockUpdate = now;
        updateClockDisplay();
    }

    // 3. Update System Stats every 3 seconds
    if (now - lastSystemStatUpdate >= 3000) {
        lastSystemStatUpdate = now;
        updateSystemStats();
    }

    // 4. Auto-Refresh Live News every 5 minutes (if connected)
    if (!apPortalRunning && WiFi.status() == WL_CONNECTED && now - lastNewsFetch >= (5 * 60 * 1000)) {
        lastNewsFetch = now;
        fetchNews();
    }

    // 5. Refresh Weather Forecast every 15 minutes (if connected)
    if (!apPortalRunning && WiFi.status() == WL_CONNECTED && now - lastWeatherFetch >= (15 * 60 * 1000)) {
        lastWeatherFetch = now;
        fetchWeather();
    }

    delay(20);
}
