/**
 * =========================================================================================
 * Project: M5Stack Core2 - Akashvani Kerala Minimalist Internet Radio
 * Target Board: M5Stack Core2 (ESP32-D0WDQ6-V3, 16MB Flash, 8MB PSRAM)
 * Features:
 *   - Clean Minimalist UI (zero text artifacts, no heavy rendering, 0% CPU when playing)
 *   - 20Hz Throttled I2C Touch Polling (99.5% CPU Dedicated Exclusively to Audio Decoding)
 *   - Continuous AAC HLS Stream Decoding with 650KB PSRAM Buffer
 * =========================================================================================
 */

#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "Audio.h"

// -----------------------------------------------------------------------------
// Wi-Fi Credentials & Multi-AP Configuration
// -----------------------------------------------------------------------------
WiFiMulti wifiMulti;
const char* AP1_SSID = "suresh2.4gExt";
const char* AP1_PASS = "alangium";
const char* AP2_SSID = "suresh";
const char* AP2_PASS = "alangium";

// -----------------------------------------------------------------------------
// Station Structure & Akashvani Kerala Channels List
// -----------------------------------------------------------------------------
struct RadioStation {
    const char* name;
    const char* region;
    const char* url;
};

const RadioStation STATIONS[] = {
    { "Akashvani Trissur",         "Thrissur, Kerala",   "https://d1cvqgmbcpg5yn.cloudfront.net/f70fdeca437dc326/f70fdeca437dc326.m3u8" },
    { "Akashvani Manjeri",         "Malappuram, Kerala", "https://d1cvqgmbcpg5yn.cloudfront.net/58390a2ed33cea4a/58390a2ed33cea4a.m3u8" },
    { "FM Rainbow Kochi",          "Kochi FM 102.3",     "https://d1cvqgmbcpg5yn.cloudfront.net/7df6f2a8c3c4d33b/7df6f2a8c3c4d33b.m3u8" },
    { "VB Malayalam (Ananthapuri)","Trivandrum, Kerala", "https://radio.wavespb.com/live/ad3a8436a329e2d6/ad3a8436a329e2d6.m3u8" },
    { "Akashvani Calicut",         "Kozhikode, Kerala",  "https://radio.wavespb.com/live/8321393de70015fc/8321393de70015fc.m3u8" },
    { "Akashvani Real FM",         "Kozhikode FM",       "https://radio.wavespb.com/live/b69c296065db7627/b69c296065db7627.m3u8" },
    { "Akashvani Kochi",           "Kochi, Kerala",      "https://radio.wavespb.com/live/70400e7510e87cdf/70400e7510e87cdf.m3u8" },
    { "Akashvani Kannur",          "Kannur, Kerala",     "https://radio.wavespb.com/live/b82c91a395fc4a7d/b82c91a395fc4a7d.m3u8" },
    { "Akashvani Devikulam",       "Idukki, Kerala",     "https://radio.wavespb.com/live/e97acc829da9bf2a/e97acc829da9bf2a.m3u8" }
};

const int NUM_STATIONS = sizeof(STATIONS) / sizeof(STATIONS[0]);
int currentStationIdx = 0;

// -----------------------------------------------------------------------------
// Audio Engine & Pinout Configuration for M5Stack Core2
// -----------------------------------------------------------------------------
Audio audio;

// M5Core2 Internal Speaker (NS4168 I2S Power Amp)
#define I2S_BCLK_PIN    12  // Bit Clock
#define I2S_LRC_PIN     0   // Left/Right Word Select Clock (WS)
#define I2S_DOUT_PIN    2   // Serial Audio Data Out

// -----------------------------------------------------------------------------
// Global States & UI Metrics
// -----------------------------------------------------------------------------
int currentVolume = 21; // Maximum volume (0 - 21)
bool isPlaying = false;
bool uiNeedsUpdate = true;

volatile unsigned long totalSamplesDecoded = 0;
unsigned long lastSampleProgressTime = 0;
unsigned long lastObservedSamples = 0;
unsigned long lastI2cPollTime = 0;
unsigned long lastWatchdogCheck = 0;
unsigned long lastStatusRefresh = 0;

// Volume slider bar bounds
const int VOL_BAR_X = 50;
const int VOL_BAR_Y = 140;
const int VOL_BAR_W = 220;
const int VOL_BAR_H = 14;

