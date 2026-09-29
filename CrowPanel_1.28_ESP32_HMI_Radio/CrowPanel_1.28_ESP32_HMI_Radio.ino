/*
 * =========================================================================================================
 * Project: CrowPanel 1.28" ESP32-S3 HMI Rotary Touch Golden Radio Firmware
 * Target:  Elecrow CrowPanel 1.28-inch HMI ESP32-S3 Rotary Display (240x240 IPS Round Touch Screen, DHE38128D)
 * Features:
 *   - Custom 240x240 Concentric Circular UI (LovyanGFX + LVGL 8.4)
 *   - CST816D Capacitive Round Touchscreen Controller
 *   - EC3501 Rotary Knob Encoder: Multi-mode (Station Tune <-> Volume Adjust), click & double-click ISR
 *   - 5x WS2812B NeoPixel Ambient Illumination: Audio Breathing, Volume VU Meter, Rotation Chase, Rainbow
 *   - Always-Accessible Dual-Mode WiFi (SoftAP 192.168.4.1 + Home Station WiFi)
 *   - Full Mobile-First Web Remote: Live Remote, Timer, Device Favorites, 277+ GitHub Catalog
 *   - Persistent NVS Storage (Preferences) with 7 verified starter stations
 *   - High-Fidelity ESP32-audioI2S pipeline with I2S output on expansion pins (BCLK=4, LRC=12, DOUT=43)
 *   - Thread-safe Core 1 Action Queue (Zero flash cache collision, zero heap fragmentation)
 * =========================================================================================================
 */

#define LGFX_USE_V1

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <esp_http_server.h>
#include <Preferences.h>
#include <Audio.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <Adafruit_NeoPixel.h>
#include <vector>
#include <time.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

#include "CST816D.h"
#include "stations_db.h"
#include "web_index.h"
#include "edifier_logo.h"

/* -------------------------------------------------------------------------
 * Hardware Pin Definitions
 * ------------------------------------------------------------------------- */
#define PIN_PWR_EN1             1    // Power rail enable 1 (MUST be driven HIGH)
#define PIN_PWR_EN2             2    // Power rail enable 2 (MUST be driven HIGH)
#define PIN_PWR_LED            40    // Green power indicator LED

#define LCD_DC_PIN              3    // GC9A01 Data/Command
#define LCD_CS_PIN              9    // GC9A01 Chip Select
#define LCD_SCLK_PIN           10    // GC9A01 SPI Clock
#define LCD_MOSI_PIN           11    // GC9A01 SPI MOSI
#define LCD_RST_PIN            14    // GC9A01 Reset
#define LCD_BL_PIN             46    // GC9A01 Backlight PWM

#define TP_SDA_PIN              6    // CST816D Touch I2C SDA
#define TP_SCL_PIN              7    // CST816D Touch I2C SCL
#define TP_INT_PIN              5    // CST816D Touch Interrupt
#define TP_RST_PIN             13    // CST816D Touch Reset

#define ENCODER_A_PIN          45    // EC3501 Rotary Phase A
#define ENCODER_B_PIN          42    // EC3501 Rotary Phase B
#define ENCODER_SW_PIN         41    // EC3501 Center Push Switch (Active LOW)

#define NEOPIXEL_PIN           48    // 5x WS2812B NeoPixel Ring
#define NEOPIXEL_COUNT          5

// External I2S Audio Output Pins (Mapped to accessible 4-pin I2C & UART ports)
// I2C Port (J36):  SCL = GPIO 39, SDA = GPIO 38, 5V, GND
// UART Port (J34): TX = GPIO 43, RX = GPIO 44, 5V, GND
#define I2S_BCLK_PIN           39    // Bit Clock -> I2C Port "SCL" pin (GPIO 39)
#define I2S_LRC_PIN            38    // Word Select (LRC / WS) -> I2C Port "SDA" pin (GPIO 38)
#define I2S_DOUT_PIN           43    // Serial Audio Data -> UART Port "TX" pin (GPIO 43)

/* -------------------------------------------------------------------------
 * Display Driver Configuration (LovyanGFX for GC9A01)
 * ------------------------------------------------------------------------- */
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_GC9A01 _panel_instance;
    lgfx::Bus_SPI      _bus_instance;

public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host   = SPI2_HOST;
            cfg.spi_mode   = 0;
            cfg.freq_write = 80000000;
            cfg.freq_read  = 20000000;
            cfg.spi_3wire  = true;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk   = LCD_SCLK_PIN;
            cfg.pin_mosi   = LCD_MOSI_PIN;
            cfg.pin_miso   = -1;
            cfg.pin_dc     = LCD_DC_PIN;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs           = LCD_CS_PIN;
            cfg.pin_rst          = LCD_RST_PIN;
            cfg.pin_busy         = -1;
            cfg.memory_width     = 240;
            cfg.memory_height    = 240;
            cfg.panel_width      = 240;
            cfg.panel_height     = 240;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            cfg.readable         = false;
            cfg.invert           = true;
            cfg.rgb_order        = false;
            cfg.bus_shared       = false;
            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);
    }
};