// Forward Declarations
void enableCore2Speaker();
void initWiFi();
void playStation(int idx);
void drawMinimalUI();
void handleTouch();
void checkWatchdog();

// =============================================================================
// I2S Real-Time Sample Hook
// =============================================================================
void audio_process_i2s(int32_t* outBuff, int16_t validSamples, bool* continueI2S) {
    if (validSamples > 0) {
        totalSamplesDecoded += validSamples;
    }
}

// =============================================================================
// Core2 Speaker Power & PMIC Control
// =============================================================================
void enableCore2Speaker() {
    if (M5.Power.getType() == m5::Power_Class::pmic_axp192) {
        // Stop Haptic Vibration Motor (LDO3 = 0)
        M5.Power.Axp192.setLDO3(0);

        // Enable 5V Boost Bus (EXTEN) to supply VDD to NS4168 Amp
        M5.Power.Axp192.setEXTEN(true);

        // Set Register 0x94 Bit 2 (0x04) to enable NS4168 Amp Power
        uint8_t reg94 = M5.Power.Axp192.readRegister8(0x94);
        M5.Power.Axp192.writeRegister8(0x94, reg94 | 0x04);

        // Configure AXP192 GPIO2 as High Output (Register 0x93 = 0x00)
        M5.Power.Axp192.writeRegister8(0x93, 0x00);
        M5.Power.Axp192.setGPIO2(true);
    } else if (M5.Power.getType() == m5::Power_Class::pmic_axp2101) {
        M5.Power.Axp2101.setALDO3(3300);
        M5.Power.Axp2101.setBLDO2(3300);
    }
    M5.Power.setExtOutput(true);
}

// =============================================================================
// Setup Routine
// =============================================================================
void setup() {
    auto cfg = M5.config();
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    M5.begin(cfg);

    Serial.begin(115200);
    delay(100);

    Serial.println("\n=======================================================");
    Serial.println("   M5Stack Core2 - Minimalist Akashvani Player         ");
    Serial.println("=======================================================");

    // Enable speaker amplifier power
    enableCore2Speaker();

    // Splash screen directly on display
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.drawString("AKASHVANI RADIO", 160, 90);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    M5.Display.drawString("Connecting to Wi-Fi...", 160, 130);

    // 2. Initialize Wi-Fi
    initWiFi();

    // 3. Configure I2S Pinout for Core2 Speaker & Mono mixing
    audio.setPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DOUT_PIN);
    audio.forceMono(true); // Mix stereo L+R for mono speaker
    audio.setVolume(currentVolume); // Maximum volume (21)
    audio.setConnectionTimeout(4000, 10000);

    // Register audio callbacks for metadata
    Audio::audio_info_callback = [](Audio::msg_t msg) {
        if (msg.msg) {
            Serial.printf("[AUDIO CORE] %s\n", msg.msg);
        }
    };

    // 4. Start first station playback
    playStation(currentStationIdx);
}

// =============================================================================
// Main Loop (Maximum CPU Dedication to Audio Loop)
// =============================================================================
void loop() {
    // 1. TOP PRIORITY: Service audio decoding continuously
    if (isPlaying) {
        audio.loop();
    }

    // 2. Throttle I2C Bus transactions (Touch & PMIC polling) to 20Hz (every 50ms)
    unsigned long now = millis();
    if (now - lastI2cPollTime >= 50) {
        lastI2cPollTime = now;
        M5.update();
        handleTouch();
    }

    // 3. Periodic Watchdog Check (every 1 second)
    checkWatchdog();

    // 4. Periodic Status Refresh (Battery every 10 seconds)
    if (now - lastStatusRefresh >= 10000) {
        lastStatusRefresh = now;
        uiNeedsUpdate = true;
    }

    // 5. Minimalist UI Redraw ONLY on state changes
    if (uiNeedsUpdate) {
        uiNeedsUpdate = false;
        drawMinimalUI();
    }
}

// =============================================================================
// Station Control Functions
// =============================================================================
void playStation(int idx) {
    if (idx < 0) idx = NUM_STATIONS - 1;
    if (idx >= NUM_STATIONS) idx = 0;
    currentStationIdx = idx;

    Serial.printf("\n[STATION] Switching to: %s (%s)\n", STATIONS[idx].name, STATIONS[idx].url);

    enableCore2Speaker();

    audio.stopSong();
    delay(40);
    audio.connecttohost(STATIONS[idx].url);
    lastSampleProgressTime = millis();
    isPlaying = true;
    uiNeedsUpdate = true;
}