static LGFX gfx;
static LGFX_Sprite loaderSprite(&gfx);
static CST816D touch(TP_SDA_PIN, TP_SCL_PIN, TP_RST_PIN, TP_INT_PIN);
static Adafruit_NeoPixel strip(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
static Audio audio;
static Preferences prefs;
static DNSServer dnsServer;
static httpd_handle_t httpServer = NULL;

/* -------------------------------------------------------------------------
 * Data Models & Runtime State
 * ------------------------------------------------------------------------- */
struct RuntimeStation {
    String name;
    String state;
    String language;
    String url;
};

static std::vector<RuntimeStation> runtimeStations;
static volatile int currentStationIndex = 0;
static volatile bool isPlaying = false;
static volatile bool isBuffering = false;
static unsigned long greenConfirmationUntilMs = 0;
static unsigned long bufferingStartMs = 0;
static int cdnFailoverCount = 0;
static String activeStreamUrl = "";
static int currentVolume = 18;    // Range 0 to 21
static bool isMuted = false;
static int preMuteVolume = 18;

// Stream URL & Status Error Tracking
static bool isStreamError = false;
static String streamErrorReason = "";
static unsigned long streamErrorUntilMs = 0;

// Knob Interaction Modes
enum KnobMode {
    KNOB_MODE_TUNE,
    KNOB_MODE_VOL
};
static volatile KnobMode currentKnobMode = KNOB_MODE_TUNE;
static unsigned long lastVolModeSwitchMs = 0;
static const unsigned long VOL_MODE_TIMEOUT_MS = 4000;

// Ambient Light Modes
enum LedMode {
    LED_MODE_BREATHE = 0,
    LED_MODE_VU,
    LED_MODE_REACTIVE,
    LED_MODE_RAINBOW,
    LED_MODE_SOLID,
    LED_MODE_OFF
};
static int currentLedMode = LED_MODE_BREATHE;
static int currentLedBrightness = 30; // 0 to 100%
static volatile int ledChaseDir = 0;  // -1 = CW matching, 1 = CCW matching
static unsigned long lastChaseMs = 0;

// Auto-on Alarm & Sleep Timer State
static bool timerEnabled = false;
static int timerHour = 6;
static int timerMin = 30;
static int timerDuration = 30; // minutes (0 = continuous)
static bool alarmActivePlaying = false;
static unsigned long alarmAutoOffExpiryMs = 0;
static int lastCheckedDay = -1;
RTC_DATA_ATTR static bool bootFromAlarm = false;

// WiFi & AP State
static String wifiSsid = "";
static String wifiPass = "";
static const char* AP_SSID = "Edifier-Radio";
static const char* AP_PASS = "12345678";

/* -------------------------------------------------------------------------
 * Dedicated Core 0 Audio Task & Non-Blocking Command Queue
 * ------------------------------------------------------------------------- */
enum AudioCmdType : uint8_t {
    AUDIO_CMD_NONE = 0,
    AUDIO_CMD_CONNECT,
    AUDIO_CMD_STOP,
    AUDIO_CMD_PAUSE_RESUME,
    AUDIO_CMD_SET_VOLUME
};

struct AudioCommand {
    AudioCmdType type;
    int value;
    char url[384];
};

static QueueHandle_t audioCmdQueue = NULL;

static volatile bool audioIsRunning = false;
static volatile uint32_t audioSampleRate = 0;
static volatile uint32_t audioBitRate = 0;
static volatile uint32_t audioInBuffer = 0;
static volatile uint32_t audioCurrentTime = 0;

void sendAudioConnect(const char* url) {
    if (!audioCmdQueue || !url) return;
    audioIsRunning = false;
    audioSampleRate = 0;
    audioBitRate = 0;
    audioInBuffer = 0;
    audioCurrentTime = 0;
    AudioCommand cmd;
    cmd.type = AUDIO_CMD_CONNECT;
    cmd.value = 0;
    strncpy(cmd.url, url, sizeof(cmd.url) - 1);
    cmd.url[sizeof(cmd.url) - 1] = '\0';
    xQueueSend(audioCmdQueue, &cmd, 0);
}

void sendAudioStop() {
    if (!audioCmdQueue) return;
    audioIsRunning = false;
    audioSampleRate = 0;
    audioBitRate = 0;
    audioInBuffer = 0;
    audioCurrentTime = 0;
    AudioCommand cmd;
    cmd.type = AUDIO_CMD_STOP;
    cmd.value = 0;
    cmd.url[0] = '\0';
    xQueueSend(audioCmdQueue, &cmd, 0);
}

void sendAudioPauseResume() {
    if (!audioCmdQueue) return;
    AudioCommand cmd;
    cmd.type = AUDIO_CMD_PAUSE_RESUME;
    cmd.value = 0;
    cmd.url[0] = '\0';
    xQueueSend(audioCmdQueue, &cmd, 0);
}

void sendAudioSetVolume(int vol) {
    if (!audioCmdQueue) return;
    AudioCommand cmd;
    cmd.type = AUDIO_CMD_SET_VOLUME;
    cmd.value = vol;
    cmd.url[0] = '\0';
    xQueueSend(audioCmdQueue, &cmd, 0);
}

static void audioTask(void *pvParameters) {
    (void)pvParameters;
    AudioCommand cmd;
    while (1) {
        while (audioCmdQueue && xQueueReceive(audioCmdQueue, &cmd, 0) == pdTRUE) {
            if (cmd.type == AUDIO_CMD_CONNECT) {
                audio.stopSong();
                audioIsRunning = false;
                audioSampleRate = 0;
                audioBitRate = 0;
                audioInBuffer = 0;
                audioCurrentTime = 0;
                audio.connecttohost(cmd.url);
            } else if (cmd.type == AUDIO_CMD_STOP) {
                audio.stopSong();
                audioIsRunning = false;
                audioSampleRate = 0;
                audioBitRate = 0;
                audioInBuffer = 0;
                audioCurrentTime = 0;
            } else if (cmd.type == AUDIO_CMD_PAUSE_RESUME) {
                audio.pauseResume();
            } else if (cmd.type == AUDIO_CMD_SET_VOLUME) {
                audio.setVolume(cmd.value);
            }
        }
        audio.loop();
        audioIsRunning = audio.isRunning();
        audioSampleRate = audioIsRunning ? audio.getSampleRate() : 0;
        audioBitRate = audioIsRunning ? audio.getBitRate() : 0;
        audioInBuffer = audio.inBufferFilled();
        audioCurrentTime = audioIsRunning ? audio.getAudioCurrentTime() : 0;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/* -------------------------------------------------------------------------
 * Asynchronous Core 1 Action Queue
 * ------------------------------------------------------------------------- */
enum PendingWebAction {
    ACT_NONE,
    ACT_PLAY_PAUSE,
    ACT_PREV,
    ACT_NEXT,
    ACT_PLAY_STATION
};
static volatile PendingWebAction pendingWebAction = ACT_NONE;
static volatile int pendingStationId = -1;
static volatile int pendingVol = -1;
static volatile int pendingDeleteStationIdx = -1;
static volatile bool pendingCdnFailover = false;
static volatile bool pendingSaveFavorites = false;

struct PendingCustomStation {
    char name[64];
    char url[256];
    char state[48];
    char lang[48];
    bool playNow;
    volatile bool pending;
};
static PendingCustomStation pendingAddStation = { "", "", "", "", false, false };

struct PendingDirectPlay {
    char name[64];
    char url[256];
    char state[48];
    char lang[48];
    volatile bool pending;
};
static PendingDirectPlay pendingDirectPlay = { "", "", "", "", false };
static bool isAdHocPlaying = false;
static RuntimeStation adHocStation;

struct PendingTimerSave {
    bool en;
    int h;
    int m;
    int dur;
    volatile bool pending;
};
static PendingTimerSave pendingTimerSave = { false, 6, 0, 30, false };

struct PendingWifiSave {
    char ssid[64];
    char pass[64];
    volatile bool pending;
};
static PendingWifiSave pendingWifiSave = { "", "", false };

/* -------------------------------------------------------------------------
 * Application Screens & Settings State
 * ------------------------------------------------------------------------- */
enum AppScreen {
    SCREEN_HOME_LAUNCHER = 0,
    SCREEN_RADIO,
    SCREEN_CLOCK,
    SCREEN_SETTINGS,
    SCREEN_WIFI_SCAN,
    SCREEN_WIFI_KEYPAD
};
static AppScreen currentScreen = SCREEN_HOME_LAUNCHER;
static int selectedAppIndex = 0; // 0: Radio, 1: Clock (Analogue), 2: Clock (Digital), 3: Settings
static volatile bool hasPendingScreenSwitch = false;
static volatile AppScreen pendingScreenSwitch = SCREEN_HOME_LAUNCHER;

static bool isScreensaverActive = false;
static unsigned long lastActivityMs = 0;
static int screensaverTimeoutSec = 60; // 0 = Off, 30s, 60s, 120s, 300s
static int currentClockFace = 0;       // 0: Luxury Analog, 1: Chrono Minimalist, 2: Cyber Digital
static int currentBacklightBrightness = 70; // 10% to 100%
static int selectedSettingRow = 0;
static int selectedWifiRow = 0;
static std::vector<String> scannedSsids;
static String selectedSsid = "";

// Custom Rotary Character Keypad State
enum KeypadAction {
    KP_CHAR = 0,
    KP_SPACE,
    KP_DEL,
    KP_CONNECT,
    KP_CANCEL
};

struct KeypadEntry {
    KeypadAction action;
    char ch;
    char display[12];
};
static KeypadEntry kpEntries[84];
static int totalKpEntries = 0;
static int kpIndex = 0;
static String kpEnteredPass = "";

/* -------------------------------------------------------------------------
 * LVGL Multi-Screen UI Elements (240x240 Circular Display)
 * ------------------------------------------------------------------------- */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* disp_buf1 = nullptr;
static lv_color_t* disp_buf2 = nullptr;

// 0. Home Launcher Screen Objects (Main Hub Carousel: Radio, Clock Analogue, Clock Digital, Settings)
static lv_obj_t* scrHomeLauncher = nullptr;
static lv_obj_t* btnHomePrev = nullptr;
static lv_obj_t* btnHomeNext = nullptr;
static lv_obj_t* btnAppCard = nullptr;
static lv_obj_t* lblIconCard = nullptr;
static lv_obj_t* dotAppIndicators[4] = { nullptr, nullptr, nullptr, nullptr };
static lv_obj_t* lblAppTitle = nullptr;
static lv_obj_t* lblAppSub = nullptr;
static lv_obj_t* lblHomeAudio = nullptr;

// 1. Radio Screen Objects
static lv_obj_t* scrRadio = nullptr;
static lv_obj_t* arcRing = nullptr;
static lv_obj_t* lblBadgeTop = nullptr;
static lv_obj_t* btnRadioHome = nullptr;
static lv_obj_t* btnRadioSettings = nullptr;
static lv_obj_t* lblStationName = nullptr;
static lv_obj_t* lblMeta = nullptr;
static lv_obj_t* lblVolBadge = nullptr;
static lv_obj_t* btnPrev = nullptr;
static lv_obj_t* btnPlayPause = nullptr;
static lv_obj_t* lblPlayPauseIcon = nullptr;
static lv_obj_t* btnNext = nullptr;
static lv_obj_t* lblIpHint = nullptr;

// 2. Clock Screen Objects (Watch Faces & Screensaver)
static lv_obj_t* scrClock = nullptr;
static lv_obj_t* btnClockHome = nullptr;
static lv_obj_t* btnClockFaceToggle = nullptr;

// Analog Dial (Luxury & Chrono)
static lv_obj_t* contAnalogDial = nullptr;
static lv_obj_t* contChronoDial = nullptr;
static lv_obj_t* lineHour = nullptr;
static lv_obj_t* lineMin = nullptr;
static lv_obj_t* lineSec = nullptr;
static lv_obj_t* objClockHub = nullptr;
static lv_obj_t* lblAnalogDate = nullptr;
static lv_obj_t* lblAnalogStation = nullptr;
static lv_point_t p_hour[2] = {{120, 120}, {120, 78}};
static lv_point_t p_min[2]  = {{120, 120}, {120, 55}};
static lv_point_t p_sec[2]  = {{120, 120}, {120, 42}};

// Digital Dial (Cyber Modern)
static lv_obj_t* contDigital = nullptr;
static lv_obj_t* lblDigitalTime = nullptr;
static lv_obj_t* lblDigitalSec = nullptr;
static lv_obj_t* lblDigitalDate = nullptr;
static lv_obj_t* lblDigitalAudio = nullptr;
static lv_obj_t* lblDigitalWifi = nullptr;

// 3. Settings Screen Objects
static lv_obj_t* scrSettings = nullptr;
static lv_obj_t* btnSettingsHome = nullptr;
static lv_obj_t* contSettingsList = nullptr;
static lv_obj_t* btnOptWifi = nullptr;
static lv_obj_t* lblOptWifi = nullptr;
static lv_obj_t* btnOptSaver = nullptr;
static lv_obj_t* lblOptSaver = nullptr;
static lv_obj_t* btnOptFace = nullptr;
static lv_obj_t* lblOptFace = nullptr;
static lv_obj_t* btnOptBright = nullptr;
static lv_obj_t* lblOptBright = nullptr;
static lv_obj_t* btnOptLed = nullptr;
static lv_obj_t* lblOptLed = nullptr;
static lv_obj_t* btnOptReset = nullptr;
static lv_obj_t* lblOptReset = nullptr;
static lv_obj_t* btnOptHome = nullptr;
static lv_obj_t* lblOptHome = nullptr;

// 4. Wi-Fi Scan Screen Objects
static lv_obj_t* scrWifiScan = nullptr;
static lv_obj_t* btnScanBack = nullptr;
static lv_obj_t* btnScanRescan = nullptr;
static lv_obj_t* lblScanStatus = nullptr;
static lv_obj_t* listWifi = nullptr;

// 5. Wi-Fi Rotary Keypad Objects
static lv_obj_t* scrWifiKeypad = nullptr;
static lv_obj_t* lblKeypadTitle = nullptr;
static lv_obj_t* lblKeypadPass = nullptr;
static lv_obj_t* boxKeypadCenter = nullptr;
static lv_obj_t* lblKeypadCurChar = nullptr;
static lv_obj_t* lblKeypadPrev2 = nullptr;
static lv_obj_t* lblKeypadPrev1 = nullptr;
static lv_obj_t* lblKeypadNext1 = nullptr;
static lv_obj_t* lblKeypadNext2 = nullptr;
static lv_obj_t* btnKpDel = nullptr;
static lv_obj_t* btnKpSpace = nullptr;
static lv_obj_t* btnKpConnect = nullptr;
static lv_obj_t* btnKpCancel = nullptr;

/* -------------------------------------------------------------------------
 * Forward Declarations
 * ------------------------------------------------------------------------- */
void playCurrentStation();
void playDirectStream(const char* name, const char* url, const char* state, const char* lang);
void triggerCdnFailover();
void nextStation();
void prevStation();
void setVolume(int v);
void toggleMute();
void togglePlayPause();
void updateUI();
void updateRadioUI();
void updateClockUI();
void updateSettingsUI();
void switchScreen(AppScreen s);
void showWatchFace(int face);
void cycleClockFace();
void cycleScreensaverTimeout();
void cycleBrightness();
void cycleLedMode();
void highlightSettingsRow(int idx);
void highlightWifiRow(int idx);
void startWifiScan();
void initKeypadEntries();
void updateKeypadUI();
void updateKeypadPasswordLabel();
void keypadSelectCurrent();
void connectWifiWithKeypad();
void saveFavorites();
void loadFavorites();
void setStarterFavorites();
void addFavorite(const RuntimeStation& st, bool playNow);
void deleteFavorite(int idx);
void setupWebServer();
void updateAmbientLeds();
void updateHomeLauncherUI();
void homeLaunchSelectedApp();
void saveLastStationIndex(int idx);
void settingsSelectCurrent();
void renderProfessionalLoaderFrame(int progress, const char* titleMsg, const char* detailMsg);
void createPowerOffOverlay();
void showPowerOffOverlay(unsigned long heldMs);
void hidePowerOffOverlay();
void enterPowerOffMode();
void runStandbySleepLoop(bool isColdBoot = false);
void loadTimerSettings();
void saveTimerSettings();

/* -------------------------------------------------------------------------
 * LVGL Display Flush Callback
 * ------------------------------------------------------------------------- */
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);
    gfx.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
    gfx.endWrite();
    lv_disp_flush_ready(disp);
}

/* -------------------------------------------------------------------------
 * LVGL High-Resolution Hardware Tick Callback (esp_timer)
 * ------------------------------------------------------------------------- */
static void lv_tick_task(void* arg) {
    lv_tick_inc(5);
}

/* -------------------------------------------------------------------------
 * LVGL Touchpad Read Callback (CST816D with Activity Reset & Gestures)
 * ------------------------------------------------------------------------- */
static void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    uint16_t touchX = 0, touchY = 0;
    uint8_t gesture = 0;
    bool touched = touch.getTouch(&touchX, &touchY, &gesture);
    if (touched) {
        lastActivityMs = millis();
        // Touch on screensaver wakes immediately back to Home
        if (isScreensaverActive && currentScreen == SCREEN_CLOCK) {
            isScreensaverActive = false;
            pendingScreenSwitch = SCREEN_HOME_LAUNCHER;
            hasPendingScreenSwitch = true;
        }

        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;

        static unsigned long lastGestureTime = 0;
        unsigned long now = millis();
        if (gesture == SlideLeft && (now - lastGestureTime > 500)) {
            lastGestureTime = now;
            if (currentScreen == SCREEN_RADIO) {
                pendingWebAction = ACT_NEXT;
            }
        } else if (gesture == SlideRight && (now - lastGestureTime > 500)) {
            lastGestureTime = now;
            if (currentScreen == SCREEN_RADIO) {
                pendingWebAction = ACT_PREV;
            }
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

/* -------------------------------------------------------------------------
 * Hardware Rotary Encoder & Switch Architecture (FreeRTOS Core 0 Task + Queue)
 * ------------------------------------------------------------------------- */
enum EncoderActionType : int8_t {
    ENCODER_ROTATE_CW = 1,
    ENCODER_ROTATE_CCW = 2,
    ENCODER_CLICK = 3
};

struct EncoderAction {
    EncoderActionType type;
};

static QueueHandle_t encoderActionQueue = NULL;

// Dial button is monitored statefully in the main loop to distinguish between short clicks and 4-second power holds

static void encTask(void *pvParameters) {
    (void)pvParameters;
    int lastStateCLK = digitalRead(ENCODER_A_PIN);

    while (1) {
        int currentStateCLK = digitalRead(ENCODER_A_PIN);

        // Detect clean rising edge of Phase A (one event per physical detent)
        if (currentStateCLK != lastStateCLK && currentStateCLK == HIGH && encoderActionQueue) {
            EncoderAction action;
            action.type = (digitalRead(ENCODER_B_PIN) != currentStateCLK)
                              ? ENCODER_ROTATE_CW
                              : ENCODER_ROTATE_CCW;
            xQueueSend(encoderActionQueue, &action, 0);
        }
        lastStateCLK = currentStateCLK;

        vTaskDelay(pdMS_TO_TICKS(2) > 0 ? pdMS_TO_TICKS(2) : 1);
    }
}

/* -------------------------------------------------------------------------
 * Next & Previous Station Functions
 * ------------------------------------------------------------------------- */
void nextStation() {
    if (runtimeStations.empty()) return;
    currentStationIndex = (currentStationIndex + 1) % (int)runtimeStations.size();
    playCurrentStation();
    updateUI();
}

void prevStation() {
    if (runtimeStations.empty()) return;
    currentStationIndex = (currentStationIndex - 1 + (int)runtimeStations.size()) % (int)runtimeStations.size();
    playCurrentStation();
    updateUI();
}

/* -------------------------------------------------------------------------
 * Dedicated Play/Pause Toggle
 * ------------------------------------------------------------------------- */
void togglePlayPause() {
    isStreamError = false;
    streamErrorReason = "";
    streamErrorUntilMs = 0;
    if (isPlaying) {
        sendAudioPauseResume();
        isPlaying = false;
        isBuffering = false;
        alarmActivePlaying = false;
    } else {
        if (audioIsRunning) {
            sendAudioPauseResume();
            isPlaying = true;
        } else {
            playCurrentStation();
        }
    }
    updateUI();
}

/* -------------------------------------------------------------------------
 * Clock Timer Callback (1000ms: NTP Hands & Digital Time Update)
 * ------------------------------------------------------------------------- */
static void clockTimerCb(lv_timer_t* timer) {
    (void)timer;
    if (currentScreen != SCREEN_CLOCK) return; // Prevent unnecessary layout/math when not on clock screen

    time_t nowSec = time(nullptr);
    struct tm timeinfo;
    if (nowSec > 100000) {
        localtime_r(&nowSec, &timeinfo);
    } else {
        // Fallback local running time
        unsigned long secSinceBoot = millis() / 1000;
        timeinfo.tm_hour = (12 + (secSinceBoot / 3600)) % 24;
        timeinfo.tm_min  = (30 + (secSinceBoot / 60)) % 60;
        timeinfo.tm_sec  = secSinceBoot % 60;
        timeinfo.tm_wday = 1; // Mon
        timeinfo.tm_mday = 21;
        timeinfo.tm_mon  = 8; // Sep (0-indexed)
        timeinfo.tm_year = 126; // 2026
    }

    // 1. Update Analog Hands (Only when an analog face is active)
    if (currentClockFace != 2) {
        float secDeg  = timeinfo.tm_sec * 6.0f;
        float minDeg  = timeinfo.tm_min * 6.0f + timeinfo.tm_sec * 0.1f;
        float hourDeg = (timeinfo.tm_hour % 12) * 30.0f + timeinfo.tm_min * 0.5f;

        float rH = hourDeg * 0.0174532925f;
        float rM = minDeg  * 0.0174532925f;
        float rS = secDeg  * 0.0174532925f;

        p_hour[0].x = (lv_coord_t)(120 - 8 * sinf(rH));
        p_hour[0].y = (lv_coord_t)(120 + 8 * cosf(rH));
        p_hour[1].x = (lv_coord_t)(120 + 42 * sinf(rH));
        p_hour[1].y = (lv_coord_t)(120 - 42 * cosf(rH));

        p_min[0].x = (lv_coord_t)(120 - 10 * sinf(rM));
        p_min[0].y = (lv_coord_t)(120 + 10 * cosf(rM));
        p_min[1].x = (lv_coord_t)(120 + 65 * sinf(rM));
        p_min[1].y = (lv_coord_t)(120 - 65 * cosf(rM));

        p_sec[0].x = (lv_coord_t)(120 - 16 * sinf(rS));
        p_sec[0].y = (lv_coord_t)(120 + 16 * cosf(rS));
        p_sec[1].x = (lv_coord_t)(120 + 78 * sinf(rS));
        p_sec[1].y = (lv_coord_t)(120 - 78 * cosf(rS));

        if (lineHour) lv_line_set_points(lineHour, p_hour, 2);
        if (lineMin)  lv_line_set_points(lineMin, p_min, 2);
        if (lineSec)  lv_line_set_points(lineSec, p_sec, 2);
    }

    // 2. Update Analog labels
    if (lblAnalogDate) {
        const char* DAYS_SHORT[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        const char* MONTHS_SHORT[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        char dBuf[24];
        snprintf(dBuf, sizeof(dBuf), "%s %02d %s", DAYS_SHORT[timeinfo.tm_wday % 7], timeinfo.tm_mday, MONTHS_SHORT[timeinfo.tm_mon % 12]);
        lv_label_set_text(lblAnalogDate, dBuf);
    }
    if (lblAnalogStation) {
        if (isPlaying && !runtimeStations.empty()) {
            char stBuf[36];
            snprintf(stBuf, sizeof(stBuf), "▶ %s", runtimeStations[currentStationIndex].name.c_str());
            lv_label_set_text(lblAnalogStation, stBuf);
            lv_obj_set_style_text_color(lblAnalogStation, lv_color_hex(0x2ea043), 0);
        } else {
            lv_label_set_text(lblAnalogStation, "⏸ PAUSED");
            lv_obj_set_style_text_color(lblAnalogStation, lv_color_hex(0x8b949e), 0);
        }
    }

    // 3. Update Digital labels
    if (lblDigitalTime) {
        char tBuf[16];
        snprintf(tBuf, sizeof(tBuf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        lv_label_set_text(lblDigitalTime, tBuf);
    }
    if (lblDigitalSec) {
        char sBuf[8];
        snprintf(sBuf, sizeof(sBuf), ":%02d", timeinfo.tm_sec);
        lv_label_set_text(lblDigitalSec, sBuf);
    }
    if (lblDigitalDate) {
        const char* DAYS[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        const char* MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        char dBuf[32];
        snprintf(dBuf, sizeof(dBuf), "%s, %s %02d", DAYS[timeinfo.tm_wday % 7], MONTHS[timeinfo.tm_mon % 12], timeinfo.tm_mday);
        lv_label_set_text(lblDigitalDate, dBuf);
    }
    if (lblDigitalAudio) {
        if (isPlaying && !runtimeStations.empty()) {
            char aBuf[48];
            snprintf(aBuf, sizeof(aBuf), "♫ CH %02d • %s", currentStationIndex + 1, runtimeStations[currentStationIndex].name.c_str());
            lv_label_set_text(lblDigitalAudio, aBuf);
            lv_obj_set_style_text_color(lblDigitalAudio, lv_color_hex(0x2ea043), 0);
        } else {
            lv_label_set_text(lblDigitalAudio, "PAUSED");
            lv_obj_set_style_text_color(lblDigitalAudio, lv_color_hex(0x8b949e), 0);
        }
    }
    if (lblDigitalWifi) {
        if (WiFi.status() == WL_CONNECTED) {
            char wBuf[32];
            snprintf(wBuf, sizeof(wBuf), "%s", WiFi.localIP().toString().c_str());
            lv_label_set_text(lblDigitalWifi, wBuf);
        } else {
            lv_label_set_text(lblDigitalWifi, "192.168.4.1 (AP)");
        }
    }
}

/* -------------------------------------------------------------------------
 * Watch Face Visibility Controller
 * ------------------------------------------------------------------------- */
void showWatchFace(int face) {
    if (!scrClock) return;
    if (face == 0) { // Luxury Analog
        if (contAnalogDial) lv_obj_clear_flag(contAnalogDial, LV_OBJ_FLAG_HIDDEN);
        if (contChronoDial) lv_obj_add_flag(contChronoDial, LV_OBJ_FLAG_HIDDEN);
        if (contDigital)    lv_obj_add_flag(contDigital, LV_OBJ_FLAG_HIDDEN);
        if (lineHour) lv_obj_clear_flag(lineHour, LV_OBJ_FLAG_HIDDEN);
        if (lineMin)  lv_obj_clear_flag(lineMin, LV_OBJ_FLAG_HIDDEN);
        if (lineSec)  lv_obj_clear_flag(lineSec, LV_OBJ_FLAG_HIDDEN);
        if (objClockHub) lv_obj_clear_flag(objClockHub, LV_OBJ_FLAG_HIDDEN);
        if (lblAnalogDate) lv_obj_clear_flag(lblAnalogDate, LV_OBJ_FLAG_HIDDEN);
        if (lblAnalogStation) lv_obj_clear_flag(lblAnalogStation, LV_OBJ_FLAG_HIDDEN);
    } else if (face == 1) { // Chrono Minimalist
        if (contAnalogDial) lv_obj_add_flag(contAnalogDial, LV_OBJ_FLAG_HIDDEN);
        if (contChronoDial) lv_obj_clear_flag(contChronoDial, LV_OBJ_FLAG_HIDDEN);
        if (contDigital)    lv_obj_add_flag(contDigital, LV_OBJ_FLAG_HIDDEN);
        if (lineHour) lv_obj_clear_flag(lineHour, LV_OBJ_FLAG_HIDDEN);
        if (lineMin)  lv_obj_clear_flag(lineMin, LV_OBJ_FLAG_HIDDEN);
        if (lineSec)  lv_obj_clear_flag(lineSec, LV_OBJ_FLAG_HIDDEN);
        if (objClockHub) lv_obj_clear_flag(objClockHub, LV_OBJ_FLAG_HIDDEN);
        if (lblAnalogDate) lv_obj_add_flag(lblAnalogDate, LV_OBJ_FLAG_HIDDEN);
        if (lblAnalogStation) lv_obj_clear_flag(lblAnalogStation, LV_OBJ_FLAG_HIDDEN);
    } else { // Cyber Digital
        if (contAnalogDial) lv_obj_add_flag(contAnalogDial, LV_OBJ_FLAG_HIDDEN);
        if (contChronoDial) lv_obj_add_flag(contChronoDial, LV_OBJ_FLAG_HIDDEN);
        if (contDigital)    lv_obj_clear_flag(contDigital, LV_OBJ_FLAG_HIDDEN);
        if (lineHour) lv_obj_add_flag(lineHour, LV_OBJ_FLAG_HIDDEN);
        if (lineMin)  lv_obj_add_flag(lineMin, LV_OBJ_FLAG_HIDDEN);
        if (lineSec)  lv_obj_add_flag(lineSec, LV_OBJ_FLAG_HIDDEN);
        if (objClockHub) lv_obj_add_flag(objClockHub, LV_OBJ_FLAG_HIDDEN);
        if (lblAnalogDate) lv_obj_add_flag(lblAnalogDate, LV_OBJ_FLAG_HIDDEN);
        if (lblAnalogStation) lv_obj_add_flag(lblAnalogStation, LV_OBJ_FLAG_HIDDEN);
    }
}

void cycleClockFace() {
    if (currentScreen == SCREEN_CLOCK) {
        if (currentClockFace == 0) currentClockFace = 1;
        else if (currentClockFace == 1) currentClockFace = 0;
        else currentClockFace = 0;
    } else {
        currentClockFace = (currentClockFace + 1) % 3;
    }
    showWatchFace(currentClockFace);
    prefs.begin("crow_settings", false);
    prefs.putInt("clock_face", currentClockFace);
    prefs.end();
    updateSettingsUI();
    Serial.printf("[SETTINGS] Clock face set to %d\n", currentClockFace);
}

void cycleScreensaverTimeout() {
    if (screensaverTimeoutSec == 0) screensaverTimeoutSec = 30;
    else if (screensaverTimeoutSec == 30) screensaverTimeoutSec = 60;
    else if (screensaverTimeoutSec == 60) screensaverTimeoutSec = 120;
    else if (screensaverTimeoutSec == 120) screensaverTimeoutSec = 300;
    else screensaverTimeoutSec = 0;

    prefs.begin("crow_settings", false);
    prefs.putInt("screensaver_sec", screensaverTimeoutSec);
    prefs.end();
    updateSettingsUI();
    Serial.printf("[SETTINGS] Screensaver timeout set to %ds\n", screensaverTimeoutSec);
}

void cycleBrightness() {
    currentBacklightBrightness += 20;
    if (currentBacklightBrightness > 100) currentBacklightBrightness = 20;
    ledcWrite(LCD_BL_PIN, (currentBacklightBrightness * 255) / 100);
    prefs.begin("crow_settings", false);
    prefs.putInt("bl_bright", currentBacklightBrightness);
    prefs.end();
    updateSettingsUI();
    Serial.printf("[SETTINGS] Backlight set to %d%%\n", currentBacklightBrightness);
}

void cycleLedMode() {
    currentLedMode = (currentLedMode + 1) % 6;
    prefs.begin("crow_settings", false);
    prefs.putInt("led_mode", currentLedMode);
    prefs.end();
    updateSettingsUI();
    Serial.printf("[SETTINGS] LED mode set to %d\n", currentLedMode);
}

void highlightSettingsRow(int idx) {
    if (!contSettingsList) return;
    uint32_t cnt = lv_obj_get_child_cnt(contSettingsList);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(contSettingsList, i);
        if ((int)i == idx) {
            lv_obj_set_style_border_color(child, lv_color_hex(0x1f6feb), 0);
            lv_obj_set_style_border_width(child, 2, 0);
            lv_obj_scroll_to_view(child, LV_ANIM_OFF);
        } else {
            lv_obj_set_style_border_color(child, lv_color_hex(0x30363d), 0);
            lv_obj_set_style_border_width(child, 1, 0);
        }
    }
}

void settingsSelectCurrent() {
    if (selectedSettingRow == 0) switchScreen(SCREEN_WIFI_SCAN);
    else if (selectedSettingRow == 1) cycleScreensaverTimeout();
    else if (selectedSettingRow == 2) cycleClockFace();
    else if (selectedSettingRow == 3) cycleBrightness();
    else if (selectedSettingRow == 4) cycleLedMode();
    else if (selectedSettingRow == 5) {
        setStarterFavorites();
        switchScreen(SCREEN_RADIO);
    } else if (selectedSettingRow == 6) {
        switchScreen(SCREEN_HOME_LAUNCHER);
    }
}

void updateSettingsUI() {
    if (!scrSettings) return;

    if (lblOptWifi) {
        String w = "📶 Wi-Fi: ";
        if (WiFi.status() == WL_CONNECTED) w += WiFi.localIP().toString();
        else w += "Scan Networks";
        lv_label_set_text(lblOptWifi, w.c_str());
    }

    if (lblOptSaver) {
        String s = "⏰ Screensaver: ";
        if (screensaverTimeoutSec == 0) s += "Off";
        else s += String(screensaverTimeoutSec) + "s";
        lv_label_set_text(lblOptSaver, s.c_str());
    }

    if (lblOptFace) {
        String f = "⌚ Face: ";
        if (currentClockFace == 0) f += "Luxury Analog";
        else if (currentClockFace == 1) f += "Chrono Minimal";
        else f += "Cyber Digital";
        lv_label_set_text(lblOptFace, f.c_str());
    }

    if (lblOptBright) {
        String b = "☀️ Backlight: " + String(currentBacklightBrightness) + "%";
        lv_label_set_text(lblOptBright, b.c_str());
    }

    if (lblOptLed) {
        const char* ledNames[] = { "Breathe", "VU Meter", "Reactive", "Rainbow", "Solid", "Off" };
        String l = "💡 LED: ";
        l += ledNames[currentLedMode % 6];
        lv_label_set_text(lblOptLed, l.c_str());
    }
}

/* -------------------------------------------------------------------------
 * Wi-Fi Scanner Screen
 * ------------------------------------------------------------------------- */
void highlightWifiRow(int idx) {
    if (!listWifi || scannedSsids.empty()) return;
    uint32_t cnt = lv_obj_get_child_cnt(listWifi);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(listWifi, i);
        if ((int)i == idx) {
            lv_obj_set_style_border_color(child, lv_color_hex(0x1f6feb), 0);
            lv_obj_set_style_border_width(child, 2, 0);
            lv_obj_scroll_to_view(child, LV_ANIM_OFF);
        } else {
            lv_obj_set_style_border_color(child, lv_color_hex(0x30363d), 0);
            lv_obj_set_style_border_width(child, 1, 0);
        }
    }
}

void startWifiScan() {
    if (!listWifi || !lblScanStatus) return;
    lv_obj_clean(listWifi);
    lv_label_set_text(lblScanStatus, "Scanning 2.4GHz Wi-Fi...");
    lv_obj_clear_flag(lblScanStatus, LV_OBJ_FLAG_HIDDEN);
    lv_timer_handler();

    int n = WiFi.scanNetworks(false, false);
    scannedSsids.clear();
    selectedWifiRow = 0;

    if (n <= 0) {
        lv_label_set_text(lblScanStatus, "No networks found.\nTap Rescan.");
        return;
    }

    lv_obj_add_flag(lblScanStatus, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < n && scannedSsids.size() < 12; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;
        bool dup = false;
        for (const auto& s : scannedSsids) {
            if (s.equals(ssid)) { dup = true; break; }
        }
        if (dup) continue;

        scannedSsids.push_back(ssid);

        lv_obj_t* btn = lv_btn_create(listWifi);
        lv_obj_set_width(btn, 154);
        lv_obj_set_height(btn, 38);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x161b22), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x30363d), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_ext_click_area(btn, 4);
        lv_obj_set_user_data(btn, (void*)(intptr_t)(scannedSsids.size() - 1));

        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
                int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
                if (idx >= 0 && idx < (int)scannedSsids.size()) {
                    selectedSsid = scannedSsids[idx];
                    kpEnteredPass = "";
                    kpIndex = 0;
                    switchScreen(SCREEN_WIFI_KEYPAD);
                }
            }
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* lbl = lv_label_create(btn);
        char rowBuf[48];
        const char* lock = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) ? "🔒 " : "";
        snprintf(rowBuf, sizeof(rowBuf), "%s%s", lock, ssid.c_str());
        lv_label_set_text(lbl, rowBuf);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xf0f6fc), 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 6, 0);
    }
    WiFi.scanDelete();
    highlightWifiRow(selectedWifiRow);
}

/* -------------------------------------------------------------------------
 * Custom Rotary Keypad Engine (Turn: Char, Click: Select)
 * ------------------------------------------------------------------------- */
void initKeypadEntries() {
    totalKpEntries = 0;
    // Lowercase a-z
    for (char c = 'a'; c <= 'z'; c++) {
        kpEntries[totalKpEntries].action = KP_CHAR;
        kpEntries[totalKpEntries].ch = c;
        snprintf(kpEntries[totalKpEntries].display, sizeof(kpEntries[totalKpEntries].display), "%c", c);
        totalKpEntries++;
    }
    // Uppercase A-Z
    for (char c = 'A'; c <= 'Z'; c++) {
        kpEntries[totalKpEntries].action = KP_CHAR;
        kpEntries[totalKpEntries].ch = c;
        snprintf(kpEntries[totalKpEntries].display, sizeof(kpEntries[totalKpEntries].display), "%c", c);
        totalKpEntries++;
    }
    // Digits 0-9
    for (char c = '0'; c <= '9'; c++) {
        kpEntries[totalKpEntries].action = KP_CHAR;
        kpEntries[totalKpEntries].ch = c;
        snprintf(kpEntries[totalKpEntries].display, sizeof(kpEntries[totalKpEntries].display), "%c", c);
        totalKpEntries++;
    }
    // Symbols
    const char* sym = "@._-!#$%&*+=:;/?";
    for (size_t i = 0; i < strlen(sym); i++) {
        kpEntries[totalKpEntries].action = KP_CHAR;
        kpEntries[totalKpEntries].ch = sym[i];
        snprintf(kpEntries[totalKpEntries].display, sizeof(kpEntries[totalKpEntries].display), "%c", sym[i]);
        totalKpEntries++;
    }
    // Actions
    kpEntries[totalKpEntries].action = KP_SPACE;
    kpEntries[totalKpEntries].ch = ' ';
    snprintf(kpEntries[totalKpEntries].display, sizeof(kpEntries[totalKpEntries].display), "SPACE");
    totalKpEntries++;

    kpEntries[totalKpEntries].action = KP_DEL;
    kpEntries[totalKpEntries].ch = 0;
    snprintf(kpEntries[totalKpEntries].display, sizeof(kpEntries[totalKpEntries].display), "DEL");
    totalKpEntries++;

    kpEntries[totalKpEntries].action = KP_CONNECT;
    kpEntries[totalKpEntries].ch = 0;
    snprintf(kpEntries[totalKpEntries].display, sizeof(kpEntries[totalKpEntries].display), "DONE");
    totalKpEntries++;

    kpEntries[totalKpEntries].action = KP_CANCEL;
    kpEntries[totalKpEntries].ch = 0;
    snprintf(kpEntries[totalKpEntries].display, sizeof(kpEntries[totalKpEntries].display), "EXIT");
    totalKpEntries++;
}

void updateKeypadPasswordLabel() {
    if (!lblKeypadPass) return;
    String disp = kpEnteredPass;
    disp += "_";
    lv_label_set_text(lblKeypadPass, disp.c_str());
}

void updateKeypadUI() {
    if (!scrWifiKeypad || totalKpEntries == 0) return;
    int cur = kpIndex;
    int p2 = (cur - 2 + totalKpEntries) % totalKpEntries;
    int p1 = (cur - 1 + totalKpEntries) % totalKpEntries;
    int n1 = (cur + 1) % totalKpEntries;
    int n2 = (cur + 2) % totalKpEntries;

    if (lblKeypadPrev2) lv_label_set_text(lblKeypadPrev2, kpEntries[p2].display);
    if (lblKeypadPrev1) lv_label_set_text(lblKeypadPrev1, kpEntries[p1].display);
    if (lblKeypadCurChar) {
        lv_label_set_text(lblKeypadCurChar, kpEntries[cur].display);
        if (kpEntries[cur].action == KP_CONNECT) {
            lv_obj_set_style_text_color(lblKeypadCurChar, lv_color_hex(0x2ea043), 0);
            lv_obj_set_style_border_color(boxKeypadCenter, lv_color_hex(0x2ea043), 0);
        } else if (kpEntries[cur].action == KP_CANCEL) {
            lv_obj_set_style_text_color(lblKeypadCurChar, lv_color_hex(0xda3633), 0);
            lv_obj_set_style_border_color(boxKeypadCenter, lv_color_hex(0xda3633), 0);
        } else if (kpEntries[cur].action == KP_DEL) {
            lv_obj_set_style_text_color(lblKeypadCurChar, lv_color_hex(0xd29922), 0);
            lv_obj_set_style_border_color(boxKeypadCenter, lv_color_hex(0xd29922), 0);
        } else {
            lv_obj_set_style_text_color(lblKeypadCurChar, lv_color_hex(0x58a6ff), 0);
            lv_obj_set_style_border_color(boxKeypadCenter, lv_color_hex(0x1f6feb), 0);
        }
    }
    if (lblKeypadNext1) lv_label_set_text(lblKeypadNext1, kpEntries[n1].display);
    if (lblKeypadNext2) lv_label_set_text(lblKeypadNext2, kpEntries[n2].display);
}

void keypadSelectCurrent() {
    if (kpIndex < 0 || kpIndex >= totalKpEntries) return;
    KeypadEntry& ent = kpEntries[kpIndex];
    if (ent.action == KP_CHAR || ent.action == KP_SPACE) {
        if (kpEnteredPass.length() < 60) {
            kpEnteredPass += ent.ch;
        }
    } else if (ent.action == KP_DEL) {
        if (kpEnteredPass.length() > 0) {
            kpEnteredPass.remove(kpEnteredPass.length() - 1);
        }
    } else if (ent.action == KP_CONNECT) {
        connectWifiWithKeypad();
        return;
    } else if (ent.action == KP_CANCEL) {
        switchScreen(SCREEN_SETTINGS);
        return;
    }
    updateKeypadPasswordLabel();
}

void connectWifiWithKeypad() {
    Serial.printf("[WIFI] Keypad connecting to '%s' with pass '%s'\n", selectedSsid.c_str(), kpEnteredPass.c_str());
    prefs.begin("crow_wifi", false);
    prefs.putString("ssid", selectedSsid);
    prefs.putString("pass", kpEnteredPass);
    prefs.end();
    WiFi.begin(selectedSsid.c_str(), kpEnteredPass.c_str());
    switchScreen(SCREEN_RADIO);
}

/* -------------------------------------------------------------------------
 * Switch Active Screen (Zero audio glitching, smooth transition)
 * ------------------------------------------------------------------------- */
void switchScreen(AppScreen s) {
    currentScreen = s;
    lastActivityMs = millis();

    if (s == SCREEN_HOME_LAUNCHER) {
        lv_scr_load(scrHomeLauncher);
        updateHomeLauncherUI();
    } else if (s == SCREEN_RADIO) {
        lv_scr_load(scrRadio);
        updateRadioUI();
    } else if (s == SCREEN_CLOCK) {
        lv_scr_load(scrClock);
        showWatchFace(currentClockFace);
        updateClockUI();
    } else if (s == SCREEN_SETTINGS) {
        lv_scr_load(scrSettings);
        updateSettingsUI();
        highlightSettingsRow(selectedSettingRow);
    } else if (s == SCREEN_WIFI_SCAN) {
        lv_scr_load(scrWifiScan);
        startWifiScan();
    } else if (s == SCREEN_WIFI_KEYPAD) {
        lv_scr_load(scrWifiKeypad);
        if (lblKeypadTitle) {
            String t = "WiFi: " + selectedSsid;
            lv_label_set_text(lblKeypadTitle, t.c_str());
        }
        updateKeypadPasswordLabel();
        updateKeypadUI();
    }
}

/* -------------------------------------------------------------------------
 * Home Screen Navigation & Persistence Helpers
 * ------------------------------------------------------------------------- */
void saveLastStationIndex(int idx) {
    if (idx < 0) return;
    prefs.begin("crow_settings", false);
    prefs.putInt("last_station", idx);
    prefs.end();
}

void homeLaunchSelectedApp() {
    if (selectedAppIndex == 0) {
        switchScreen(SCREEN_RADIO);
    } else if (selectedAppIndex == 1) {
        if (currentClockFace == 2) currentClockFace = 0;
        showWatchFace(currentClockFace);
        switchScreen(SCREEN_CLOCK);
    } else if (selectedAppIndex == 2) {
        currentClockFace = 2;
        showWatchFace(currentClockFace);
        switchScreen(SCREEN_CLOCK);
    } else if (selectedAppIndex == 3) {
        switchScreen(SCREEN_SETTINGS);
    }
}

/* -------------------------------------------------------------------------
 * Create Multi-Screen Circular UI (240x240)
 * ------------------------------------------------------------------------- */