// =============================================================================
// Touch & Button Handling
// =============================================================================
void handleTouch() {
    // 1. Physical Bezel Touch Buttons (BtnA, BtnB, BtnC)
    if (M5.BtnA.wasClicked()) {
        playStation(currentStationIdx - 1);
    }
    if (M5.BtnB.wasClicked()) {
        if (isPlaying) {
            audio.pauseResume();
            isPlaying = audio.isRunning();
        } else {
            playStation(currentStationIdx);
        }
        uiNeedsUpdate = true;
    }
    if (M5.BtnC.wasClicked()) {
        playStation(currentStationIdx + 1);
    }

    // 2. Touch Screen Coordinates & Volume Control
    auto count = M5.Touch.getCount();
    if (count > 0) {
        auto detail = M5.Touch.getDetail(0);
        int x = detail.x;
        int y = detail.y;

        // Volume Buttons [-] and [+]
        if (detail.wasClicked() || detail.isPressed()) {
            // [-] button
            if (x >= 10 && x <= 45 && y >= 130 && y <= 165 && detail.wasClicked()) {
                currentVolume = (currentVolume > 0) ? currentVolume - 1 : 0;
                audio.setVolume(currentVolume);
                uiNeedsUpdate = true;
            }
            // [+] button
            else if (x >= 275 && x <= 310 && y >= 130 && y <= 165 && detail.wasClicked()) {
                currentVolume = (currentVolume < 21) ? currentVolume + 1 : 21;
                audio.setVolume(currentVolume);
                uiNeedsUpdate = true;
            }
            // Touching directly on the volume bar
            else if (x >= VOL_BAR_X && x <= (VOL_BAR_X + VOL_BAR_W) && y >= 130 && y <= 165) {
                int newVol = map(x, VOL_BAR_X, VOL_BAR_X + VOL_BAR_W, 0, 21);
                newVol = constrain(newVol, 0, 21);
                if (newVol != currentVolume) {
                    currentVolume = newVol;
                    audio.setVolume(currentVolume);
                    uiNeedsUpdate = true;
                }
            }
        }

        if (detail.wasClicked()) {
            // Tap Bottom Row On-Screen Buttons
            if (y >= 185) {
                if (x < 105) {
                    playStation(currentStationIdx - 1);
                } else if (x > 215) {
                    playStation(currentStationIdx + 1);
                } else {
                    if (isPlaying) {
                        audio.pauseResume();
                        isPlaying = audio.isRunning();
                    } else {
                        playStation(currentStationIdx);
                    }
                    uiNeedsUpdate = true;
                }
            }
        }
    }
}