void createCircularUI() {
    // ---------------------------------------------------------------------
    // 0. SCREEN_HOME_LAUNCHER (Carousel with Left/Right icons & 4 apps)
    // ---------------------------------------------------------------------
    scrHomeLauncher = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scrHomeLauncher, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(scrHomeLauncher, LV_OBJ_FLAG_SCROLLABLE);

    // Left Arrow Button (⮜)
    btnHomePrev = lv_btn_create(scrHomeLauncher);
    lv_obj_set_size(btnHomePrev, 42, 42);
    lv_obj_align(btnHomePrev, LV_ALIGN_CENTER, -76, -12);
    lv_obj_set_style_radius(btnHomePrev, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btnHomePrev, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_border_color(btnHomePrev, lv_color_hex(0x30363d), 0);
    lv_obj_set_style_border_width(btnHomePrev, 1, 0);
    lv_obj_set_ext_click_area(btnHomePrev, 10);
    lv_obj_add_event_cb(btnHomePrev, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            selectedAppIndex = (selectedAppIndex - 1 + 4) % 4;
            updateHomeLauncherUI();
        }
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lblHP = lv_label_create(btnHomePrev);
    lv_label_set_text(lblHP, LV_SYMBOL_LEFT);
    lv_obj_center(lblHP);
    lv_obj_set_style_text_color(lblHP, lv_color_hex(0x58a6ff), 0);
    lv_obj_set_style_text_font(lblHP, &lv_font_montserrat_18, 0);

    // Right Arrow Button (⮞)
    btnHomeNext = lv_btn_create(scrHomeLauncher);
    lv_obj_set_size(btnHomeNext, 42, 42);
    lv_obj_align(btnHomeNext, LV_ALIGN_CENTER, 76, -12);
    lv_obj_set_style_radius(btnHomeNext, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btnHomeNext, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_border_color(btnHomeNext, lv_color_hex(0x30363d), 0);
    lv_obj_set_style_border_width(btnHomeNext, 1, 0);
    lv_obj_set_ext_click_area(btnHomeNext, 10);
    lv_obj_add_event_cb(btnHomeNext, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            selectedAppIndex = (selectedAppIndex + 1) % 4;
            updateHomeLauncherUI();
        }
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lblHN = lv_label_create(btnHomeNext);
    lv_label_set_text(lblHN, LV_SYMBOL_RIGHT);
    lv_obj_center(lblHN);
    lv_obj_set_style_text_color(lblHN, lv_color_hex(0x58a6ff), 0);
    lv_obj_set_style_text_font(lblHN, &lv_font_montserrat_18, 0);

    // Center App Card (76x76 circular button)
    btnAppCard = lv_btn_create(scrHomeLauncher);
    lv_obj_set_size(btnAppCard, 76, 76);
    lv_obj_align(btnAppCard, LV_ALIGN_CENTER, 0, -14);
    lv_obj_set_style_radius(btnAppCard, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btnAppCard, lv_color_hex(0x0d2818), 0);
    lv_obj_set_style_border_color(btnAppCard, lv_color_hex(0x2ea043), 0);
    lv_obj_set_style_border_width(btnAppCard, 3, 0);
    lv_obj_set_ext_click_area(btnAppCard, 10);
    lv_obj_add_event_cb(btnAppCard, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            homeLaunchSelectedApp();
        }
    }, LV_EVENT_CLICKED, NULL);

    lblIconCard = lv_label_create(btnAppCard);
    lv_label_set_text(lblIconCard, LV_SYMBOL_AUDIO);
    lv_obj_center(lblIconCard);
    lv_obj_set_style_text_color(lblIconCard, lv_color_hex(0x2ea043), 0);
    lv_obj_set_style_text_font(lblIconCard, &lv_font_montserrat_32, 0);

    // App Title Label
    lblAppTitle = lv_label_create(scrHomeLauncher);
    lv_label_set_text(lblAppTitle, "RADIO");
    lv_obj_set_style_text_font(lblAppTitle, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lblAppTitle, lv_color_hex(0x2ea043), 0);
    lv_obj_align(lblAppTitle, LV_ALIGN_CENTER, 0, 38);

    // App Subtitle Label
    lblAppSub = lv_label_create(scrHomeLauncher);
    lv_obj_set_width(lblAppSub, 170);
    lv_obj_set_style_text_align(lblAppSub, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lblAppSub, "10 Malayalam Stations");
    lv_obj_set_style_text_font(lblAppSub, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblAppSub, lv_color_hex(0x8b949e), 0);
    lv_label_set_long_mode(lblAppSub, LV_LABEL_LONG_DOT);
    lv_obj_align(lblAppSub, LV_ALIGN_CENTER, 0, 58);

    // Live Audio Indicator
    lblHomeAudio = lv_label_create(scrHomeLauncher);
    lv_obj_set_width(lblHomeAudio, 160);
    lv_obj_set_style_text_align(lblHomeAudio, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lblHomeAudio, "");
    lv_obj_set_style_text_font(lblHomeAudio, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblHomeAudio, lv_color_hex(0x2ea043), 0);
    lv_label_set_long_mode(lblHomeAudio, LV_LABEL_LONG_DOT);
    lv_obj_align(lblHomeAudio, LV_ALIGN_CENTER, 0, 78);

    // 4 Selection Dots (Pills)
    int dotX[4] = { -24, -8, 8, 24 };
    for (int i = 0; i < 4; i++) {
        dotAppIndicators[i] = lv_obj_create(scrHomeLauncher);
        lv_obj_set_size(dotAppIndicators[i], (i == 0) ? 16 : 6, 5);
        lv_obj_set_style_radius(dotAppIndicators[i], 3, 0);
        lv_obj_set_style_bg_color(dotAppIndicators[i], (i == 0) ? lv_color_hex(0x2ea043) : lv_color_hex(0x30363d), 0);
        lv_obj_set_style_border_width(dotAppIndicators[i], 0, 0);
        lv_obj_align(dotAppIndicators[i], LV_ALIGN_BOTTOM_MID, dotX[i], -22);
        lv_obj_clear_flag(dotAppIndicators[i], LV_OBJ_FLAG_CLICKABLE);
    }

    // ---------------------------------------------------------------------
    // 1. SCREEN_RADIO (Clean, high-performance, circular-optimized)
    // ---------------------------------------------------------------------
    scrRadio = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scrRadio, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(scrRadio, LV_OBJ_FLAG_SCROLLABLE);

    // Circular Perimeter Volume Progress Arc (228x228)
    arcRing = lv_arc_create(scrRadio);
    lv_obj_set_size(arcRing, 228, 228);
    lv_obj_center(arcRing);
    lv_arc_set_rotation(arcRing, 135);
    lv_arc_set_bg_angles(arcRing, 0, 270);
    lv_arc_set_range(arcRing, 0, 21);
    lv_arc_set_value(arcRing, currentVolume);
    lv_obj_clear_flag(arcRing, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(arcRing, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(arcRing, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(arcRing, 0, 0);
    // Background track (subtle dark ring along circular bezel)
    lv_obj_set_style_arc_width(arcRing, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arcRing, lv_color_hex(0x21262d), LV_PART_MAIN);
    // Indicator progress fill (vibrant blue or red if muted)
    lv_obj_set_style_arc_width(arcRing, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arcRing, lv_color_hex(isMuted ? 0xda3633 : 0x1f6feb), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arcRing, true, LV_PART_INDICATOR);
    // Hide knob for sleek bezel-integrated progress ring
    lv_obj_set_style_opa(arcRing, LV_OPA_TRANSP, LV_PART_KNOB);

    // Top-Center: Large Home Button (⌂) safely inside circular display
    btnRadioHome = lv_btn_create(scrRadio);
    lv_obj_set_size(btnRadioHome, 38, 38);
    lv_obj_align(btnRadioHome, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_radius(btnRadioHome, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btnRadioHome, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_border_color(btnRadioHome, lv_color_hex(0x30363d), 0);
    lv_obj_set_style_border_width(btnRadioHome, 1, 0);
    lv_obj_set_ext_click_area(btnRadioHome, 10);
    lv_obj_add_event_cb(btnRadioHome, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) switchScreen(SCREEN_HOME_LAUNCHER);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lblRH = lv_label_create(btnRadioHome);
    lv_label_set_text(lblRH, LV_SYMBOL_HOME);
    lv_obj_center(lblRH);
    lv_obj_set_style_text_color(lblRH, lv_color_hex(0x58a6ff), 0);
    lv_obj_set_style_text_font(lblRH, &lv_font_montserrat_16, 0);

    btnRadioSettings = nullptr; // Uncluttered radio interface

    // Top Center: Channel Status Capsule
    lblBadgeTop = lv_label_create(scrRadio);
    lv_obj_set_style_text_color(lblBadgeTop, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(lblBadgeTop, &lv_font_montserrat_10, 0);
    lv_obj_set_style_bg_color(lblBadgeTop, lv_color_hex(0x1f6feb), 0);
    lv_obj_set_style_bg_opa(lblBadgeTop, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(lblBadgeTop, 8, 0);
    lv_obj_set_style_pad_ver(lblBadgeTop, 2, 0);
    lv_obj_set_style_radius(lblBadgeTop, 8, 0);
    lv_obj_align(lblBadgeTop, LV_ALIGN_TOP_MID, 0, 48);
    lv_label_set_text(lblBadgeTop, "CH 01/10");

    // Station Name (Large, clear, centered)
    lblStationName = lv_label_create(scrRadio);
    lv_obj_set_width(lblStationName, 180);
    lv_obj_set_style_text_align(lblStationName, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lblStationName, lv_color_hex(0xf0f6fc), 0);
    lv_obj_set_style_text_font(lblStationName, &lv_font_montserrat_18, 0);
    lv_label_set_long_mode(lblStationName, LV_LABEL_LONG_DOT);
    lv_obj_align(lblStationName, LV_ALIGN_CENTER, 0, -18);
    lv_label_set_text(lblStationName, "EDIFIER Radio");

    // Subtitle: State & Language
    lblMeta = lv_label_create(scrRadio);
    lv_obj_set_width(lblMeta, 170);
    lv_obj_set_style_text_align(lblMeta, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lblMeta, lv_color_hex(0x8b949e), 0);
    lv_obj_set_style_text_font(lblMeta, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(lblMeta, LV_LABEL_LONG_DOT);
    lv_obj_align(lblMeta, LV_ALIGN_CENTER, 0, 8);
    lv_label_set_text(lblMeta, "Live Stream");

    // Volume Pill Indicator
    lblVolBadge = lv_label_create(scrRadio);
    lv_obj_set_style_text_font(lblVolBadge, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblVolBadge, lv_color_hex(0x58a6ff), 0);
    lv_obj_set_style_bg_color(lblVolBadge, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_bg_opa(lblVolBadge, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(lblVolBadge, 10, 0);
    lv_obj_set_style_pad_ver(lblVolBadge, 3, 0);
    lv_obj_set_style_radius(lblVolBadge, 8, 0);
    lv_obj_align(lblVolBadge, LV_ALIGN_CENTER, 0, 32);
    lv_label_set_text(lblVolBadge, "VOL 18 / 21");

    // Left Arrow (Prev Station) - generous 42x42 touch button with ext click area
    btnPrev = lv_btn_create(scrRadio);
    lv_obj_set_size(btnPrev, 42, 42);
    lv_obj_align(btnPrev, LV_ALIGN_BOTTOM_MID, -54, -20);
    lv_obj_set_style_radius(btnPrev, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btnPrev, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_border_color(btnPrev, lv_color_hex(0x30363d), 0);
    lv_obj_set_style_border_width(btnPrev, 1, 0);
    lv_obj_set_ext_click_area(btnPrev, 10);
    lv_obj_add_event_cb(btnPrev, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) prevStation();
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lblPrev = lv_label_create(btnPrev);
    lv_label_set_text(lblPrev, LV_SYMBOL_PREV);
    lv_obj_center(lblPrev);
    lv_obj_set_style_text_color(lblPrev, lv_color_hex(0x58a6ff), 0);
    lv_obj_set_style_text_font(lblPrev, &lv_font_montserrat_16, 0);

    // Center Big Play/Pause Button (54x54 circular button with large icon)
    btnPlayPause = lv_btn_create(scrRadio);
    lv_obj_set_size(btnPlayPause, 54, 54);
    lv_obj_align(btnPlayPause, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_radius(btnPlayPause, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btnPlayPause, lv_color_hex(0x238636), 0);
    lv_obj_set_style_border_color(btnPlayPause, lv_color_hex(0x2ea043), 0);
    lv_obj_set_style_border_width(btnPlayPause, 2, 0);
    lv_obj_set_ext_click_area(btnPlayPause, 8);
    lv_obj_add_event_cb(btnPlayPause, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) togglePlayPause();
    }, LV_EVENT_CLICKED, NULL);
    lblPlayPauseIcon = lv_label_create(btnPlayPause);
    lv_label_set_text(lblPlayPauseIcon, LV_SYMBOL_PLAY);
    lv_obj_center(lblPlayPauseIcon);
    lv_obj_set_style_text_color(lblPlayPauseIcon, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(lblPlayPauseIcon, &lv_font_montserrat_24, 0);

    // Right Arrow (Next Station) - generous 42x42 touch button with ext click area
    btnNext = lv_btn_create(scrRadio);
    lv_obj_set_size(btnNext, 42, 42);
    lv_obj_align(btnNext, LV_ALIGN_BOTTOM_MID, 54, -20);
    lv_obj_set_style_radius(btnNext, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btnNext, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_border_color(btnNext, lv_color_hex(0x30363d), 0);
    lv_obj_set_style_border_width(btnNext, 1, 0);
    lv_obj_set_ext_click_area(btnNext, 10);
    lv_obj_add_event_cb(btnNext, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) nextStation();
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lblNext = lv_label_create(btnNext);
    lv_label_set_text(lblNext, LV_SYMBOL_NEXT);
    lv_obj_center(lblNext);
    lv_obj_set_style_text_color(lblNext, lv_color_hex(0x58a6ff), 0);
    lv_obj_set_style_text_font(lblNext, &lv_font_montserrat_16, 0);

    // Top-Center: IP Address Capsule (Touch shortcut to Settings, gives IP for Web Remote)
    lblIpHint = lv_label_create(scrRadio);
    lv_obj_set_style_text_font(lblIpHint, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblIpHint, lv_color_hex(0x58a6ff), 0);
    lv_obj_set_style_bg_color(lblIpHint, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_bg_opa(lblIpHint, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(lblIpHint, lv_color_hex(0x30363d), 0);
    lv_obj_set_style_border_width(lblIpHint, 1, 0);
    lv_obj_set_style_radius(lblIpHint, 6, 0);
    lv_obj_set_style_pad_hor(lblIpHint, 7, 0);
    lv_obj_set_style_pad_ver(lblIpHint, 2, 0);
    lv_obj_align(lblIpHint, LV_ALIGN_TOP_MID, 0, 68);
    lv_obj_add_flag(lblIpHint, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(lblIpHint, 8);
    lv_obj_add_event_cb(lblIpHint, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            switchScreen(SCREEN_SETTINGS);
        }
    }, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lblIpHint, LV_SYMBOL_WIFI " 192.168.1.101");

    // ---------------------------------------------------------------------
    // 2. SCREEN_CLOCK (Luxury Analog, Chrono, Cyber Digital)
    // ---------------------------------------------------------------------
    scrClock = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scrClock, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(scrClock, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(scrClock, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            if (isScreensaverActive) {
                isScreensaverActive = false;
                switchScreen(SCREEN_HOME_LAUNCHER);
            } else {
                cycleClockFace();
            }
        }
    }, LV_EVENT_CLICKED, NULL);

    // Top-Right Home button safely placed inside circle (38x38 touch target)
    btnClockHome = lv_btn_create(scrClock);
    lv_obj_set_size(btnClockHome, 38, 38);
    lv_obj_align(btnClockHome, LV_ALIGN_TOP_MID, 32, 14);
    lv_obj_set_style_radius(btnClockHome, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btnClockHome, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_border_color(btnClockHome, lv_color_hex(0x30363d), 0);
    lv_obj_set_style_border_width(btnClockHome, 1, 0);
    lv_obj_set_ext_click_area(btnClockHome, 10);
    lv_obj_add_event_cb(btnClockHome, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            isScreensaverActive = false;
            switchScreen(SCREEN_HOME_LAUNCHER);
        }
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lblCH = lv_label_create(btnClockHome);
    lv_label_set_text(lblCH, LV_SYMBOL_HOME);
    lv_obj_center(lblCH);
    lv_obj_set_style_text_color(lblCH, lv_color_hex(0x58a6ff), 0);
    lv_obj_set_style_text_font(lblCH, &lv_font_montserrat_16, 0);

    // Top-Left Face Toggle button safely placed inside circle (38x38 touch target)
    btnClockFaceToggle = lv_btn_create(scrClock);
    lv_obj_set_size(btnClockFaceToggle, 38, 38);
    lv_obj_align(btnClockFaceToggle, LV_ALIGN_TOP_MID, -32, 14);
    lv_obj_set_style_radius(btnClockFaceToggle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btnClockFaceToggle, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_border_color(btnClockFaceToggle, lv_color_hex(0x30363d), 0);
    lv_obj_set_style_border_width(btnClockFaceToggle, 1, 0);
    lv_obj_set_ext_click_area(btnClockFaceToggle, 10);
    lv_obj_add_event_cb(btnClockFaceToggle, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) cycleClockFace();
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lblCF = lv_label_create(btnClockFaceToggle);
    lv_label_set_text(lblCF, LV_SYMBOL_REFRESH);
    lv_obj_center(lblCF);
    lv_obj_set_style_text_color(lblCF, lv_color_hex(0x58a6ff), 0);
    lv_obj_set_style_text_font(lblCF, &lv_font_montserrat_16, 0);

    // Minimal Analog Dial Container (with 12, 3, 6, 9)
    contChronoDial = lv_obj_create(scrClock);
    lv_obj_set_size(contChronoDial, 240, 240);
    lv_obj_center(contChronoDial);
    lv_obj_set_style_bg_opa(contChronoDial, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(contChronoDial, 0, 0);
    lv_obj_clear_flag(contChronoDial, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    const struct { const char* num; lv_align_t al; int x; int y; } chronoNums[4] = {
        { "12", LV_ALIGN_TOP_MID, 0, 24 },
        { "6",  LV_ALIGN_BOTTOM_MID, 0, -24 },
        { "9",  LV_ALIGN_LEFT_MID, 24, 0 },
        { "3",  LV_ALIGN_RIGHT_MID, -24, 0 }
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t* cNum = lv_label_create(contChronoDial);
        lv_label_set_text(cNum, chronoNums[i].num);
        lv_obj_set_style_text_font(cNum, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(cNum, lv_color_hex(0xf0f6fc), 0);
        lv_obj_align(cNum, chronoNums[i].al, chronoNums[i].x, chronoNums[i].y);
    }

    contAnalogDial = contChronoDial; // Unified lightweight analog container

    // Hands Layer
    lineHour = lv_line_create(scrClock);
    lv_line_set_points(lineHour, p_hour, 2);
    lv_obj_set_style_line_width(lineHour, 4, 0);
    lv_obj_set_style_line_color(lineHour, lv_color_hex(0xf0f6fc), 0);
    lv_obj_set_style_line_rounded(lineHour, true, 0);

    lineMin = lv_line_create(scrClock);
    lv_line_set_points(lineMin, p_min, 2);
    lv_obj_set_style_line_width(lineMin, 3, 0);
    lv_obj_set_style_line_color(lineMin, lv_color_hex(0x58a6ff), 0);
    lv_obj_set_style_line_rounded(lineMin, true, 0);

    lineSec = lv_line_create(scrClock);
    lv_line_set_points(lineSec, p_sec, 2);
    lv_obj_set_style_line_width(lineSec, 2, 0);
    lv_obj_set_style_line_color(lineSec, lv_color_hex(0xf85149), 0);
    lv_obj_set_style_line_rounded(lineSec, true, 0);

    objClockHub = lv_obj_create(scrClock);
    lv_obj_set_size(objClockHub, 10, 10);
    lv_obj_center(objClockHub);
    lv_obj_set_style_radius(objClockHub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(objClockHub, lv_color_hex(0xf85149), 0);
    lv_obj_set_style_border_color(objClockHub, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_width(objClockHub, 2, 0);

    // Analog Date Capsule: "MON 21 SEP"
    lblAnalogDate = lv_label_create(scrClock);
    lv_obj_set_style_text_font(lblAnalogDate, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblAnalogDate, lv_color_hex(0x58a6ff), 0);
    lv_obj_align(lblAnalogDate, LV_ALIGN_CENTER, 0, -45);
    lv_label_set_text(lblAnalogDate, "MON 21 SEP");

    // Analog Station Capsule: "▶ Station"
    lblAnalogStation = lv_label_create(scrClock);
    lv_obj_set_width(lblAnalogStation, 140);
    lv_obj_set_style_text_align(lblAnalogStation, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lblAnalogStation, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblAnalogStation, lv_color_hex(0x2ea043), 0);
    lv_label_set_long_mode(lblAnalogStation, LV_LABEL_LONG_DOT);
    lv_obj_align(lblAnalogStation, LV_ALIGN_CENTER, 0, 48);
    lv_label_set_text(lblAnalogStation, "EDIFIER Radio");

    // Container for Cyber Digital Face
    contDigital = lv_obj_create(scrClock);
    lv_obj_set_size(contDigital, 220, 220);
    lv_obj_center(contDigital);
    lv_obj_set_style_bg_opa(contDigital, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(contDigital, 0, 0);
    lv_obj_clear_flag(contDigital, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lblDigitalTime = lv_label_create(contDigital);
    lv_obj_set_style_text_font(lblDigitalTime, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(lblDigitalTime, lv_color_hex(0xf0f6fc), 0);
    lv_obj_align(lblDigitalTime, LV_ALIGN_CENTER, -16, -14);
    lv_label_set_text(lblDigitalTime, "12:00");

    lblDigitalSec = lv_label_create(contDigital);
    lv_obj_set_style_text_font(lblDigitalSec, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lblDigitalSec, lv_color_hex(0x58a6ff), 0);
    lv_obj_align(lblDigitalSec, LV_ALIGN_CENTER, 60, -8);
    lv_label_set_text(lblDigitalSec, ":00");

    lblDigitalDate = lv_label_create(contDigital);
    lv_obj_set_style_text_font(lblDigitalDate, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblDigitalDate, lv_color_hex(0x8b949e), 0);
    lv_obj_align(lblDigitalDate, LV_ALIGN_CENTER, 0, 18);
    lv_label_set_text(lblDigitalDate, "Monday, Sep 21");

    lblDigitalAudio = lv_label_create(contDigital);
    lv_obj_set_width(lblDigitalAudio, 150);
    lv_obj_set_style_text_align(lblDigitalAudio, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(lblDigitalAudio, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblDigitalAudio, lv_color_hex(0x2ea043), 0);
    lv_label_set_long_mode(lblDigitalAudio, LV_LABEL_LONG_DOT);
    lv_obj_align(lblDigitalAudio, LV_ALIGN_CENTER, 0, 42);
    lv_label_set_text(lblDigitalAudio, "♫ EDIFIER Radio");

    lblDigitalWifi = lv_label_create(contDigital);
    lv_obj_set_style_text_font(lblDigitalWifi, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblDigitalWifi, lv_color_hex(0x484f58), 0);
    lv_obj_align(lblDigitalWifi, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_label_set_text(lblDigitalWifi, "192.168.4.1");

    // ---------------------------------------------------------------------
    // 3. SCREEN_SETTINGS
    // ---------------------------------------------------------------------
    scrSettings = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scrSettings, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(scrSettings, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblSetTitle = lv_label_create(scrSettings);
    lv_label_set_text(lblSetTitle, "SETTINGS");
    lv_obj_set_style_text_font(lblSetTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblSetTitle, lv_color_hex(0x58a6ff), 0);
    lv_obj_align(lblSetTitle, LV_ALIGN_TOP_MID, -22, 16);

    btnSettingsHome = lv_btn_create(scrSettings);
    lv_obj_set_size(btnSettingsHome, 38, 38);
    lv_obj_align(btnSettingsHome, LV_ALIGN_TOP_MID, 36, 12);
    lv_obj_set_style_radius(btnSettingsHome, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btnSettingsHome, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_border_color(btnSettingsHome, lv_color_hex(0x30363d), 0);
    lv_obj_set_style_border_width(btnSettingsHome, 1, 0);
    lv_obj_set_ext_click_area(btnSettingsHome, 10);
    lv_obj_add_event_cb(btnSettingsHome, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            switchScreen(SCREEN_HOME_LAUNCHER);
        }
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lblSH = lv_label_create(btnSettingsHome);
    lv_label_set_text(lblSH, LV_SYMBOL_HOME);
    lv_obj_center(lblSH);
    lv_obj_set_style_text_color(lblSH, lv_color_hex(0x58a6ff), 0);
    lv_obj_set_style_text_font(lblSH, &lv_font_montserrat_16, 0);

    contSettingsList = lv_obj_create(scrSettings);
    lv_obj_set_size(contSettingsList, 160, 136);
    lv_obj_align(contSettingsList, LV_ALIGN_CENTER, 0, 18);
    lv_obj_set_style_bg_opa(contSettingsList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(contSettingsList, 0, 0);
    lv_obj_set_style_pad_all(contSettingsList, 2, 0);
    lv_obj_set_style_pad_top(contSettingsList, 12, 0);
    lv_obj_set_style_pad_bottom(contSettingsList, 12, 0);
    lv_obj_set_scroll_snap_y(contSettingsList, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_flex_flow(contSettingsList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(contSettingsList, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    auto makeSetBtn = [](lv_obj_t* parent, const char* initText, lv_obj_t** outLbl, lv_event_cb_t cb, int rowIdx) -> lv_obj_t* {
        lv_obj_t* btn = lv_btn_create(parent);
        lv_obj_set_width(btn, 154);
        lv_obj_set_height(btn, 38);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x161b22), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x30363d), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_ext_click_area(btn, 4);
        lv_obj_set_user_data(btn, (void*)(intptr_t)rowIdx);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, initText);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xf0f6fc), 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 6, 0);
        if (outLbl) *outLbl = lbl;
        return btn;
    };

    btnOptWifi = makeSetBtn(contSettingsList, "📶 Wi-Fi Networks", &lblOptWifi, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) switchScreen(SCREEN_WIFI_SCAN);
    }, 0);

    btnOptSaver = makeSetBtn(contSettingsList, "⏰ Screensaver: 60s", &lblOptSaver, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) cycleScreensaverTimeout();
    }, 1);

    btnOptFace = makeSetBtn(contSettingsList, "⌚ Clock Face: Luxury", &lblOptFace, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) cycleClockFace();
    }, 2);

    btnOptBright = makeSetBtn(contSettingsList, "☀️ Backlight: 70%", &lblOptBright, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) cycleBrightness();
    }, 3);

    btnOptLed = makeSetBtn(contSettingsList, "💡 Ambient LED: Breathe", &lblOptLed, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) cycleLedMode();
    }, 4);

    btnOptReset = makeSetBtn(contSettingsList, "🔄 Reset 10 Stations", &lblOptReset, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            setStarterFavorites();
            switchScreen(SCREEN_RADIO);
        }
    }, 5);

    btnOptHome = makeSetBtn(contSettingsList, "⌂ Home Launcher", &lblOptHome, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) switchScreen(SCREEN_HOME_LAUNCHER);
    }, 6);

    // ---------------------------------------------------------------------
    // 4. SCREEN_WIFI_SCAN
    // ---------------------------------------------------------------------
    scrWifiScan = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scrWifiScan, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(scrWifiScan, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblScanHead = lv_label_create(scrWifiScan);
    lv_label_set_text(lblScanHead, "SELECT WI-FI");
    lv_obj_set_style_text_font(lblScanHead, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblScanHead, lv_color_hex(0x58a6ff), 0);
    lv_obj_align(lblScanHead, LV_ALIGN_TOP_MID, 0, 16);

    btnScanBack = lv_btn_create(scrWifiScan);
    lv_obj_set_size(btnScanBack, 38, 38);
    lv_obj_align(btnScanBack, LV_ALIGN_TOP_MID, -34, 12);
    lv_obj_set_style_radius(btnScanBack, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btnScanBack, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_border_color(btnScanBack, lv_color_hex(0x30363d), 0);
    lv_obj_set_style_border_width(btnScanBack, 1, 0);
    lv_obj_set_ext_click_area(btnScanBack, 10);
    lv_obj_add_event_cb(btnScanBack, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) switchScreen(SCREEN_SETTINGS);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lblSB = lv_label_create(btnScanBack);
    lv_label_set_text(lblSB, LV_SYMBOL_LEFT);
    lv_obj_center(lblSB);
    lv_obj_set_style_text_color(lblSB, lv_color_hex(0x58a6ff), 0);
    lv_obj_set_style_text_font(lblSB, &lv_font_montserrat_16, 0);

    btnScanRescan = lv_btn_create(scrWifiScan);
    lv_obj_set_size(btnScanRescan, 38, 38);
    lv_obj_align(btnScanRescan, LV_ALIGN_TOP_MID, 34, 12);
    lv_obj_set_style_radius(btnScanRescan, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btnScanRescan, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_border_color(btnScanRescan, lv_color_hex(0x30363d), 0);
    lv_obj_set_style_border_width(btnScanRescan, 1, 0);
    lv_obj_set_ext_click_area(btnScanRescan, 10);
    lv_obj_add_event_cb(btnScanRescan, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) startWifiScan();
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lblSR = lv_label_create(btnScanRescan);
    lv_label_set_text(lblSR, LV_SYMBOL_REFRESH);
    lv_obj_center(lblSR);
    lv_obj_set_style_text_color(lblSR, lv_color_hex(0x58a6ff), 0);
    lv_obj_set_style_text_font(lblSR, &lv_font_montserrat_16, 0);

    lblScanStatus = lv_label_create(scrWifiScan);
    lv_label_set_text(lblScanStatus, "Scanning 2.4GHz Wi-Fi...");
    lv_obj_set_style_text_font(lblScanStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblScanStatus, lv_color_hex(0x8b949e), 0);
    lv_obj_align(lblScanStatus, LV_ALIGN_CENTER, 0, 0);

    listWifi = lv_obj_create(scrWifiScan);
    lv_obj_set_size(listWifi, 160, 136);
    lv_obj_align(listWifi, LV_ALIGN_CENTER, 0, 18);
    lv_obj_set_style_bg_opa(listWifi, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(listWifi, 0, 0);
    lv_obj_set_style_pad_all(listWifi, 2, 0);
    lv_obj_set_style_pad_top(listWifi, 10, 0);
    lv_obj_set_style_pad_bottom(listWifi, 10, 0);
    lv_obj_set_scroll_snap_y(listWifi, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_flex_flow(listWifi, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(listWifi, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // ---------------------------------------------------------------------
    // 5. SCREEN_WIFI_KEYPAD (Custom Rotary Password Dial)
    // ---------------------------------------------------------------------
    scrWifiKeypad = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scrWifiKeypad, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(scrWifiKeypad, LV_OBJ_FLAG_SCROLLABLE);

    lblKeypadTitle = lv_label_create(scrWifiKeypad);
    lv_label_set_text(lblKeypadTitle, "WiFi: SSID");
    lv_obj_set_style_text_font(lblKeypadTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblKeypadTitle, lv_color_hex(0x58a6ff), 0);
    lv_obj_align(lblKeypadTitle, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t* boxPass = lv_obj_create(scrWifiKeypad);
    lv_obj_set_size(boxPass, 144, 30);
    lv_obj_align(boxPass, LV_ALIGN_TOP_MID, 0, 38);
    lv_obj_set_style_bg_color(boxPass, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_border_color(boxPass, lv_color_hex(0x30363d), 0);
    lv_obj_set_style_border_width(boxPass, 1, 0);
    lv_obj_set_style_radius(boxPass, 8, 0);
    lv_obj_set_style_pad_all(boxPass, 4, 0);
    lv_obj_clear_flag(boxPass, LV_OBJ_FLAG_SCROLLABLE);

    lblKeypadPass = lv_label_create(boxPass);
    lv_label_set_text(lblKeypadPass, "_");
    lv_obj_set_style_text_font(lblKeypadPass, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblKeypadPass, lv_color_hex(0xf0f6fc), 0);
    lv_obj_center(lblKeypadPass);

    lblKeypadPrev2 = lv_label_create(scrWifiKeypad);
    lv_obj_set_style_text_font(lblKeypadPrev2, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblKeypadPrev2, lv_color_hex(0x30363d), 0);
    lv_obj_align(lblKeypadPrev2, LV_ALIGN_CENTER, -72, -8);

    lblKeypadPrev1 = lv_label_create(scrWifiKeypad);
    lv_obj_set_style_text_font(lblKeypadPrev1, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblKeypadPrev1, lv_color_hex(0x8b949e), 0);
    lv_obj_align(lblKeypadPrev1, LV_ALIGN_CENTER, -40, -8);

    boxKeypadCenter = lv_obj_create(scrWifiKeypad);
    lv_obj_set_size(boxKeypadCenter, 60, 48);
    lv_obj_align(boxKeypadCenter, LV_ALIGN_CENTER, 0, -8);
    lv_obj_set_style_bg_color(boxKeypadCenter, lv_color_hex(0x0d1117), 0);
    lv_obj_set_style_border_color(boxKeypadCenter, lv_color_hex(0x1f6feb), 0);
    lv_obj_set_style_border_width(boxKeypadCenter, 2, 0);
    lv_obj_set_style_radius(boxKeypadCenter, 10, 0);
    lv_obj_set_ext_click_area(boxKeypadCenter, 8);
    lv_obj_clear_flag(boxKeypadCenter, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(boxKeypadCenter, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            keypadSelectCurrent();
        }
    }, LV_EVENT_CLICKED, NULL);

    lblKeypadCurChar = lv_label_create(boxKeypadCenter);
    lv_label_set_text(lblKeypadCurChar, "a");
    lv_obj_set_style_text_font(lblKeypadCurChar, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lblKeypadCurChar, lv_color_hex(0x58a6ff), 0);
    lv_obj_center(lblKeypadCurChar);

    lblKeypadNext1 = lv_label_create(scrWifiKeypad);
    lv_obj_set_style_text_font(lblKeypadNext1, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblKeypadNext1, lv_color_hex(0x8b949e), 0);
    lv_obj_align(lblKeypadNext1, LV_ALIGN_CENTER, 40, -8);

    lblKeypadNext2 = lv_label_create(scrWifiKeypad);
    lv_obj_set_style_text_font(lblKeypadNext2, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblKeypadNext2, lv_color_hex(0x30363d), 0);
    lv_obj_align(lblKeypadNext2, LV_ALIGN_CENTER, 72, -8);

    lv_obj_t* lblKpHdr = lv_label_create(scrWifiKeypad);
    lv_label_set_text(lblKpHdr, "Turn: Select • Click: Enter");
    lv_obj_set_style_text_font(lblKpHdr, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblKpHdr, lv_color_hex(0x6e7681), 0);
    lv_obj_align(lblKpHdr, LV_ALIGN_CENTER, 0, 26);

    auto makeKpBtn = [](lv_obj_t* parent, const char* txt, lv_color_t bg, int w, int x, int y, lv_event_cb_t cb) -> lv_obj_t* {
        lv_obj_t* b = lv_btn_create(parent);
        lv_obj_set_size(b, w, 32);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, x, y);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_set_style_bg_color(b, bg, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_ext_click_area(b, 4);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, txt);
        lv_obj_center(l);
        lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
        return b;
    };

    btnKpDel = makeKpBtn(scrWifiKeypad, "⌫", lv_color_hex(0x21262d), 34, -54, -20, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            if (kpEnteredPass.length() > 0) {
                kpEnteredPass.remove(kpEnteredPass.length() - 1);
                updateKeypadPasswordLabel();
            }
        }
    });

    btnKpSpace = makeKpBtn(scrWifiKeypad, "␣", lv_color_hex(0x21262d), 34, -18, -20, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            if (kpEnteredPass.length() < 60) {
                kpEnteredPass += ' ';
                updateKeypadPasswordLabel();
            }
        }
    });

    btnKpConnect = makeKpBtn(scrWifiKeypad, "OK", lv_color_hex(0x238636), 34, 18, -20, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) connectWifiWithKeypad();
    });

    btnKpCancel = makeKpBtn(scrWifiKeypad, "✖", lv_color_hex(0x30363d), 34, 54, -20, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) switchScreen(SCREEN_SETTINGS);
    });

    // Load initial screen
    lv_scr_load(scrRadio);
    createPowerOffOverlay();
}

/* -------------------------------------------------------------------------
 * Power-Off Countdown Overlay & Standby Sleep Engine
 * ------------------------------------------------------------------------- */
static lv_obj_t* pwrOverlay = NULL;
static lv_obj_t* pwrArc = NULL;
static lv_obj_t* pwrTitle = NULL;
static lv_obj_t* pwrCountdownLbl = NULL;
static lv_obj_t* pwrSubLbl = NULL;
static bool pwrCountdownActive = false;

void createPowerOffOverlay() {
    if (pwrOverlay) return;

    // Use top layer so overlay displays across any active screen
    pwrOverlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(pwrOverlay, 240, 240);
    lv_obj_center(pwrOverlay);
    lv_obj_set_style_radius(pwrOverlay, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pwrOverlay, lv_color_hex(0x0a0c10), 0);
    lv_obj_set_style_bg_opa(pwrOverlay, LV_OPA_90, 0);
    lv_obj_set_style_border_color(pwrOverlay, lv_color_hex(0xda3633), 0);
    lv_obj_set_style_border_width(pwrOverlay, 2, 0);
    lv_obj_clear_flag(pwrOverlay, LV_OBJ_FLAG_SCROLLABLE);

    // Circular countdown progress arc
    pwrArc = lv_arc_create(pwrOverlay);
    lv_obj_set_size(pwrArc, 204, 204);
    lv_obj_center(pwrArc);
    lv_arc_set_rotation(pwrArc, 270);
    lv_arc_set_bg_angles(pwrArc, 0, 360);
    lv_arc_set_range(pwrArc, 0, 100);
    lv_arc_set_value(pwrArc, 0);
    lv_obj_remove_style(pwrArc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(pwrArc, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_color(pwrArc, lv_color_hex(0x21262d), LV_PART_MAIN);
    lv_obj_set_style_arc_width(pwrArc, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(pwrArc, lv_color_hex(0xf85149), LV_PART_INDICATOR);
    lv_obj_clear_flag(pwrArc, LV_OBJ_FLAG_CLICKABLE);

    // Title: "POWER OFF"
    pwrTitle = lv_label_create(pwrOverlay);
    lv_label_set_text(pwrTitle, "POWER OFF");
    lv_obj_set_style_text_font(pwrTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(pwrTitle, lv_color_hex(0xf85149), 0);
    lv_obj_align(pwrTitle, LV_ALIGN_CENTER, 0, -44);

    // Big countdown number: "3", "2", "1"
    pwrCountdownLbl = lv_label_create(pwrOverlay);
    lv_label_set_text(pwrCountdownLbl, "3");
    lv_obj_set_style_text_font(pwrCountdownLbl, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(pwrCountdownLbl, lv_color_hex(0xffffff), 0);
    lv_obj_align(pwrCountdownLbl, LV_ALIGN_CENTER, 0, 2);

    // Subtitle: "Release to cancel"
    pwrSubLbl = lv_label_create(pwrOverlay);
    lv_label_set_text(pwrSubLbl, "Release to cancel");
    lv_obj_set_style_text_font(pwrSubLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(pwrSubLbl, lv_color_hex(0x8b949e), 0);
    lv_obj_align(pwrSubLbl, LV_ALIGN_CENTER, 0, 44);

    lv_obj_add_flag(pwrOverlay, LV_OBJ_FLAG_HIDDEN);
}

void showPowerOffOverlay(unsigned long heldMs) {
    if (!pwrOverlay) return;
    lv_obj_clear_flag(pwrOverlay, LV_OBJ_FLAG_HIDDEN);

    float progress = (float)(heldMs - 700) / 3300.0f;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    lv_arc_set_value(pwrArc, (int)(progress * 100.0f));

    int secsRemaining = (int)((4000 - heldMs + 999) / 1000);
    if (secsRemaining < 1) secsRemaining = 1;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", secsRemaining);
    lv_label_set_text(pwrCountdownLbl, buf);

    // Red countdown NeoPixel illumination (clockwise direction)
    int activeLeds = (int)(progress * (NEOPIXEL_COUNT + 1));
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        int ledIdx = (NEOPIXEL_COUNT - 1) - i; // Clockwise order (4 down to 0)
        strip.setPixelColor(ledIdx, (i < activeLeds) ? strip.Color(220, 25, 25) : strip.Color(0, 0, 0));
    }
    strip.show();
}

void hidePowerOffOverlay() {
    if (!pwrOverlay) return;
    lv_obj_add_flag(pwrOverlay, LV_OBJ_FLAG_HIDDEN);
    updateAmbientLeds();
}

void enterPowerOffMode() {
    Serial.println("\n[POWER] 4-second hold confirmed -> Entering Power-Off Standby Mode...");

    if (pwrCountdownLbl) lv_label_set_text(pwrCountdownLbl, "OFF");
    if (pwrArc) lv_arc_set_value(pwrArc, 100);
    lv_timer_handler();
    delay(300);

    // 1. Stop audio playback cleanly
    sendAudioStop();
    isPlaying = false;
    isBuffering = false;

    // 2. Shut off NeoPixel LEDs
    strip.clear();
    strip.show();

    // 3. Smooth fade out LCD backlight
    for (int b = currentBacklightBrightness; b >= 0; b -= 5) {
        ledcWrite(LCD_BL_PIN, (b * 255) / 100);
        delay(15);
    }
    ledcWrite(LCD_BL_PIN, 0);

    // 4. Put display panel into low-power sleep
    gfx.sleep();

    // 5. Turn off hardware power rails and green power LED
    digitalWrite(PIN_PWR_LED, LOW);
    digitalWrite(PIN_PWR_EN1, LOW);
    digitalWrite(PIN_PWR_EN2, LOW);

    // 6. Shut down Wi-Fi radio cleanly
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // 7. Wait for user to release the dial button so it doesn't immediately re-trigger
    while (digitalRead(ENCODER_SW_PIN) == LOW) {
        delay(30);
    }
    delay(200); // Debounce physical switch release

    runStandbySleepLoop(false);
}

void runStandbySleepLoop(bool isColdBoot) {
    // 1. Keep hardware rails, backlight, screen, and green power LED OFF
    pinMode(PIN_PWR_LED, OUTPUT);
    digitalWrite(PIN_PWR_LED, LOW); // Green power LED OFF
    pinMode(PIN_PWR_EN1, OUTPUT);
    digitalWrite(PIN_PWR_EN1, HIGH); // Power rail for NeoPixel control
    pinMode(PIN_PWR_EN2, OUTPUT);
    digitalWrite(PIN_PWR_EN2, HIGH);

    pinMode(LCD_BL_PIN, OUTPUT);
    digitalWrite(LCD_BL_PIN, LOW); // Backlight OFF
    gfx.sleep();

    // 2. Shut off NeoPixel ring LEDs
    strip.begin();
    strip.clear();
    strip.show();

    // 3. Configure Rotary Center Switch
    pinMode(ENCODER_SW_PIN, INPUT_PULLUP);

    Serial.println("\n========================================================");
    Serial.println("  [STANDBY] Radio is in STANDBY MODE.");
    Serial.println("  [STANDBY] Screen OFF, Audio OFF, Ring LEDs OFF, Board Red LED ON.");
    Serial.println("  [STANDBY] Press and hold rotary dial for 4s to Turn ON.");
    Serial.println("========================================================\n");

    // 4. Standby Loop
    while (true) {
        // Check Auto-On Alarm
        if (timerEnabled) {
            time_t nowSec = time(nullptr);
            if (nowSec > 100000) {
                struct tm t;
                localtime_r(&nowSec, &t);
                if (t.tm_hour == timerHour && t.tm_min == timerMin && t.tm_sec < 5) {
                    Serial.println("\n[ALARM] >>> Auto-On Alarm Triggered! Powering ON... <<<");
                    bootFromAlarm = true;
                    if (isColdBoot) return;
                    else {
                        prefs.begin("crow_pwr", false);
                        prefs.putBool("wake_boot", true);
                        prefs.end();
                        delay(100);
                        ESP.restart();
                    }
                }
            }
        }

        // Check if Rotary Dial Push Switch is pressed (Active LOW)
        if (digitalRead(ENCODER_SW_PIN) == LOW) {
            unsigned long pressStart = millis();
            bool powerOnConfirmed = false;
            int spinStep = 0;

            strip.setBrightness(220); // Bright, vivid emerald green
            Serial.println("[POWER] Rotary push button pressed -> Monitoring 4s hold...");

            while (digitalRead(ENCODER_SW_PIN) == LOW) {
                unsigned long held = millis() - pressStart;

                strip.clear();
                // Clockwise spinning chase head around the 5 LEDs (4 -> 3 -> 2 -> 1 -> 0):
                int spinLed = (NEOPIXEL_COUNT - 1) - (spinStep % NEOPIXEL_COUNT);
                spinStep++;

                // Progressive hold fill in clockwise direction (0 to 5 LEDs over 4000ms)
                int activeLeds = (int)((held * (NEOPIXEL_COUNT + 1)) / 4000);
                if (activeLeds > NEOPIXEL_COUNT) activeLeds = NEOPIXEL_COUNT;

                // Fill latched LEDs in clockwise order (from 4 down to 0)
                for (int i = 0; i < NEOPIXEL_COUNT; i++) {
                    int ledIdx = (NEOPIXEL_COUNT - 1) - i;
                    if (i < activeLeds) {
                        strip.setPixelColor(ledIdx, strip.Color(0, 200, 40)); // Solid bright emerald green
                    }
                }
                // Highlight the active rotating chase head
                strip.setPixelColor(spinLed, strip.Color(40, 255, 80));
                strip.show();

                if (held >= 4000) {
                    powerOnConfirmed = true;
                    break;
                }
                delay(35);
            }

            if (powerOnConfirmed) {
                Serial.println("[POWER] 4s hold verified! Double emerald flash confirmation...");
                // Double emerald green confirmation flash
                for (int f = 0; f < 2; f++) {
                    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
                        strip.setPixelColor(i, strip.Color(0, 255, 60));
                    }
                    strip.show();
                    delay(180);
                    strip.clear();
                    strip.show();
                    delay(100);
                }

                // Wait for switch release before booting
                while (digitalRead(ENCODER_SW_PIN) == LOW) {
                    delay(20);
                }
                delay(100);

                if (isColdBoot) {
                    Serial.println("[POWER] Power-ON confirmed! Resuming setup directly...");
                    return; // Directly continues setup() to turn ON rails, backlight, loader and radio!
                } else {
                    Serial.println("[POWER] Power-ON confirmed! Clean rebooting into active radio...");
                    prefs.begin("crow_pwr", false);
                    prefs.putBool("wake_boot", true);
                    prefs.end();
                    delay(100);
                    ESP.restart();
                }
            } else {
                // Button released early (< 4 seconds) -> cancel, turn ring LEDs completely OFF
                strip.clear();
                strip.show();
                delay(100);
            }
        }

        delay(30); // Low-overhead non-blocking poll
    }
}

/* -------------------------------------------------------------------------
 * Update UI State (Dispatches to all active views)
 * ------------------------------------------------------------------------- */
void updateRadioUI() {
    if (currentScreen != SCREEN_RADIO) return;
    if (!scrRadio || !lblStationName) return;
    if (!isAdHocPlaying && runtimeStations.empty()) return;

    if (!isAdHocPlaying) {
        if (currentStationIndex < 0) currentStationIndex = 0;
        if (currentStationIndex >= (int)runtimeStations.size()) currentStationIndex = (int)runtimeStations.size() - 1;
        lv_label_set_text(lblStationName, runtimeStations[currentStationIndex].name.c_str());
    } else {
        lv_label_set_text(lblStationName, adHocStation.name.c_str());
    }

    bool showError = (isStreamError && millis() < streamErrorUntilMs);

    // Top Badge: Channel & Status
    char bBuf[32];
    if (showError) {
        snprintf(bBuf, sizeof(bBuf), "! URL OFFLINE");
        lv_obj_set_style_bg_color(lblBadgeTop, lv_color_hex(0xda3633), 0); // Warning red
    } else if (isBuffering) {
        snprintf(bBuf, sizeof(bBuf), "[ BUFF... ]");
        lv_obj_set_style_bg_color(lblBadgeTop, lv_color_hex(0xd29922), 0); // Golden yellow
    } else if (isPlaying) {
        if (isAdHocPlaying) {
            snprintf(bBuf, sizeof(bBuf), "ONLINE STREAM");
            lv_obj_set_style_bg_color(lblBadgeTop, lv_color_hex(0x1f6feb), 0);
        } else {
            snprintf(bBuf, sizeof(bBuf), "CH %02d/%02d", currentStationIndex + 1, (int)runtimeStations.size());
            lv_obj_set_style_bg_color(lblBadgeTop, lv_color_hex(0x238636), 0);
        }
    } else {
        snprintf(bBuf, sizeof(bBuf), "PAUSED");
        lv_obj_set_style_bg_color(lblBadgeTop, lv_color_hex(0x30363d), 0);
    }
    lv_label_set_text(lblBadgeTop, bBuf);

    // Subtitle Meta
    if (showError) {
        lv_label_set_text(lblMeta, streamErrorReason.c_str());
        lv_obj_set_style_text_color(lblMeta, lv_color_hex(0xf85149), 0); // High-contrast warning red
    } else {
        String mStr = isAdHocPlaying ? adHocStation.language : runtimeStations[currentStationIndex].language;
        String sStr = isAdHocPlaying ? adHocStation.state : runtimeStations[currentStationIndex].state;
        if (sStr.length() > 0) {
            mStr += " • " + sStr;
        }
        lv_label_set_text(lblMeta, mStr.c_str());
        lv_obj_set_style_text_color(lblMeta, lv_color_hex(0x8b949e), 0); // Classic silver/gray
    }

    // Volume Pill Indicator
    if (lblVolBadge) {
        char vBuf[32];
        if (currentVolume == 0) {
            snprintf(vBuf, sizeof(vBuf), "MUTED");
            lv_obj_set_style_text_color(lblVolBadge, lv_color_hex(0xda3633), 0);
        } else {
            snprintf(vBuf, sizeof(vBuf), "VOL %d / 21", currentVolume);
            lv_obj_set_style_text_color(lblVolBadge, lv_color_hex(0x58a6ff), 0);
        }
        lv_label_set_text(lblVolBadge, vBuf);
    }

    // Circular Perimeter Volume Progress Arc (270° around bezel)
    if (arcRing) {
        lv_arc_set_value(arcRing, currentVolume);
        if (isMuted || currentVolume == 0) {
            lv_obj_set_style_arc_color(arcRing, lv_color_hex(0xda3633), LV_PART_INDICATOR);
        } else {
            lv_obj_set_style_arc_color(arcRing, lv_color_hex(0x1f6feb), LV_PART_INDICATOR);
        }
    }

    // Dedicated Play/Pause button styling
    if (btnPlayPause && lblPlayPauseIcon) {
        if (showError) {
            lv_label_set_text(lblPlayPauseIcon, LV_SYMBOL_WARNING);
            lv_obj_set_style_bg_color(btnPlayPause, lv_color_hex(0xda3633), 0);
            lv_obj_set_style_border_color(btnPlayPause, lv_color_hex(0xf85149), 0);
        } else if (isBuffering) {
            lv_label_set_text(lblPlayPauseIcon, LV_SYMBOL_REFRESH);
            lv_obj_set_style_bg_color(btnPlayPause, lv_color_hex(0xd29922), 0);
            lv_obj_set_style_border_color(btnPlayPause, lv_color_hex(0xf0883e), 0);
        } else if (isPlaying) {
            lv_label_set_text(lblPlayPauseIcon, LV_SYMBOL_PAUSE);
            lv_obj_set_style_bg_color(btnPlayPause, lv_color_hex(0x238636), 0);
            lv_obj_set_style_border_color(btnPlayPause, lv_color_hex(0x2ea043), 0);
        } else {
            lv_label_set_text(lblPlayPauseIcon, LV_SYMBOL_PLAY);
            lv_obj_set_style_bg_color(btnPlayPause, lv_color_hex(0x1f6feb), 0);
            lv_obj_set_style_border_color(btnPlayPause, lv_color_hex(0x58a6ff), 0);
        }
    }

    // IP Address (for Web Remote access)
    if (lblIpHint) {
        char ipBuf[64];
        if (WiFi.status() == WL_CONNECTED) {
            snprintf(ipBuf, sizeof(ipBuf), LV_SYMBOL_WIFI " %s", WiFi.localIP().toString().c_str());
        } else {
            snprintf(ipBuf, sizeof(ipBuf), LV_SYMBOL_WIFI " 192.168.4.1 (AP)");
        }
        lv_label_set_text(lblIpHint, ipBuf);
    }
}

void updateHomeLauncherUI() {
    if (!scrHomeLauncher || !btnAppCard || !lblIconCard) return;

    struct AppConfig {
        const char* title;
        const char* icon;
        const char* sub;
        uint32_t colorHex;
        uint32_t bgHex;
    };

    String radioSub = isPlaying
        ? ("Live: " + (currentStationIndex >= 0 && currentStationIndex < (int)runtimeStations.size() ? runtimeStations[currentStationIndex].name : String("Radio")))
        : (String(runtimeStations.size()) + " Malayalam Stations");

    AppConfig apps[4] = {
        { "RADIO",            LV_SYMBOL_AUDIO,    radioSub.c_str(),            0x2ea043, 0x0d2818 },
        { "CLOCK (ANALOGUE)", LV_SYMBOL_IMAGE,    "Luxury & Chrono Faces",     0x58a6ff, 0x0c213f },
        { "CLOCK (DIGITAL)",  LV_SYMBOL_LIST,     "Cyber Modern Face",         0x79c0ff, 0x0f2942 },
        { "SETTINGS",         LV_SYMBOL_SETTINGS, "Wi-Fi, Screen & System",    0xf0883e, 0x2e1b0c }
    };

    if (selectedAppIndex < 0) selectedAppIndex = 0;
    if (selectedAppIndex > 3) selectedAppIndex = 3;

    lv_obj_set_style_border_color(btnAppCard, lv_color_hex(apps[selectedAppIndex].colorHex), 0);
    lv_obj_set_style_border_width(btnAppCard, 3, 0);
    lv_obj_set_style_bg_color(btnAppCard, lv_color_hex(apps[selectedAppIndex].bgHex), 0);
    lv_label_set_text(lblIconCard, apps[selectedAppIndex].icon);
    lv_obj_set_style_text_color(lblIconCard, lv_color_hex(apps[selectedAppIndex].colorHex), 0);

    if (lblAppTitle) {
        lv_label_set_text(lblAppTitle, apps[selectedAppIndex].title);
        lv_obj_set_style_text_color(lblAppTitle, lv_color_hex(apps[selectedAppIndex].colorHex), 0);
    }
    if (lblAppSub) {
        lv_label_set_text(lblAppSub, apps[selectedAppIndex].sub);
    }

    for (int i = 0; i < 4; i++) {
        if (!dotAppIndicators[i]) continue;
        if (i == selectedAppIndex) {
            lv_obj_set_size(dotAppIndicators[i], 16, 5);
            lv_obj_set_style_bg_color(dotAppIndicators[i], lv_color_hex(apps[selectedAppIndex].colorHex), 0);
        } else {
            lv_obj_set_size(dotAppIndicators[i], 6, 5);
            lv_obj_set_style_bg_color(dotAppIndicators[i], lv_color_hex(0x30363d), 0);
        }
    }

    if (lblHomeAudio) {
        if (isPlaying && selectedAppIndex != 0) {
            String au = "♫ " + (currentStationIndex >= 0 && currentStationIndex < (int)runtimeStations.size() ? runtimeStations[currentStationIndex].name : String("Radio"));
            lv_label_set_text(lblHomeAudio, au.c_str());
            lv_obj_clear_flag(lblHomeAudio, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(lblHomeAudio, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void updateClockUI() {
    clockTimerCb(NULL);
}

void updateUI() {
    if (currentScreen == SCREEN_HOME_LAUNCHER) {
        updateHomeLauncherUI();
    } else if (currentScreen == SCREEN_RADIO) {
        updateRadioUI();
    } else if (currentScreen == SCREEN_CLOCK) {
        updateClockUI();
    } else if (currentScreen == SCREEN_SETTINGS) {
        updateSettingsUI();
    }
}

/* -------------------------------------------------------------------------
 * Audio Playback Control
 * ------------------------------------------------------------------------- */
void triggerCdnFailover() {
    if (isAdHocPlaying) {
        Serial.printf("[AUDIO-FAIL] Direct online stream failed: %s\n", adHocStation.name.c_str());
        isBuffering = false;
        isPlaying = false;
        isStreamError = true;
        streamErrorReason = "! URL Offline / Stream 404";
        streamErrorUntilMs = millis() + 8000;
        sendAudioStop();
        updateUI();
        return;
    }
    if (runtimeStations.empty() || currentStationIndex < 0 || currentStationIndex >= (int)runtimeStations.size()) return;

    cdnFailoverCount++;
    Serial.printf("[CDN-FAILOVER] Failover event #%d for station: %s\n", 
                  cdnFailoverCount, runtimeStations[currentStationIndex].name.c_str());

    String baseFavUrl = runtimeStations[currentStationIndex].url;

    // Check if this is an Akashvani CloudFront/WavesPB stream with a 16-hex token
    String token = "";
    int lastSlash = baseFavUrl.lastIndexOf('/');
    if (lastSlash > 0) {
        String endPart = baseFavUrl.substring(lastSlash + 1);
        if (endPart.endsWith(".m3u8")) endPart = endPart.substring(0, endPart.length() - 5);
        if (endPart.length() >= 16) {
            token = endPart;
        }
    }

    if (token.length() >= 16) {
        // Akashvani dynamically distributes across 3 known CloudFront distributions
        static const char* AKASHVANI_CDNS[] = {
            "d1cvqgmbcpg5yn.cloudfront.net",
            "d1tmej9eu7kw5c.cloudfront.net",
            "d3hrxqn1tritdh.cloudfront.net"
        };
        const int NUM_CDNS = 3;

        int currentCdnIdx = -1;
        for (int i = 0; i < NUM_CDNS; i++) {
            if (activeStreamUrl.indexOf(AKASHVANI_CDNS[i]) >= 0) {
                currentCdnIdx = i;
                break;
            }
        }
        if (currentCdnIdx < 0) {
            for (int i = 0; i < NUM_CDNS; i++) {
                if (baseFavUrl.indexOf(AKASHVANI_CDNS[i]) >= 0) {
                    currentCdnIdx = i;
                    break;
                }
            }
        }

        // Attempts 1 & 2: Rotate to the other 2 CloudFront CDNs
        if (cdnFailoverCount <= 2) {
            int nextCdnIdx = (currentCdnIdx + cdnFailoverCount) % NUM_CDNS;
            if (currentCdnIdx < 0) nextCdnIdx = cdnFailoverCount - 1;
            activeStreamUrl = "https://" + String(AKASHVANI_CDNS[nextCdnIdx]) + "/" + token + "/" + token + ".m3u8";
            Serial.printf("[CDN-FAILOVER] Attempt %d: Auto-routing to alternate CloudFront CDN (%s): %s\n", 
                          cdnFailoverCount, AKASHVANI_CDNS[nextCdnIdx], activeStreamUrl.c_str());
            bufferingStartMs = millis();
            isBuffering = true;
            isPlaying = true;
            updateUI();
            sendAudioConnect(activeStreamUrl.c_str());
            return;
        }

        // Attempt 3: Authoritative wavespb coordinator
        if (cdnFailoverCount == 3) {
            activeStreamUrl = "https://radio.wavespb.com/live/" + token + "/" + token + ".m3u8";
            Serial.printf("[CDN-FAILOVER] Attempt 3: Fallback to wavespb coordinator: %s\n", activeStreamUrl.c_str());
            bufferingStartMs = millis();
            isBuffering = true;
            isPlaying = true;
            updateUI();
            sendAudioConnect(activeStreamUrl.c_str());
            return;
        }
    } else {
        // Non-Akashvani stream: clean retry
        if (cdnFailoverCount <= 2) {
            activeStreamUrl = baseFavUrl;
            Serial.printf("[CDN-FAILOVER] Attempt %d: Retrying stream URL: %s\n", cdnFailoverCount, activeStreamUrl.c_str());
            bufferingStartMs = millis();
            isBuffering = true;
            isPlaying = true;
            updateUI();
            sendAudioConnect(activeStreamUrl.c_str());
            return;
        }
    }

    // Failover exhausted - station stream URL is offline or dead
    Serial.println("[AUDIO-FAIL] Stream unavailable or exhausted failover attempts.");
    isBuffering = false;
    isPlaying = false;
    isStreamError = true;
    streamErrorReason = "! URL Offline / Stream 404";
    streamErrorUntilMs = millis() + 8000;
    cdnFailoverCount = 0;
    sendAudioStop();
    updateUI();
}

void playCurrentStation() {
    isAdHocPlaying = false;
    isStreamError = false;
    streamErrorReason = "";
    streamErrorUntilMs = 0;
    if (runtimeStations.empty()) return;
    if (currentStationIndex < 0 || currentStationIndex >= (int)runtimeStations.size()) {
        currentStationIndex = 0;
    }
    saveLastStationIndex(currentStationIndex);

    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("[AUDIO] Cannot connect to station: WiFi not connected! (Connect to SSID '%s' at http://192.168.4.1)\n", AP_SSID);
        sendAudioStop();
        isBuffering = false;
        isPlaying = false;
        isStreamError = true;
        streamErrorReason = "! Wi-Fi Disconnected";
        streamErrorUntilMs = millis() + 6000;
        updateUI();
        return;
    }

    activeStreamUrl = runtimeStations[currentStationIndex].url;
    cdnFailoverCount = 0;
    bufferingStartMs = millis();

    Serial.printf("[AUDIO] Connecting to CH %02d: %s -> %s\n",
                  currentStationIndex + 1,
                  runtimeStations[currentStationIndex].name.c_str(),
                  activeStreamUrl.c_str());
    Serial.printf("[HEAP] Free: %u, MaxAlloc: %u, FreePSRAM: %u\n",
                  ESP.getFreeHeap(),
                  ESP.getMaxAllocHeap(),
                  ESP.getFreePsram());

    isBuffering = true;
    isPlaying = true;
    updateUI();

    if (activeStreamUrl.startsWith("https://airhlspush.pc.cdn.bitgravity.com")) {
        activeStreamUrl.replace("https://", "http://");
    }
    sendAudioConnect(activeStreamUrl.c_str());
}

void playDirectStream(const char* name, const char* url, const char* state, const char* lang) {
    if (!url || strlen(url) == 0) return;
    isStreamError = false;
    streamErrorReason = "";
    streamErrorUntilMs = 0;
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[AUDIO] Cannot play direct stream: WiFi not connected!");
        sendAudioStop();
        isBuffering = false;
        isPlaying = false;
        isStreamError = true;
        streamErrorReason = "! Wi-Fi Disconnected";
        streamErrorUntilMs = millis() + 6000;
        updateUI();
        return;
    }

    isAdHocPlaying = true;
    adHocStation.name = (name && strlen(name) > 0) ? name : "Online Station";
    adHocStation.url = url;
    adHocStation.state = (state && strlen(state) > 0) ? state : "";
    adHocStation.language = (lang && strlen(lang) > 0) ? lang : "Stream";

    activeStreamUrl = adHocStation.url;
    cdnFailoverCount = 0;
    bufferingStartMs = millis();

    Serial.printf("[AUDIO] Direct Online Play: %s -> %s\n",
                  adHocStation.name.c_str(),
                  activeStreamUrl.c_str());

    isBuffering = true;
    isPlaying = true;
    updateUI();

    if (activeStreamUrl.startsWith("https://airhlspush.pc.cdn.bitgravity.com")) {
        activeStreamUrl.replace("https://", "http://");
    }
    sendAudioConnect(activeStreamUrl.c_str());
}

void setVolume(int v) {
    if (v < 0) v = 0;
    if (v > 21) v = 21;
    currentVolume = v;
    isMuted = (currentVolume == 0);
    sendAudioSetVolume(currentVolume);
    if (currentScreen == SCREEN_RADIO && arcRing) {
        lv_arc_set_value(arcRing, currentVolume);
        lv_obj_set_style_arc_color(arcRing, lv_color_hex(isMuted ? 0xda3633 : 0x1f6feb), LV_PART_INDICATOR);
    }
    updateUI();
}

void toggleMute() {
    if (isMuted) {
        isMuted = false;
        currentVolume = (preMuteVolume > 0) ? preMuteVolume : 12;
        sendAudioSetVolume(currentVolume);
    } else {
        preMuteVolume = currentVolume;
        isMuted = true;
        currentVolume = 0;
        sendAudioSetVolume(0);
    }
    if (currentScreen == SCREEN_RADIO && arcRing) {
        lv_arc_set_value(arcRing, currentVolume);
        lv_obj_set_style_arc_color(arcRing, lv_color_hex(isMuted ? 0xda3633 : 0x1f6feb), LV_PART_INDICATOR);
    }
    updateUI();
}

/* -------------------------------------------------------------------------
 * Audio Callbacks (ESP32-audioI2S v4.0.0 Event Dispatcher)
 * ------------------------------------------------------------------------- */
void my_audio_info(Audio::msg_t m) {
    // Suppress high-frequency VU and BANDS logs that flood UART buffer and stall the MCU
    if (m.s && (strstr(m.s, "VU") != NULL || strstr(m.s, "BANDS") != NULL)) {
        return;
    }

    if (m.s && m.msg) {
        Serial.printf("[AUDIO] %s: %s\n", m.s, m.msg);
    }

    // Check for HTTP errors (404, 403, 500) with debouncing (must match HTTP status line, not random URLs/timestamps)
    static unsigned long lastFailoverMs = 0;
    if (m.msg && (strstr(m.msg, "HTTP/1.1 404") || strstr(m.msg, "HTTP/1.0 404") || 
                  strstr(m.msg, " 404 Not Found") || strstr(m.msg, " 404 not found") ||
                  strstr(m.msg, "HTTP/1.1 403") || strstr(m.msg, "HTTP/1.1 500") ||
                  strstr(m.msg, "HTTP/1.1 502") || strstr(m.msg, "HTTP/1.1 503"))) {
        Serial.printf("[AUDIO-ERR] HTTP stream error detected: %s\n", m.msg);
        // During an active failover sequence (cdnFailoverCount > 0), bypass debounce so all CDNs are tried immediately
        if (cdnFailoverCount > 0 || (millis() - lastFailoverMs > 2500)) {
            lastFailoverMs = millis();
            pendingCdnFailover = true;
        }
        return;
    }

    // Audio stream events or bitrate/sample-rate/sync confirms stream is actively decoding!
    // Note: Never match on raw HTTP headers like "audio/" or "video/mp2t" or ".ts" as they arrive before audio frames!
    bool isSyncOrData = false;
    if (m.e == Audio::evt_bitrate || m.e == Audio::evt_name) {
        isSyncOrData = true;
    } else if (m.e == Audio::evt_streamtitle && m.msg && !strstr(m.msg, "HTTP/")) {
        isSyncOrData = true;
    } else if (m.msg && (strstr(m.msg, "stream ready") || 
                         strstr(m.msg, "buffer filled") || 
                         strstr(m.msg, "Decoder has been initialized") || 
                         strstr(m.msg, "SampleRate") || 
                         strstr(m.msg, "Channels:") || 
                         strstr(m.msg, "BitRate") || 
                         strstr(m.msg, "format is") || 
                         strstr(m.msg, "Sync accepted"))) {
        isSyncOrData = true;
    }

    if (isSyncOrData) {
        if (isBuffering) {
            isBuffering = false;
            isPlaying = true;
            greenConfirmationUntilMs = millis() + 2000; // Green LED confirmation
            if (cdnFailoverCount > 0 && currentStationIndex >= 0 && currentStationIndex < (int)runtimeStations.size()) {
                runtimeStations[currentStationIndex].url = activeStreamUrl;
                pendingSaveFavorites = true;
                Serial.printf("[CDN-FAILOVER] Recovered and scheduled save %s -> %s\n",
                              runtimeStations[currentStationIndex].name.c_str(), activeStreamUrl.c_str());
            }
            cdnFailoverCount = 0;
            updateUI();
        }
    }

    if (m.e == Audio::evt_streamtitle && m.msg && strlen(m.msg) > 0 && !strstr(m.msg, "HTTP/")) {
        if (lblMeta) {
            lv_label_set_text(lblMeta, m.msg);
        }
    }

    if (m.e == Audio::evt_eof) {
        Serial.printf("[AUDIO] EOF or segment completed: %s\n", m.msg ? m.msg : "");
        // In HLS streaming (.m3u8), TS chunks end periodically. Audio::loop() fetches the next chunk automatically.
        // Never call playCurrentStation() on chunk EOF as it would abort the HLS playlist!
    }
}

/* -------------------------------------------------------------------------
 * NVS Favorites Storage Engine
 * ------------------------------------------------------------------------- */
void setStarterFavorites() {
    runtimeStations.clear();
    for (int i = 0; i < TOTAL_STARTER_STATIONS; i++) {
        RuntimeStation st;
        st.name     = STARTER_STATIONS[i].name;
        st.state    = STARTER_STATIONS[i].state;
        st.language = STARTER_STATIONS[i].language;
        st.url      = STARTER_STATIONS[i].url;
        runtimeStations.push_back(st);
    }
    saveFavorites();
}

void saveFavorites() {
    prefs.begin("fav_radio", false);
    prefs.putInt("fav_ver", 13);
    prefs.putInt("fav_cnt", (int)runtimeStations.size());
    for (size_t i = 0; i < runtimeStations.size(); i++) {
        char key[16];
        snprintf(key, sizeof(key), "fn_%u", (unsigned int)i);
        prefs.putString(key, runtimeStations[i].name);
        snprintf(key, sizeof(key), "fu_%u", (unsigned int)i);
        prefs.putString(key, runtimeStations[i].url);
        snprintf(key, sizeof(key), "fs_%u", (unsigned int)i);
        prefs.putString(key, runtimeStations[i].state);
        snprintf(key, sizeof(key), "fl_%u", (unsigned int)i);
        prefs.putString(key, runtimeStations[i].language);
    }
    prefs.end();
    Serial.printf("[NVS] Saved %d favorites to persistent storage\n", (int)runtimeStations.size());
}

void loadFavorites() {
    runtimeStations.clear();
    prefs.begin("fav_radio", false);
    int fav_ver = prefs.getInt("fav_ver", 0);
    // Purge outdated streams to ensure verified CloudFront mirrors (v13 - verified active distribution mirrors)
    if (fav_ver < 13) {
        prefs.clear();
        prefs.putInt("fav_ver", 13);
        prefs.end();
        Serial.println("[NVS] Migrating favorites to verified CloudFront mirrors (v13 active distributions)...");
        setStarterFavorites();
        return;
    }

    int cnt = prefs.getInt("fav_cnt", 0);
    if (cnt <= 0) {
        prefs.end();
        Serial.println("[NVS] No favorites found. Seeding Malayalam starter defaults.");
        setStarterFavorites();
        return;
    }

    if (cnt > 64) cnt = 64;
    bool hasBadUrl = false;
    for (int i = 0; i < cnt; i++) {
        char key[16];
        snprintf(key, sizeof(key), "fn_%u", (unsigned int)i);
        String name = prefs.getString(key, "");
        snprintf(key, sizeof(key), "fu_%u", (unsigned int)i);
        String url = prefs.getString(key, "");
        snprintf(key, sizeof(key), "fs_%u", (unsigned int)i);
        String state = prefs.getString(key, "General");
        snprintf(key, sizeof(key), "fl_%u", (unsigned int)i);
        String lang = prefs.getString(key, "General");

        if (url.startsWith("https://airhlspush") || url.indexOf("d1tmej9eu7kw5c.cloudfront.net/f70fdeca437dc326") >= 0 || url.length() < 10) {
            hasBadUrl = true;
        }

        if (name.length() > 0 && url.length() > 0) {
            RuntimeStation st;
            st.name = name;
            st.url = url;
            st.state = state;
            st.language = lang;
            runtimeStations.push_back(st);
        }
    }
    prefs.end();

    if (hasBadUrl || runtimeStations.empty()) {
        Serial.println("[NVS] Found invalid/legacy URLs. Reseeding with verified Malayalam stations.");
        setStarterFavorites();
        return;
    }

    Serial.printf("[NVS] Loaded %d favorites from persistent storage\n", (int)runtimeStations.size());
}

void addFavorite(const RuntimeStation& st, bool playNow) {
    // Check duplicate
    for (size_t i = 0; i < runtimeStations.size(); i++) {
        if (runtimeStations[i].url.equalsIgnoreCase(st.url)) {
            Serial.println("[FAV] Station already in favorites");
            if (playNow) {
                currentStationIndex = (int)i;
                playCurrentStation();
            }
            return;
        }
    }

    runtimeStations.push_back(st);
    saveFavorites();
    if (playNow) {
        currentStationIndex = (int)runtimeStations.size() - 1;
        playCurrentStation();
    } else {
        updateUI();
    }
}

void deleteFavorite(int idx) {
    if (idx < 0 || idx >= (int)runtimeStations.size()) return;
    bool wasPlaying = (currentStationIndex == idx && isPlaying);
    runtimeStations.erase(runtimeStations.begin() + idx);
    if (runtimeStations.empty()) {
        setStarterFavorites();
    } else {
        saveFavorites();
    }

    if (currentStationIndex >= (int)runtimeStations.size()) {
        currentStationIndex = (int)runtimeStations.size() - 1;
    }
    if (wasPlaying) {
        playCurrentStation();
    } else {
        updateUI();
    }
}

/* -------------------------------------------------------------------------
 * Auto-On Alarm & Sleep Timer Persistent Settings (NVS)
 * ------------------------------------------------------------------------- */
void saveTimerSettings() {
    prefs.begin("crow_timer", false);
    prefs.putBool("en", timerEnabled);
    prefs.putInt("h", timerHour);
    prefs.putInt("m", timerMin);
    prefs.putInt("dur", timerDuration);
    prefs.end();
    Serial.printf("[TIMER] Saved to NVS: Enabled=%s, Time=%02d:%02d, Duration=%d min\n",
                  timerEnabled ? "YES" : "NO", timerHour, timerMin, timerDuration);
}

void loadTimerSettings() {
    prefs.begin("crow_timer", false);
    timerEnabled = prefs.getBool("en", false);
    timerHour = prefs.getInt("h", 6);
    timerMin = prefs.getInt("m", 30);
    timerDuration = prefs.getInt("dur", 30);
    bool alarmBootFlag = prefs.getBool("alarm_boot", false);
    if (alarmBootFlag) {
        bootFromAlarm = true;
        prefs.putBool("alarm_boot", false); // Clear alarm boot flag
    }
    prefs.end();
    Serial.printf("[TIMER] Loaded from NVS: Enabled=%s, Time=%02d:%02d, Duration=%d min (BootFromAlarm=%s)\n",
                  timerEnabled ? "YES" : "NO", timerHour, timerMin, timerDuration,
                  bootFromAlarm ? "YES" : "NO");
}

/* -------------------------------------------------------------------------
 * Ambient Lighting Engine (5x WS2812B NeoPixels)
 * ------------------------------------------------------------------------- */
void updateAmbientLeds() {
    if (pwrCountdownActive) return; // Do not overwrite power-off countdown LED illumination

    static unsigned long lastUpdate = 0;
    unsigned long now = millis();
    if (now - lastUpdate < 40) return;
    lastUpdate = now;

    uint8_t scaledBright = (uint8_t)((currentLedBrightness * 255) / 100);
    if (scaledBright < 40) scaledBright = 60; // Ensure visible vibrant feedback
    strip.setBrightness(scaledBright);

    // 1. Rotation Chase Feedback (Knob turned)
    if (ledChaseDir != 0 && (now - lastChaseMs < 600)) {
        static int chaseIndex = 0;
        strip.clear();
        int ledIdx = (chaseIndex % NEOPIXEL_COUNT + NEOPIXEL_COUNT) % NEOPIXEL_COUNT;
        strip.setPixelColor(ledIdx, strip.Color(31, 111, 235)); // Cyan / Blue pulse
        strip.show();
        chaseIndex += ledChaseDir;
        return;
    } else {
        ledChaseDir = 0;
    }

    // 2. Volume Meter Mode (When adjusting volume knob)
    if (currentKnobMode == KNOB_MODE_VOL) {
        strip.clear();
        int activeLeds = (currentVolume * NEOPIXEL_COUNT + 10) / 21;
        for (int i = 0; i < NEOPIXEL_COUNT; i++) {
            if (i < activeLeds) {
                if (i <= 2) strip.setPixelColor(i, strip.Color(46, 160, 67));   // Green
                else if (i == 3) strip.setPixelColor(i, strip.Color(210, 153, 34)); // Yellow
                else strip.setPixelColor(i, strip.Color(218, 54, 51));          // Red
            }
        }
        strip.show();
        return;
    }

    // 2.5 URL Failure / Offline Alert -> Warning pulsing red LEDs
    if (isStreamError && (now < streamErrorUntilMs)) {
        static float errPhase = 0;
        errPhase += 0.25f;
        uint8_t rAmp = (uint8_t)(160 + 95 * sin(errPhase));
        for (int i = 0; i < NEOPIXEL_COUNT; i++) {
            strip.setPixelColor(i, strip.Color(rAmp, 0, 0)); // Warning pulsing crimson red
        }
        strip.show();
        return;
    }

    // 3. Station Loading / Buffering -> Vibrant pulsing yellow
    // "Whne The Station is Loadinf the buffering is yellow, let the LED tooo be in yellow at taht time"
    if (isBuffering) {
        static float bPhase = 0;
        bPhase += 0.25f;
        uint8_t amp = (uint8_t)(160 + 95 * sin(bPhase)); // Smooth pulsing yellow
        for (int i = 0; i < NEOPIXEL_COUNT; i++) {
            strip.setPixelColor(i, strip.Color(amp, (amp * 19) / 25, 0)); // Pure bright golden-yellow
        }
        strip.show();
        return;
    }

    // 4. Station Starts Playing -> Green for a moment!
    // "when it starts playing, the UI is green, let LED be greean for amoment"
    if (now < greenConfirmationUntilMs) {
        for (int i = 0; i < NEOPIXEL_COUNT; i++) {
            strip.setPixelColor(i, strip.Color(0, 240, 30)); // Bright emerald green confirmation
        }
        strip.show();
        return;
    }

    // 5. Playing Continues -> Turn LEDs off so it doesn't disturb user
    // "then when coninue playing LED can be off in the User"
    if (isPlaying) {
        strip.clear();
        strip.show();
        return;
    }

    // Default Idle (Paused / Off)
    strip.clear();
    strip.show();
}

/* -------------------------------------------------------------------------
 * Web Server Handlers
 * ------------------------------------------------------------------------- */
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

static esp_err_t http_root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, PAGE_INDEX, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_status_handler(httpd_req_t *req) {
    const char* title = "";
    const char* state = "";
    const char* lang  = "";
    int statusIdx = currentStationIndex;

    if (isAdHocPlaying) {
        title = adHocStation.name.c_str();
        state = adHocStation.state.c_str();
        lang  = adHocStation.language.c_str();
        statusIdx = -1;
    } else if (!runtimeStations.empty() && currentStationIndex >= 0 && currentStationIndex < (int)runtimeStations.size()) {
        title = runtimeStations[currentStationIndex].name.c_str();
        state = runtimeStations[currentStationIndex].state.c_str();
        lang  = runtimeStations[currentStationIndex].language.c_str();
    }

    const char* scrName = "launcher";
    if (currentScreen == SCREEN_RADIO) scrName = "radio";
    else if (currentScreen == SCREEN_CLOCK) scrName = "clock";
    else if (currentScreen == SCREEN_SETTINGS) scrName = "settings";
    else if (currentScreen == SCREEN_WIFI_SCAN) scrName = "wifi_scan";
    else if (currentScreen == SCREEN_WIFI_KEYPAD) scrName = "wifi_keypad";

    char buf[700];
    snprintf(buf, sizeof(buf),
             "{\"playing\":%s,\"buffering\":%s,\"idx\":%d,\"title\":\"%s\",\"state\":\"%s\",\"lang\":\"%s\",\"vol\":%d,\"muted\":%s,\"total\":%d,\"sta_ip\":\"%s\",\"led_mode\":%d,\"led_bright\":%d,\"screen\":\"%s\",\"clock_face\":%d,\"screensaver_sec\":%d,\"brightness\":%d,\"stream_error\":%s,\"error_msg\":\"%s\"}",
             isPlaying ? "true" : "false",
             isBuffering ? "true" : "false",
             statusIdx,
             title, state, lang,
             currentVolume,
             isMuted ? "true" : "false",
             (int)runtimeStations.size(),
             (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString().c_str() : "Disconnected",
             currentLedMode,
             currentLedBrightness,
             scrName,
             currentClockFace,
             screensaverTimeoutSec,
             currentBacklightBrightness,
             (isStreamError && millis() < streamErrorUntilMs) ? "true" : "false",
             (isStreamError && millis() < streamErrorUntilMs) ? streamErrorReason.c_str() : "");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

static int parseQueryParamInt(const char* q, const char* paramName) {
    if (!q || !paramName) return -1;
    size_t nameLen = strlen(paramName);
    const char* p = q;
    while ((p = strstr(p, paramName)) != NULL) {
        if ((p == q || *(p - 1) == '?' || *(p - 1) == '&') && *(p + nameLen) == '=') {
            return atoi(p + nameLen + 1);
        }
        p += nameLen;
    }
    return -1;
}

static esp_err_t http_cmd_handler(httpd_req_t *req) {
    char query[128] = {0};
    bool hasQuery = (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK);
    const char* fullUri = req->uri ? req->uri : "";

    if (hasQuery || strchr(fullUri, '?')) {
        char act[32] = {0};
        if (hasQuery) {
            httpd_query_key_value(query, "action", act, sizeof(act));
        }

        if (strcmp(act, "play_pause") == 0 || strstr(query, "action=play_pause") || strstr(fullUri, "action=play_pause")) pendingWebAction = ACT_PLAY_PAUSE;
        else if (strcmp(act, "next") == 0 || strstr(query, "action=next") || strstr(fullUri, "action=next")) pendingWebAction = ACT_NEXT;
        else if (strcmp(act, "prev") == 0 || strstr(query, "action=prev") || strstr(fullUri, "action=prev")) pendingWebAction = ACT_PREV;
        else if (strcmp(act, "mute") == 0 || strstr(query, "action=mute") || strstr(fullUri, "action=mute")) toggleMute();
        else if (strcmp(act, "vol") == 0 || strstr(query, "action=vol") || strstr(fullUri, "action=vol")) {
            int v = parseQueryParamInt(query, "val");
            if (v < 0) v = parseQueryParamInt(fullUri, "val");
            if (v >= 0) pendingVol = v;
        } else if (strcmp(act, "station") == 0 || strstr(query, "action=station") || strstr(fullUri, "action=station")) {
            int sid = parseQueryParamInt(query, "id");
            if (sid < 0) sid = parseQueryParamInt(query, "idx");
            if (sid < 0) sid = parseQueryParamInt(fullUri, "id");
            if (sid < 0) sid = parseQueryParamInt(fullUri, "idx");
            if (sid >= 0) {
                pendingStationId = sid;
                pendingWebAction = ACT_PLAY_STATION;
                Serial.printf("[HTTPD] Station command request -> idx %d\n", sid);
            }
        } else if (strcmp(act, "screen") == 0 || strstr(fullUri, "action=screen")) {
            char to[16] = {0};
            if (hasQuery) httpd_query_key_value(query, "to", to, sizeof(to));
            if (strcmp(to, "launcher") == 0 || strstr(fullUri, "to=launcher") || strstr(fullUri, "to=home")) switchScreen(SCREEN_HOME_LAUNCHER);
            else if (strcmp(to, "radio") == 0 || strstr(fullUri, "to=radio")) switchScreen(SCREEN_RADIO);
            else if (strcmp(to, "clock") == 0 || strstr(fullUri, "to=clock")) switchScreen(SCREEN_CLOCK);
            else if (strcmp(to, "settings") == 0 || strstr(fullUri, "to=settings")) switchScreen(SCREEN_SETTINGS);
        } else if (strcmp(act, "clock_face") == 0 || strstr(fullUri, "action=clock_face")) {
            cycleClockFace();
        } else if (strcmp(act, "screensaver") == 0 || strstr(fullUri, "action=screensaver")) {
            cycleScreensaverTimeout();
        } else if (strcmp(act, "bright") == 0 || strstr(fullUri, "action=bright")) {
            cycleBrightness();
        }
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_favorites_get_handler(httpd_req_t *req) {
    String json = "[";
    for (size_t i = 0; i < runtimeStations.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"id\":" + String((int)i) +
                ",\"name\":\"" + escapeJson(runtimeStations[i].name) + "\"" +
                ",\"url\":\"" + escapeJson(runtimeStations[i].url) + "\"" +
                ",\"state\":\"" + escapeJson(runtimeStations[i].state) + "\"" +
                ",\"lang\":\"" + escapeJson(runtimeStations[i].language) + "\"}";
    }
    json += "]";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json.c_str(), json.length());
}

static esp_err_t http_favorites_delete_handler(httpd_req_t *req) {
    char query[64] = {0};
    int delId = -1;
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char idx_str[16] = {0};
        if (httpd_query_key_value(query, "id", idx_str, sizeof(idx_str)) == ESP_OK) {
            delId = atoi(idx_str);
        }
    }
    if (delId >= 0) {
        pendingDeleteStationIdx = delId;
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"status\":\"error\",\"msg\":\"Missing id\"}", HTTPD_RESP_USE_STRLEN);
}

static void url_decode_str(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit((int)a) && isxdigit((int)b))) {
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

static esp_err_t http_favorites_add_handler(httpd_req_t *req) {
    char post_buf[512] = {0};
    int remaining = req->content_len;
    int received = 0;
    while (remaining > 0 && received < (int)sizeof(post_buf) - 1) {
        int max_chunk = (int)sizeof(post_buf) - 1 - received;
        int to_recv = (remaining < max_chunk) ? remaining : max_chunk;
        int ret = httpd_req_recv(req, post_buf + received, to_recv);
        if (ret <= 0) break;
        received += ret;
        remaining -= ret;
    }
    post_buf[received] = '\0';

    char raw_name[96] = {0}, raw_url[256] = {0}, raw_state[64] = {0}, raw_lang[64] = {0};
    char raw_play[16] = {0};

    char query[256] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "name", raw_name, sizeof(raw_name));
        httpd_query_key_value(query, "url", raw_url, sizeof(raw_url));
        httpd_query_key_value(query, "state", raw_state, sizeof(raw_state));
        httpd_query_key_value(query, "lang", raw_lang, sizeof(raw_lang));
        httpd_query_key_value(query, "play", raw_play, sizeof(raw_play));
    }
    if (received > 0) {
        if (raw_name[0] == '\0')  httpd_query_key_value(post_buf, "name", raw_name, sizeof(raw_name));
        if (raw_url[0] == '\0')   httpd_query_key_value(post_buf, "url", raw_url, sizeof(raw_url));
        if (raw_state[0] == '\0') httpd_query_key_value(post_buf, "state", raw_state, sizeof(raw_state));
        if (raw_lang[0] == '\0')  httpd_query_key_value(post_buf, "lang", raw_lang, sizeof(raw_lang));
        if (raw_play[0] == '\0')  httpd_query_key_value(post_buf, "play", raw_play, sizeof(raw_play));
    }

    url_decode_str(pendingAddStation.name, raw_name);
    url_decode_str(pendingAddStation.url, raw_url);
    url_decode_str(pendingAddStation.state, raw_state);
    url_decode_str(pendingAddStation.lang, raw_lang);
    pendingAddStation.playNow = (atoi(raw_play) != 0);

    if (strlen(pendingAddStation.name) > 0 && strlen(pendingAddStation.url) > 0) {
        pendingAddStation.pending = true;
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"status\":\"error\",\"msg\":\"Missing name or url\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_play_handler(httpd_req_t *req) {
    char query[512] = {0};
    char post_buf[512] = {0};
    int foundId = -1;
    char raw_url[256] = {0};
    char raw_name[64] = {0};
    char raw_state[48] = {0};
    char raw_lang[32] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        foundId = parseQueryParamInt(query, "id");
        if (foundId < 0) foundId = parseQueryParamInt(query, "idx");
        httpd_query_key_value(query, "url", raw_url, sizeof(raw_url));
        httpd_query_key_value(query, "name", raw_name, sizeof(raw_name));
        httpd_query_key_value(query, "state", raw_state, sizeof(raw_state));
        httpd_query_key_value(query, "lang", raw_lang, sizeof(raw_lang));
    }
    if (foundId < 0 && req->uri) {
        foundId = parseQueryParamInt(req->uri, "id");
        if (foundId < 0) foundId = parseQueryParamInt(req->uri, "idx");
    }

    int remaining = req->content_len;
    if (remaining > 0 && remaining < (int)sizeof(post_buf)) {
        int received = httpd_req_recv(req, post_buf, remaining);
        if (received > 0) {
            post_buf[received] = '\0';
            if (raw_url[0] == '\0')   httpd_query_key_value(post_buf, "url", raw_url, sizeof(raw_url));
            if (raw_name[0] == '\0')  httpd_query_key_value(post_buf, "name", raw_name, sizeof(raw_name));
            if (raw_state[0] == '\0') httpd_query_key_value(post_buf, "state", raw_state, sizeof(raw_state));
            if (raw_lang[0] == '\0')  httpd_query_key_value(post_buf, "lang", raw_lang, sizeof(raw_lang));
        }
    }

    if (raw_url[0] != '\0') {
        url_decode_str(pendingDirectPlay.url, raw_url);
        url_decode_str(pendingDirectPlay.name, (raw_name[0] != '\0') ? raw_name : "Online Stream");
        url_decode_str(pendingDirectPlay.state, raw_state);
        url_decode_str(pendingDirectPlay.lang, raw_lang);
        pendingDirectPlay.pending = true;
        Serial.printf("[HTTPD] Direct play stream request: %s (%s)\n", pendingDirectPlay.name, pendingDirectPlay.url);
    } else if (foundId >= 0) {
        pendingStationId = foundId;
        pendingWebAction = ACT_PLAY_STATION;
        Serial.printf("[HTTPD] Play station request -> idx %d\n", foundId);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_timer_handler(httpd_req_t *req) {
    char query[128] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char en_str[8] = {0}, h_str[8] = {0}, m_str[8] = {0}, dur_str[8] = {0};
        if (httpd_query_key_value(query, "en", en_str, sizeof(en_str)) == ESP_OK) {
            pendingTimerSave.en = (atoi(en_str) == 1);
            if (httpd_query_key_value(query, "h", h_str, sizeof(h_str)) == ESP_OK) pendingTimerSave.h = atoi(h_str);
            if (httpd_query_key_value(query, "m", m_str, sizeof(m_str)) == ESP_OK) pendingTimerSave.m = atoi(m_str);
            if (httpd_query_key_value(query, "dur", dur_str, sizeof(dur_str)) == ESP_OK) pendingTimerSave.dur = atoi(dur_str);
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

static esp_err_t http_led_handler(httpd_req_t *req) {
    char query[64] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char m_str[16] = {0}, b_str[16] = {0};
        if (httpd_query_key_value(query, "mode", m_str, sizeof(m_str)) == ESP_OK) {
            currentLedMode = atoi(m_str);
        }
        if (httpd_query_key_value(query, "bright", b_str, sizeof(b_str)) == ESP_OK) {
            currentLedBrightness = atoi(b_str);
        }
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_wifi_handler(httpd_req_t *req) {
    char query[160] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char s_str[64] = {0}, p_str[64] = {0};
        httpd_query_key_value(query, "ssid", s_str, sizeof(s_str));
        httpd_query_key_value(query, "pass", p_str, sizeof(p_str));
        url_decode_str(pendingWifiSave.ssid, s_str);
        url_decode_str(pendingWifiSave.pass, p_str);
        pendingWifiSave.pending = true;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_wifi_scan_handler(httpd_req_t *req) {
    Serial.println("[WIFI] Scanning 2.4GHz Wi-Fi networks...");
    int n = WiFi.scanNetworks(false, false);
    String json = "[";
    if (n > 0) {
        int count = 0;
        for (int i = 0; i < n && count < 25; i++) {
            String s = WiFi.SSID(i);
            if (s.length() == 0) continue;
            if (count > 0) json += ",";
            json += "{\"ssid\":\"";
            for (size_t c = 0; c < s.length(); c++) {
                if (s[c] == '"' || s[c] == '\\') json += '\\';
                json += s[c];
            }
            json += "\",\"rssi\":";
            json += String(WiFi.RSSI(i));
            json += ",\"secure\":";
            json += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "0" : "1";
            json += "}";
            count++;
        }
    }
    json += "]";
    WiFi.scanDelete();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json.c_str(), HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_debug_handler(httpd_req_t *req) {
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"running\":%s,\"buffering\":%s,\"playing\":%s,\"sr\":%u,\"br\":%u,\"in_buff\":%u,\"cur_time\":%u,\"idx\":%d,\"url\":\"%s\",\"failover_cnt\":%d,\"heap\":%u,\"psram\":%u}",
             audioIsRunning ? "true" : "false",
             isBuffering ? "true" : "false",
             isPlaying ? "true" : "false",
             audioSampleRate,
             audioBitRate,
             audioInBuffer,
             audioCurrentTime,
             currentStationIndex,
             activeStreamUrl.c_str(),
             cdnFailoverCount,
             ESP.getFreeHeap(),
             ESP.getFreePsram());
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

void setupWebServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    config.core_id = 1;
    config.task_priority = 4;

    if (httpd_start(&httpServer, &config) == ESP_OK) {
        httpd_uri_t uri_root = { "/", HTTP_GET, http_root_handler, NULL };
        httpd_register_uri_handler(httpServer, &uri_root);

        httpd_uri_t uri_status = { "/api/status", HTTP_GET, http_status_handler, NULL };
        httpd_register_uri_handler(httpServer, &uri_status);

        httpd_uri_t uri_debug = { "/api/debug", HTTP_GET, http_debug_handler, NULL };
        httpd_register_uri_handler(httpServer, &uri_debug);

        httpd_uri_t uri_cmd = { "/api/cmd", HTTP_GET, http_cmd_handler, NULL };
        httpd_register_uri_handler(httpServer, &uri_cmd);

        httpd_uri_t uri_fav_get = { "/api/favorites", HTTP_GET, http_favorites_get_handler, NULL };
        httpd_register_uri_handler(httpServer, &uri_fav_get);

        httpd_uri_t uri_fav_add = { "/api/favorites/add", HTTP_POST, http_favorites_add_handler, NULL };
        httpd_register_uri_handler(httpServer, &uri_fav_add);

        httpd_uri_t uri_fav_del = { "/api/favorites/delete", HTTP_POST, http_favorites_delete_handler, NULL };
        httpd_register_uri_handler(httpServer, &uri_fav_del);

        httpd_uri_t uri_play = { "/api/play", HTTP_GET, http_play_handler, NULL };
        httpd_register_uri_handler(httpServer, &uri_play);

        httpd_uri_t uri_play_post = { "/api/play", HTTP_POST, http_play_handler, NULL };
        httpd_register_uri_handler(httpServer, &uri_play_post);

        httpd_uri_t uri_timer = { "/api/timer", HTTP_GET, http_timer_handler, NULL };
        httpd_register_uri_handler(httpServer, &uri_timer);

        httpd_uri_t uri_led = { "/api/led", HTTP_GET, http_led_handler, NULL };
        httpd_register_uri_handler(httpServer, &uri_led);

        httpd_uri_t uri_wifi = { "/api/wifi", HTTP_GET, http_wifi_handler, NULL };
        httpd_register_uri_handler(httpServer, &uri_wifi);

        httpd_uri_t uri_wifi_scan = { "/api/wifi/scan", HTTP_GET, http_wifi_scan_handler, NULL };
        httpd_register_uri_handler(httpServer, &uri_wifi_scan);

        Serial.println("[HTTPD] Web Remote Server started successfully on port 80");
    }
}

/* -------------------------------------------------------------------------
 * Professional Minimalist Circular Loader Renderer
 * Clean border progress bar with centered loading information only
 * ------------------------------------------------------------------------- */
void renderProfessionalLoaderFrame(int progress, const char* titleMsg, const char* detailMsg) {
    if (progress < 0) progress = 0;
    if (progress > 100) progress = 100;

    // 1. Deep OLED Obsidian Space Black Background
    loaderSprite.fillScreen(0x0000);

    // 2. Outer Circular Bezel Track (radius 108 to 113)
    loaderSprite.fillArc(120, 120, 113, 108, 0, 360, 0x18C3);

    // Subtle 12-hour tick marks around the inner edge of the bezel for a precision instrument feel
    for (int deg = 0; deg < 360; deg += 30) {
        float rad = deg * (PI / 180.0f);
        int x_in = 120 + (int)(105.0f * cosf(rad));
        int y_in = 120 + (int)(105.0f * sinf(rad));
        int x_out = 120 + (int)(108.0f * cosf(rad));
        int y_out = 120 + (int)(108.0f * sinf(rad));
        loaderSprite.drawLine(x_in, y_in, x_out, y_out, 0x2965);
    }

    // 3. Active Precision Progress Arc (Sweeping clockwise from 12 o'clock / 270 deg)
    float startDeg = 270.0f;
    float sweepDeg = progress * 3.6f;
    if (sweepDeg > 360.0f) sweepDeg = 360.0f;
    if (sweepDeg > 1.0f) {
        // Luxury warm solar gold arc
        loaderSprite.fillArc(120, 120, 113, 108, startDeg, startDeg + sweepDeg, 0xFDC0);

        // Leading edge precision bead
        float leadRad = (startDeg + sweepDeg) * (PI / 180.0f);
        int leadX = 120 + (int)(110.5f * cosf(leadRad));
        int leadY = 120 + (int)(110.5f * sinf(leadRad));
        loaderSprite.fillCircle(leadX, leadY, 3, 0xFFFF);
    }

    // 4. Center Loading Information (NO radial rays, clean typography)
    // 4A. Brand Header at Top Center (Y = 38)
    loaderSprite.setFont(&fonts::Font2);
    loaderSprite.setTextDatum(textdatum_t::top_center);
    loaderSprite.setTextColor(0xD586, 0x0000); // Warm metallic gold
    loaderSprite.drawString("MALAYALAM AIR", 120, 38);

    // Subtle divider line
    loaderSprite.drawFastHLine(85, 58, 70, 0x2965);

    // 4B. Large Clean Numeric Percentage (Y = 94)
    loaderSprite.setFont(&fonts::Font4);
    loaderSprite.setTextDatum(textdatum_t::middle_center);
    loaderSprite.setTextColor(0xFFFF, 0x0000);
    char pctStr[16];
    snprintf(pctStr, sizeof(pctStr), "%d%%", progress);
    loaderSprite.drawString(pctStr, 120, 94);

    // 4C. Step Title (Y = 132)
    loaderSprite.setFont(&fonts::Font2);
    loaderSprite.setTextDatum(textdatum_t::middle_center);
    loaderSprite.setTextColor(0xFDC0, 0x0000); // Solar amber gold
    loaderSprite.drawString(titleMsg ? titleMsg : "", 120, 132);

    // 4D. Sub-Detail line (Y = 158)
    loaderSprite.setFont(&fonts::Font2);
    loaderSprite.setTextDatum(textdatum_t::middle_center);
    loaderSprite.setTextColor(0x8410, 0x0000); // Soft titanium grey
    loaderSprite.drawString(detailMsg ? detailMsg : "", 120, 158);

    // 4E. 5-Stage Minimalist Pill Indicators (Y = 186)
    int currentStage = 1;
    if (progress >= 95) currentStage = 5;
    else if (progress >= 80) currentStage = 4;
    else if (progress >= 50) currentStage = 3;
    else if (progress >= 25) currentStage = 2;
    else currentStage = 1;

    const int dotCount = 5;
    const int dotSpacing = 14;
    const int startX = 120 - ((dotCount - 1) * dotSpacing) / 2;
    for (int d = 0; d < dotCount; d++) {
        int dotX = startX + d * dotSpacing;
        if (d + 1 <= currentStage) {
            loaderSprite.fillCircle(dotX, 186, 3, 0xFDC0); // Active: gold
        } else {
            loaderSprite.fillCircle(dotX, 186, 2, 0x2965); // Inactive: dark slate
        }
    }

    // 5. Push Off-screen Buffer to Display via DMA
    loaderSprite.pushSprite(0, 0);

    // 6. Dynamic Ambient LED Shift: Red -> Blue -> Green as system loads (0% -> 50% -> 100%)
    // "Let the LEDs change ambience from Red to Green through blue as the system loader loads."
    uint8_t r = 0, g = 0, b = 0;
    if (progress <= 50) {
        float t = progress / 50.0f; // 0.0 to 1.0 (Red to Blue transition)
        r = (uint8_t)(255.0f * (1.0f - t));
        g = 0;
        b = (uint8_t)(255.0f * t);
    } else {
        float t = (progress - 50) / 50.0f; // 0.0 to 1.0 (Blue to Green transition)
        r = 0;
        g = (uint8_t)(255.0f * t);
        b = (uint8_t)(255.0f * (1.0f - t));
    }

    int numLeds = strip.numPixels();
    int activeLeds = (progress * numLeds + 50) / 100;
    if (activeLeds > numLeds) activeLeds = numLeds;
    if (activeLeds < 1 && progress > 0) activeLeds = 1;

    for (int p = 0; p < numLeds; p++) {
        if (p < activeLeds) {
            strip.setPixelColor(p, strip.Color(r, g, b));
        } else {
            // Subtle ambient floor of current transition tone
            strip.setPixelColor(p, strip.Color(r / 20, g / 20, b / 20));
        }
    }
    strip.show();
}

/* -------------------------------------------------------------------------
 * Arduino Setup
 * ------------------------------------------------------------------------- */
void setup() {
    Serial.begin(115200);
    delay(100);


    // Check if this boot is from an Auto-On Alarm
    prefs.begin("crow_timer", false);
    bool alarmBootFlag = prefs.getBool("alarm_boot", false);
    if (alarmBootFlag) {
        bootFromAlarm = true;
        prefs.putBool("alarm_boot", false);
    }
    prefs.end();

    // Check if this boot was explicitly triggered by user power-on wake
    prefs.begin("crow_pwr", false);
    bool wakeBoot = prefs.getBool("wake_boot", false);
    if (wakeBoot) {
        prefs.putBool("wake_boot", false);
    }
    prefs.end();

    // If power was just applied (cold boot) AND not triggered by wake_boot or alarm:
    if (!wakeBoot && !bootFromAlarm) {
        Serial.println("\n========================================================");
        Serial.println("  EDIFIER 1.28\" ESP32-S3 HMI Studio Radio");
        Serial.println("  [STANDBY] Cold Power-ON detected -> Starting in STANDBY.");
        Serial.println("  [STANDBY] Ring LEDs OFF, Board Power LED RED.");
        Serial.println("  [STANDBY] Hold knob push-button for 4s to power ON.");
        Serial.println("========================================================\n");

        // Keep hardware rails, display, backlight, and green LED powered OFF
        pinMode(PIN_PWR_LED, OUTPUT);
        digitalWrite(PIN_PWR_LED, LOW);
        pinMode(PIN_PWR_EN1, OUTPUT);
        digitalWrite(PIN_PWR_EN1, HIGH); // Power rail for NeoPixel control
        pinMode(PIN_PWR_EN2, OUTPUT);
        digitalWrite(PIN_PWR_EN2, HIGH);

        pinMode(LCD_BL_PIN, OUTPUT);
        digitalWrite(LCD_BL_PIN, LOW);

        pinMode(ENCODER_SW_PIN, INPUT_PULLUP);

        // Load persistent Auto-On Alarm settings (so alarm can wake device if scheduled)
        loadTimerSettings();

        // Ensure NeoPixels are completely OFF during standby (only board red power LED stays on)
        strip.begin();
        strip.clear();
        strip.show();

        // Wait for switch release if button was held during power insertion
        while (digitalRead(ENCODER_SW_PIN) == LOW) {
            delay(30);
        }
        delay(100);

        // Enter low-power Standby Sleep Loop!
        // When user holds knob button for 4s, runStandbySleepLoop returns directly!
        runStandbySleepLoop(true);
    }

    Serial.println("\n\n========================================================");
    Serial.println("  EDIFIER 1.28\" ESP32-S3 HMI Studio Radio Starting... ");
    Serial.println("  A Passion for Sound | Malayalam Air Edition");
    Serial.println("========================================================");

    // 1. Energize power rails (CRUCIAL for display and peripherals!)
    pinMode(PIN_PWR_EN1, OUTPUT);
    digitalWrite(PIN_PWR_EN1, HIGH);
    pinMode(PIN_PWR_EN2, OUTPUT);
    digitalWrite(PIN_PWR_EN2, HIGH);

    pinMode(PIN_PWR_LED, OUTPUT);
    digitalWrite(PIN_PWR_LED, HIGH); // Green power LED ON

    // 2. Load User Preferences & Hardware Backlight PWM
    prefs.begin("crow_settings", false);
    screensaverTimeoutSec = prefs.getInt("screensaver_sec", 60);
    currentClockFace = prefs.getInt("clock_face", 0);
    currentBacklightBrightness = prefs.getInt("bl_bright", 70);
    currentLedMode = prefs.getInt("led_mode", LED_MODE_BREATHE);
    currentLedBrightness = prefs.getInt("led_bright", 30);
    currentStationIndex = prefs.getInt("last_station", 0);
    prefs.end();

    // Load persistent Auto-On Alarm & Sleep Timer settings
    loadTimerSettings();

    pinMode(LCD_BL_PIN, OUTPUT);
    ledcAttach(LCD_BL_PIN, 5000, 8);
    ledcWrite(LCD_BL_PIN, (currentBacklightBrightness * 255) / 100);

    // 3. Initialize CST816D Touch Controller
    touch.begin();

    // 4. Initialize LovyanGFX Display
    gfx.init();
    gfx.initDMA();

    // 5. Initialize Rotary Encoder & Center Switch (Queue & FreeRTOS Core 0 Task)
    encoderActionQueue = xQueueCreate(32, sizeof(EncoderAction));
    pinMode(ENCODER_A_PIN, INPUT_PULLUP);
    pinMode(ENCODER_B_PIN, INPUT_PULLUP);
    pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
    xTaskCreatePinnedToCore(encTask, "encTask", 2048, NULL, 3, NULL, 0);

    // 6. Initialize NeoPixel Ambient Light Ring
    strip.begin();
    strip.setBrightness((currentLedBrightness * 255) / 100);
    strip.clear();
    strip.show();

    // 7. Launch Professional Circular Loader
    loaderSprite.setPsram(true);
    loaderSprite.createSprite(240, 240);

    int progress = 0;
    auto advanceLoader = [&](int targetProgress, const char* title, const char* detail, int stepDelayMs) {
        while (progress < targetProgress) {
            progress++;
            renderProfessionalLoaderFrame(progress, title, detail);
            delay(stepDelayMs);
        }
    };

    advanceLoader(15, "INITIALIZING HARDWARE", "Display, Touch & Encoder", 12);

    // 8. Initialize LVGL 8.4
    lv_init();

    // High-Resolution Hardware Periodic Tick Timer (5ms)
    const esp_timer_create_args_t lv_tick_timer_args = {
        .callback = &lv_tick_task,
        .name = "lv_tick"
    };
    esp_timer_handle_t lv_tick_timer = NULL;
    esp_timer_create(&lv_tick_timer_args, &lv_tick_timer);
    esp_timer_start_periodic(lv_tick_timer, 5000);

    // Allocate draw buffers in external SPIRAM to preserve internal SRAM for networking/TLS
    size_t buf_pixels = 240 * 20;
    disp_buf1 = (lv_color_t*)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    disp_buf2 = (lv_color_t*)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!disp_buf1) disp_buf1 = (lv_color_t*)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!disp_buf2) disp_buf2 = (lv_color_t*)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

    lv_disp_draw_buf_init(&draw_buf, disp_buf1, disp_buf2, buf_pixels);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // 9. Initialize Keypad & Multi-Screen UI
    initKeypadEntries();
    createCircularUI();
    lv_timer_create(clockTimerCb, 1000, NULL); // 1-second watch face timer

    advanceLoader(30, "SYSTEM INITIALIZED", "LVGL 8.4 Framework Ready", 10);

    // 10. Load Persistent Favorites (NVS) & Seed Malayalam Stations
    advanceLoader(40, "READING STATIONS", "Accessing Persistent NVS", 10);
    loadFavorites();
    if (currentStationIndex < 0 || currentStationIndex >= (int)runtimeStations.size()) {
        currentStationIndex = 0;
    }
    advanceLoader(50, "FAVORITES LOADED", "10 Malayalam Stations", 10);

    // 11. Configure Audio Pipeline (I2S Output on Expansion Pins & Core 0 Task)
    Audio::audio_info_callback = my_audio_info;
    audio.setPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DOUT_PIN);
    audio.settings.BUFFER_TRESHOLD_HLS = 98304; // 96 KB prebuffer for HLS (6-8s resilience against network jitter)
    audio.setConnectionTimeout(1200, 2500);

    audioCmdQueue = xQueueCreate(16, sizeof(AudioCommand));
    sendAudioSetVolume(currentVolume);
    xTaskCreatePinnedToCore(audioTask, "audioTask", 16384, NULL, 3, NULL, 0);

    advanceLoader(60, "AUDIO READY", "I2S Core 0 Engine Active", 10);

    // 12. Dual-Mode WiFi: SoftAP + Station
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    dnsServer.start(53, "*", WiFi.softAPIP());
    Serial.printf("[WIFI] SoftAP Active: %s | IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

    // Check stored WiFi credentials
    prefs.begin("crow_wifi", false);
    wifiSsid = prefs.getString("ssid", "suresh2.4gExt");
    wifiPass = prefs.getString("pass", "alangium");
    if (wifiSsid == "suresh" || wifiSsid.length() == 0) {
        wifiSsid = "suresh2.4gExt";
        wifiPass = "alangium";
        prefs.putString("ssid", wifiSsid);
        prefs.putString("pass", wifiPass);
    }
    prefs.end();

    if (wifiSsid.length() > 0) {
        Serial.printf("[WIFI] Connecting to SSID: %s\n", wifiSsid.c_str());
        WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
    }

    char wifiDetail[40];
    snprintf(wifiDetail, sizeof(wifiDetail), "SSID: %s", wifiSsid.c_str());
    unsigned long wifiStartMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiStartMs < 9000)) {
        if (progress < 85) progress++;
        renderProfessionalLoaderFrame(progress, "CONNECTING WI-FI", wifiDetail);
        delay(35);
    }

    if (WiFi.status() == WL_CONNECTED) {
        char ipDetail[40];
        snprintf(ipDetail, sizeof(ipDetail), "IP: %s", WiFi.localIP().toString().c_str());
        configTzTime("IST-5:30", "pool.ntp.org", "time.google.com");
        advanceLoader(90, "WI-FI CONNECTED", ipDetail, 10);
    } else {
        Serial.println("[WIFI] Connection timeout, SoftAP active");
        advanceLoader(90, "AP MODE ACTIVE", "SSID: Edifier-Radio", 10);
    }

    // 13. Setup Web Remote Server
    setupWebServer();

    // 14. Station Tuning Preparation & LVGL Display Pre-Warm
    // Fully resolve styles, font glyphs and widget coordinates before loader dismisses
    lv_scr_load(scrRadio);
    updateRadioUI();
    lv_timer_handler();

    String stName = (currentStationIndex >= 0 && currentStationIndex < (int)runtimeStations.size())
                    ? runtimeStations[currentStationIndex].name : String("AIR Thrissur");
    char tuneMsg[40];
    snprintf(tuneMsg, sizeof(tuneMsg), "Tuning: %s", stName.c_str());
    advanceLoader(100, "DEVICE READY", tuneMsg, 12);
    delay(400); // Clean hold at 100% with solid green LEDs

    // 15. Clean Transition to Radio Screen (Instantaneous, fluid switch)
    loaderSprite.deleteSprite();
    lastActivityMs = millis();
    switchScreen(SCREEN_RADIO);
    playCurrentStation();
    lv_timer_handler();

    if (bootFromAlarm) {
        bootFromAlarm = false;
        if (timerDuration > 0) {
            alarmActivePlaying = true;
            alarmAutoOffExpiryMs = millis() + ((unsigned long)timerDuration * 60UL * 1000UL);
            Serial.printf("[ALARM] Booted from Alarm Timer! Auto-Off set for %d minutes (expires at millis %lu)\n",
                          timerDuration, alarmAutoOffExpiryMs);
        }
    }
}



/* -------------------------------------------------------------------------
 * Arduino Main Loop
 * ------------------------------------------------------------------------- */
void loop() {
    // 0. Check deferred screen switch from touch interrupt
    if (hasPendingScreenSwitch) {
        hasPendingScreenSwitch = false;
        switchScreen(pendingScreenSwitch);
    }

    // 0. Dial Push-Button State & 4-Second Power-Off Hold Monitor
    static bool swLastPressed = false;
    static unsigned long swPressStartMs = 0;

    bool swPressed = (digitalRead(ENCODER_SW_PIN) == LOW);
    if (swPressed && !swLastPressed) {
        swPressStartMs = millis();
        swLastPressed = true;
    } else if (swPressed && swLastPressed) {
        unsigned long heldMs = millis() - swPressStartMs;
        if (heldMs >= 700) {
            pwrCountdownActive = true;
            lastActivityMs = millis();
            showPowerOffOverlay(heldMs);
        }
        if (heldMs >= 4000) {
            enterPowerOffMode();
            swLastPressed = false;
            pwrCountdownActive = false;
        }
    } else if (!swPressed && swLastPressed) {
        swLastPressed = false;
        unsigned long heldMs = millis() - swPressStartMs;
        if (pwrCountdownActive) {
            pwrCountdownActive = false;
            hidePowerOffOverlay();
        } else if (heldMs >= 30 && heldMs < 700) {
            // Standard short tap: dispatch ENCODER_CLICK
            EncoderAction act = { ENCODER_CLICK };
            if (encoderActionQueue) xQueueSend(encoderActionQueue, &act, 0);
        }
    }

    // 1. Process Queued Rotary Knob Actions IMMEDIATELY for zero-latency response
    EncoderAction action;
    while (encoderActionQueue && xQueueReceive(encoderActionQueue, &action, 0) == pdTRUE) {
        lastActivityMs = millis();

        if (action.type == ENCODER_ROTATE_CW) {
            ledChaseDir = -1; // Inverted so LED rotation matches physical CW dial rotation
            lastChaseMs = millis();

            if (isScreensaverActive && currentScreen == SCREEN_CLOCK) {
                isScreensaverActive = false;
                switchScreen(SCREEN_HOME_LAUNCHER);
            } else if (currentScreen == SCREEN_HOME_LAUNCHER) {
                selectedAppIndex = (selectedAppIndex + 1) % 4;
                updateHomeLauncherUI();
            } else if (currentScreen == SCREEN_RADIO || currentScreen == SCREEN_CLOCK) {
                setVolume(currentVolume + 1);
            } else if (currentScreen == SCREEN_WIFI_KEYPAD) {
                kpIndex = (kpIndex + 1) % totalKpEntries;
                updateKeypadUI();
            } else if (currentScreen == SCREEN_SETTINGS) {
                selectedSettingRow = (selectedSettingRow + 1) % 7;
                highlightSettingsRow(selectedSettingRow);
            } else if (currentScreen == SCREEN_WIFI_SCAN) {
                if (!scannedSsids.empty()) {
                    selectedWifiRow = (selectedWifiRow + 1) % (int)scannedSsids.size();
                    highlightWifiRow(selectedWifiRow);
                }
            }
        } else if (action.type == ENCODER_ROTATE_CCW) {
            ledChaseDir = 1; // Inverted so LED rotation matches physical CCW dial rotation
            lastChaseMs = millis();

            if (isScreensaverActive && currentScreen == SCREEN_CLOCK) {
                isScreensaverActive = false;
                switchScreen(SCREEN_HOME_LAUNCHER);
            } else if (currentScreen == SCREEN_HOME_LAUNCHER) {
                selectedAppIndex = (selectedAppIndex - 1 + 4) % 4;
                updateHomeLauncherUI();
            } else if (currentScreen == SCREEN_RADIO || currentScreen == SCREEN_CLOCK) {
                setVolume(currentVolume - 1);
            } else if (currentScreen == SCREEN_WIFI_KEYPAD) {
                kpIndex = (kpIndex - 1 + totalKpEntries) % totalKpEntries;
                updateKeypadUI();
            } else if (currentScreen == SCREEN_SETTINGS) {
                selectedSettingRow = (selectedSettingRow - 1 + 7) % 7;
                highlightSettingsRow(selectedSettingRow);
            } else if (currentScreen == SCREEN_WIFI_SCAN) {
                if (!scannedSsids.empty()) {
                    selectedWifiRow = (selectedWifiRow - 1 + (int)scannedSsids.size()) % (int)scannedSsids.size();
                    highlightWifiRow(selectedWifiRow);
                }
            }
        } else if (action.type == ENCODER_CLICK) {
            if (isScreensaverActive && currentScreen == SCREEN_CLOCK) {
                isScreensaverActive = false;
                switchScreen(SCREEN_HOME_LAUNCHER);
            } else if (currentScreen == SCREEN_HOME_LAUNCHER) {
                homeLaunchSelectedApp();
            } else if (currentScreen == SCREEN_RADIO) {
                togglePlayPause();
            } else if (currentScreen == SCREEN_CLOCK) {
                cycleClockFace();
            } else if (currentScreen == SCREEN_SETTINGS) {
                settingsSelectCurrent();
            } else if (currentScreen == SCREEN_WIFI_KEYPAD) {
                keypadSelectCurrent();
            } else if (currentScreen == SCREEN_WIFI_SCAN) {
                if (!scannedSsids.empty() && selectedWifiRow >= 0 && selectedWifiRow < (int)scannedSsids.size()) {
                    selectedSsid = scannedSsids[selectedWifiRow];
                    kpEnteredPass = "";
                    kpIndex = 0;
                    switchScreen(SCREEN_WIFI_KEYPAD);
                }
            }
        }
    }

    // 2. Continuous Audio Stream Worker
    if (isBuffering && audioIsRunning && (audioSampleRate > 0 || audioBitRate > 0 || audioCurrentTime > 0)) {
        isBuffering = false;
        isPlaying = true;
        greenConfirmationUntilMs = millis() + 2000; // Green LED confirmation
        updateUI();
    }

    // 3. Monitor WiFi connection status changes & NTP Sync
    static wl_status_t lastWifiStatus = WL_IDLE_STATUS;
    wl_status_t curWifiStatus = WiFi.status();
    if (curWifiStatus != lastWifiStatus) {
        lastWifiStatus = curWifiStatus;
        if (curWifiStatus == WL_CONNECTED) {
            Serial.printf("\n[WIFI] Connected to Home Network! IP: %s\n", WiFi.localIP().toString().c_str());
            configTzTime("IST-5:30", "pool.ntp.org", "time.google.com");
            updateUI();
            if (!isPlaying) {
                playCurrentStation();
            }
        } else {
            Serial.println("\n[WIFI] Home WiFi not connected. Use AP: Edifier-Radio (192.168.4.1)");
            updateUI();
        }
    }

    // 4. DNS Server for Captive Portal on SoftAP
    dnsServer.processNextRequest();

    // 5. Screensaver idle check (Transfers to Clock Face while keeping audio playing)
    if (screensaverTimeoutSec > 0 && !isScreensaverActive && (currentScreen == SCREEN_RADIO || currentScreen == SCREEN_HOME_LAUNCHER)) {
        if (millis() - lastActivityMs > (unsigned long)screensaverTimeoutSec * 1000) {
            isScreensaverActive = true;
            switchScreen(SCREEN_CLOCK);
            Serial.println("[SCREENSAVER] Idle timeout reached -> Clock Screensaver active");
        }
    }

    // 6. Core 1 Dispatch Queue for Web Server Requests
    if (pendingWebAction != ACT_NONE) {
        PendingWebAction act = pendingWebAction;
        pendingWebAction = ACT_NONE;
        if (act == ACT_PLAY_PAUSE) {
            if (isPlaying) {
                sendAudioPauseResume();
                isPlaying = false;
                isBuffering = false;
            } else {
                if (audioIsRunning) {
                    sendAudioPauseResume();
                    isPlaying = true;
                } else {
                    playCurrentStation();
                }
            }
        } else if (act == ACT_NEXT) {
            currentStationIndex = (currentStationIndex + 1) % runtimeStations.size();
            playCurrentStation();
        } else if (act == ACT_PREV) {
            currentStationIndex = (currentStationIndex - 1 + runtimeStations.size()) % runtimeStations.size();
            playCurrentStation();
        } else if (act == ACT_PLAY_STATION) {
            int targetId = pendingStationId;
            pendingStationId = -1;
            if (targetId >= 0 && targetId < (int)runtimeStations.size()) {
                currentStationIndex = targetId;
                playCurrentStation();
            }
        }
        updateUI();
    }

    if (pendingVol >= 0) {
        int v = pendingVol;
        pendingVol = -1;
        setVolume(v);
    }

    if (pendingDeleteStationIdx >= 0) {
        int idx = pendingDeleteStationIdx;
        pendingDeleteStationIdx = -1;
        deleteFavorite(idx);
    }

    if (pendingAddStation.pending) {
        pendingAddStation.pending = false;
        RuntimeStation st;
        st.name = pendingAddStation.name;
        st.url = pendingAddStation.url;
        st.state = (strlen(pendingAddStation.state) > 0) ? pendingAddStation.state : "General";
        st.language = (strlen(pendingAddStation.lang) > 0) ? pendingAddStation.lang : "General";
        addFavorite(st, pendingAddStation.playNow);
    }

    if (pendingDirectPlay.pending) {
        pendingDirectPlay.pending = false;
        playDirectStream(pendingDirectPlay.name, pendingDirectPlay.url, pendingDirectPlay.state, pendingDirectPlay.lang);
    }

    if (pendingWifiSave.pending) {
        pendingWifiSave.pending = false;
        prefs.begin("crow_wifi", false);
        prefs.putString("ssid", pendingWifiSave.ssid);
        prefs.putString("pass", pendingWifiSave.pass);
        prefs.end();
        WiFi.begin(pendingWifiSave.ssid, pendingWifiSave.pass);
    }

    if (pendingTimerSave.pending) {
        pendingTimerSave.pending = false;
        saveTimerSettings();
    }

    // 7. Auto-On Alarm Watcher (Runs every second when device is powered ON)
    static unsigned long lastAlarmCheckMs = 0;
    static int lastTriggeredMinute = -1;
    if (millis() - lastAlarmCheckMs > 1000) {
        lastAlarmCheckMs = millis();
        if (timerEnabled) {
            time_t nowSec = time(nullptr);
            if (nowSec > 100000) {
                struct tm t;
                localtime_r(&nowSec, &t);
                int currentMinOfDay = t.tm_hour * 60 + t.tm_min;
                int alarmMinOfDay = timerHour * 60 + timerMin;

                if (currentMinOfDay == alarmMinOfDay && currentMinOfDay != lastTriggeredMinute) {
                    lastTriggeredMinute = currentMinOfDay;
                    Serial.printf("[ALARM] Alarm Time Reached (%02d:%02d IST)! Starting playback...\n",
                                  timerHour, timerMin);

                    if (!isPlaying) {
                        if (isScreensaverActive && currentScreen == SCREEN_CLOCK) {
                            isScreensaverActive = false;
                        }
                        switchScreen(SCREEN_RADIO);
                        playCurrentStation();
                    }

                    if (timerDuration > 0) {
                        alarmActivePlaying = true;
                        alarmAutoOffExpiryMs = millis() + ((unsigned long)timerDuration * 60UL * 1000UL);
                        Serial.printf("[ALARM] Auto-Off Sleep Timer set for %d minutes\n", timerDuration);
                    }
                }
            }
        }
    }

    // 8. Auto-Off Timer Countdown Check
    if (alarmActivePlaying && timerDuration > 0 && (millis() > alarmAutoOffExpiryMs)) {
        alarmActivePlaying = false;
        Serial.printf("[ALARM] Auto-Off Sleep Timer (%d min) expired -> Entering Standby Mode...\n", timerDuration);
        enterPowerOffMode();
    }

    // 8. Buffering Timeout Watchdog (25 seconds for multi-segment HLS buffering)
    if (isBuffering && (audioSampleRate == 0) && (millis() - bufferingStartMs > 25000)) {
        Serial.println("[WATCHDOG] Buffering timeout (25s) - triggering CDN failover or recovery");
        bufferingStartMs = millis();
        pendingCdnFailover = true;
    }

    if (pendingCdnFailover) {
        pendingCdnFailover = false;
        triggerCdnFailover();
    }

    if (pendingSaveFavorites) {
        pendingSaveFavorites = false;
        saveFavorites();
    }

    // Auto-clear stream URL error state after alert duration expires
    if (isStreamError && (millis() >= streamErrorUntilMs)) {
        isStreamError = false;
        streamErrorReason = "";
        updateUI();
    }

    // 9. Background Wi-Fi Auto-Reconnect
    static unsigned long lastWifiCheckMs = 0;
    if (millis() - lastWifiCheckMs > 10000) {
        lastWifiCheckMs = millis();
        if (WiFi.status() != WL_CONNECTED && wifiSsid.length() > 0) {
            Serial.printf("[WIFI] Auto-reconnecting to '%s'...\n", wifiSsid.c_str());
            WiFi.reconnect();
        }
    }

    // 10. Ambient Lighting Engine Loop
    updateAmbientLeds();

    // 11. LVGL Timer Handler & Display Redraw (60+ FPS responsive UI)
    lv_timer_handler();

    vTaskDelay(pdMS_TO_TICKS(4));
}