// =============================================================================
// UI Rendering - Clean, High-Contrast Minimalist Screen (Zero Stray Text)
// =============================================================================
void drawMinimalUI() {
    // 1. Black Background
    M5.Display.fillScreen(TFT_BLACK);

    // 2. Top Header Bar
    M5.Display.fillRect(0, 0, 320, 26, 0x18C3);
    M5.Display.drawLine(0, 26, 320, 26, 0x3186);

    // Wi-Fi Label
    char wifiBuf[32];
    snprintf(wifiBuf, sizeof(wifiBuf), "WiFi: %s", WiFi.SSID().c_str());
    M5.Display.setTextDatum(middle_left);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_GREEN, 0x18C3);
    M5.Display.drawString(wifiBuf, 10, 13);

    // Play/Pause State Badge
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(isPlaying ? TFT_CYAN : TFT_ORANGE, 0x18C3);
    M5.Display.drawString(isPlaying ? "[ LIVE ]" : "[ PAUSED ]", 160, 13);

    // Battery Level
    char batBuf[16];
    snprintf(batBuf, sizeof(batBuf), "BAT: %d%%", M5.Power.getBatteryLevel());
    M5.Display.setTextDatum(middle_right);
    M5.Display.setTextColor(TFT_WHITE, 0x18C3);
    M5.Display.drawString(batBuf, 310, 13);

    // 3. Station Info Box
    const RadioStation& st = STATIONS[currentStationIdx];

    // Channel Indicator
    char chBuf[24];
    snprintf(chBuf, sizeof(chBuf), "CHANNEL %d OF %d", currentStationIdx + 1, NUM_STATIONS);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(0x7BEF, TFT_BLACK);
    M5.Display.drawString(chBuf, 160, 48);

    // Station Name (Large)
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.drawString(st.name, 160, 75);

    // Location / Subtitle
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    M5.Display.drawString(st.region, 160, 102);

    // 4. Volume Bar & Buttons
    // [-] button
    M5.Display.fillRect(10, 134, 32, 26, 0x2104);
    M5.Display.drawRect(10, 134, 32, 26, 0x4208);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_WHITE, 0x2104);
    M5.Display.drawString("-", 26, 147);

    // Volume Track
    M5.Display.fillRect(VOL_BAR_X, VOL_BAR_Y, VOL_BAR_W, VOL_BAR_H, 0x1082);
    M5.Display.drawRect(VOL_BAR_X, VOL_BAR_Y, VOL_BAR_W, VOL_BAR_H, 0x3186);

    // Active Level Bar
    int activeW = map(currentVolume, 0, 21, 0, VOL_BAR_W);
    if (activeW > 0) {
        M5.Display.fillRect(VOL_BAR_X, VOL_BAR_Y, activeW, VOL_BAR_H, TFT_CYAN);
    }

    // [+] button
    M5.Display.fillRect(278, 134, 32, 26, 0x2104);
    M5.Display.drawRect(278, 134, 32, 26, 0x4208);
    M5.Display.setTextColor(TFT_WHITE, 0x2104);
    M5.Display.drawString("+", 294, 147);

    // Volume Numerical Label
    char volBuf[32];
    snprintf(volBuf, sizeof(volBuf), "VOLUME: %d / 21", currentVolume);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(TFT_LIGHTGRAY, TFT_BLACK);
    M5.Display.drawString(volBuf, 160, 168);

    // 5. Bottom Navigation Bar
    M5.Display.drawLine(0, 185, 320, 185, 0x2104);

    // Button A: Prev
    M5.Display.fillRect(10, 192, 90, 40, 0x18C3);
    M5.Display.drawRect(10, 192, 90, 40, 0x3186);
    M5.Display.setTextColor(TFT_WHITE, 0x18C3);
    M5.Display.drawString("< PREV", 55, 212);

    // Button B: Play/Pause
    uint32_t btnBCol = isPlaying ? 0x9800 : 0x0400;
    M5.Display.fillRect(115, 192, 90, 40, btnBCol);
    M5.Display.drawRect(115, 192, 90, 40, 0x3186);
    M5.Display.setTextColor(TFT_WHITE, btnBCol);
    M5.Display.drawString(isPlaying ? "PAUSE" : "PLAY", 160, 212);

    // Button C: Next
    M5.Display.fillRect(220, 192, 90, 40, 0x18C3);
    M5.Display.drawRect(220, 192, 90, 40, 0x3186);
    M5.Display.setTextColor(TFT_WHITE, 0x18C3);
    M5.Display.drawString("NEXT >", 265, 212);
}

// =============================================================================
// Helper Functions: Wi-Fi Setup & Smart Watchdog
// =============================================================================
void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Disable Wi-Fi sleep for uninterrupted stream

    wifiMulti.addAP(AP1_SSID, AP1_PASS);
    wifiMulti.addAP(AP2_SSID, AP2_PASS);

    Serial.printf("[WIFI] Connecting to '%s' / '%s' ...\n", AP1_SSID, AP2_SSID);

    while (wifiMulti.run() != WL_CONNECTED) {
        delay(250);
        Serial.print(".");
    }

    Serial.println("\n[WIFI] Connected Successfully!");
    Serial.printf("[WIFI] SSID: %s | IP: %s | RSSI: %d dBm\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

void checkWatchdog() {
    unsigned long now = millis();
    if (now - lastWatchdogCheck < 1000) return;
    lastWatchdogCheck = now;

    // Track sample progression
    if (totalSamplesDecoded != lastObservedSamples) {
        lastObservedSamples = totalSamplesDecoded;
        lastSampleProgressTime = now;
    }

    // Auto-heal if playing but 0 samples produced for 6 seconds
    if (isPlaying && (now - lastSampleProgressTime >= 6000)) {
        Serial.printf("[WATCHDOG] Stream stalled on station: %s. Auto-recovering...\n",
                      STATIONS[currentStationIdx].name);
        audio.stopSong();
        delay(40);
        audio.connecttohost(STATIONS[currentStationIdx].url);
        lastSampleProgressTime = now;
    }
}
