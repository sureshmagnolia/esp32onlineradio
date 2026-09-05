/**
 * =========================================================================================
 * Project: ESP32 Internet Radio - Board 1 (Wi-Fi Fetcher & Audio Decoder)
 * Board Target: ESP32-WROOM-32 (Standard Dual-Core Dev Board)
 * Features:
 *   1. Multi-SSID Auto-Failover ('suresh2.4gExt' & 'suresh')
 *   2. Wi-Fi Power-Save Disabled (WiFi.setSleep(false) for zero stutter)
 *   3. Stream URL Auto-Recovery & Fallback Engine
 *   4. Non-blocking Background Reconnect & Telemetry
 * Architecture: Method 1 - I2S Wire Bridge (I2S Master Output)
 * =========================================================================================
 */

#include <Arduino.h>
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
// Stream URLs (Primary + Backup Fallback)
// -----------------------------------------------------------------------------
// Primary: Akashvani Thrissur Direct MP3 Relay (HTTP saves ~45KB RAM vs HTTPS)
const char* STREAM_URL_PRIMARY = "http://airrelay.onrender.com/thrissur.mp3";
const char* STREAM_URL_BACKUP  = "https://airrelay.onrender.com/thrissur.mp3"; 

const char* currentStreamUrl = STREAM_URL_PRIMARY;

// -----------------------------------------------------------------------------
// Pin Configuration
// -----------------------------------------------------------------------------
#define STATUS_LED_PIN    2   // Onboard Blue Status LED

// I2S Bus Pin Configuration (ESP32 Board 1 is Master Clock & Data Source)
#define I2S_BCLK_PIN      27  // Bit Clock (GPIO 27)
#define I2S_LRC_PIN       26  // Left/Right Word Select Clock (GPIO 26)
#define I2S_DOUT_PIN      25  // Audio Data Out to Board 2 DIN (GPIO 25)

// -----------------------------------------------------------------------------
// Global Objects & Watchdog Metrics
// -----------------------------------------------------------------------------
Audio audio;

volatile unsigned long totalSamplesSent = 0;
unsigned long lastObservedSamples = 0;
unsigned long lastSampleProgressTime = 0;
unsigned long lastFlowReport = 0;
unsigned long lastStreamRetry = 0;
unsigned long lastLedBlink = 0;
bool ledState = false;
int retryCount = 0;

// Stall threshold: If no new audio samples decoded for 4.5 seconds, force auto-heal
const unsigned long STREAM_STALL_TIMEOUT_MS = 4500;

// Forward Function Declarations
void initWiFi();
void checkWiFiAndStream();

// =============================================================================
// I2S Real-Time Sample Hook
// =============================================================================
void audio_process_i2s(int32_t* outBuff, int16_t validSamples, bool* continueI2S) {
    if (validSamples > 0) {
        totalSamplesSent += validSamples;
    }
}

// =============================================================================
// Setup Routine
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    Serial.println("\n==================================================");
    Serial.println(" ESP32 Internet Radio - Akashvani Thrissur (AIR)  ");
    Serial.println(" Auto-Healing Watchdog & High-Reliability Audio   ");
    Serial.println("==================================================");

    // Register audio logging callback
    Audio::audio_info_callback = [](Audio::msg_t msg) {
        if (msg.msg) {
            Serial.printf("[AUDIO CORE] %s\n", msg.msg);
        }
    };

    // 1. Initialize I2S Master output
    bool i2sOk = audio.setPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DOUT_PIN);
    Serial.printf("[SETUP] I2S setPinout result: %s\n", i2sOk ? "SUCCESS (Pins Active)" : "FAILED");
    
    audio.setVolume(21); // Maximum volume (no software attenuation)
    audio.setConnectionTimeout(3000, 8000); // 3s connect timeout, 8s read timeout

    // 2. Initialize Wi-Fi with Multi-AP support
    initWiFi();

    // 3. Connect to the initial audio stream
    lastSampleProgressTime = millis();
    Serial.printf("[AUDIO] Starting stream: %s ...\n", currentStreamUrl);
    audio.connecttohost(currentStreamUrl);
}

// =============================================================================
// Main Loop
// =============================================================================
void loop() {
    // 1. Service audio decoder and I2S buffer
    audio.loop();

    // 2. Check connection health, watch for silent stalls, and auto-recover
    checkWiFiAndStream();

    // 3. Periodic I2S Audio Flow Telemetry & Heap Diagnostics (Every 3 seconds)
    unsigned long now = millis();
    if (now - lastFlowReport >= 3000) {
        lastFlowReport = now;
        bool isActivelyPlaying = (totalSamplesSent > lastObservedSamples) || 
                                 (now - lastSampleProgressTime < STREAM_STALL_TIMEOUT_MS && totalSamplesSent > 0);

        if (isActivelyPlaying) {
            digitalWrite(STATUS_LED_PIN, HIGH); // Solid ON when audio is actively flowing
            Serial.printf("[I2S FLOW METER] >>> PLAYING! Total Samples: %lu | RSSI: %d dBm | Free Heap: %d bytes | SSID: %s <<<\n",
                          totalSamplesSent, WiFi.RSSI(), ESP.getFreeHeap(), WiFi.SSID().c_str());
        } else {
            Serial.printf("[I2S FLOW METER] Buffering/Stalled... (Running: %s, Wi-Fi: %s, Free Heap: %d bytes)\n",
                          audio.isRunning() ? "YES" : "NO",
                          WiFi.status() == WL_CONNECTED ? "ONLINE" : "OFFLINE",
                          ESP.getFreeHeap());
        }
    }
}

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * @brief Configures Multi-SSID Wi-Fi and connects to the best available AP.
 */
void initWiFi() {
    WiFi.mode(WIFI_STA);
    
    // CRITICAL FOR AUDIO: Disable 802.11 power saving to prevent packet latency & jitter
    WiFi.setSleep(false);

    // Register all available Access Points
    wifiMulti.addAP(AP1_SSID, AP1_PASS);
    wifiMulti.addAP(AP2_SSID, AP2_PASS);

    Serial.printf("[WIFI] Scanning & connecting to best available network ('%s' or '%s') ...\n",
                  AP1_SSID, AP2_SSID);

    while (wifiMulti.run() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        // Blink LED while searching
        ledState = !ledState;
        digitalWrite(STATUS_LED_PIN, ledState);
    }

    // Configure high-performance public DNS (Google 8.8.8.8 & Cloudflare 1.1.1.1)
    IPAddress dns1(8, 8, 8, 8);
    IPAddress dns2(1, 1, 1, 1);
    WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2);

    Serial.println("\n[WIFI] Connected Successfully!");
    Serial.printf("[WIFI] Active SSID : %s\n", WiFi.SSID().c_str());
    Serial.printf("[WIFI] IP Address  : %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] RSSI Signal : %d dBm\n", WiFi.RSSI());
    digitalWrite(STATUS_LED_PIN, HIGH);
}

/**
 * @brief Monitors Wi-Fi and sample flow; automatically detects silent stalls and heals.
 */
void checkWiFiAndStream() {
    unsigned long now = millis();

    // Check if new audio samples were produced
    if (totalSamplesSent != lastObservedSamples) {
        lastObservedSamples = totalSamplesSent;
        lastSampleProgressTime = now;
    }

    // Case 1: Wi-Fi Disconnected -> Run WiFiMulti to reconnect to best AP
    if (WiFi.status() != WL_CONNECTED) {
        if (now - lastLedBlink >= 250) {
            lastLedBlink = now;
            ledState = !ledState;
            digitalWrite(STATUS_LED_PIN, ledState); // Fast blink when Wi-Fi drops
        }

        if (wifiMulti.run() == WL_CONNECTED) {
            Serial.printf("\n[WIFI] Reconnected to SSID: %s (Signal: %d dBm)\n",
                          WiFi.SSID().c_str(), WiFi.RSSI());
            // Reset decoder and restart audio immediately upon Wi-Fi recovery
            audio.stopSong();
            delay(50);
            audio.connecttohost(currentStreamUrl);
            lastSampleProgressTime = now;
            retryCount = 0;
        }
        return;
    }

    // Case 2: Wi-Fi is connected, but stream is stalled or stopped
    // Evaluates TRUE if audio engine stopped OR if 0 new samples decoded for > STREAM_STALL_TIMEOUT_MS
    bool isStreamStalled = !audio.isRunning() || 
                          (totalSamplesSent > 0 && (now - lastSampleProgressTime >= STREAM_STALL_TIMEOUT_MS));

    if (isStreamStalled) {
        // Blink LED (500ms) to indicate auto-recovering state
        if (now - lastLedBlink >= 500) {
            lastLedBlink = now;
            ledState = !ledState;
            digitalWrite(STATUS_LED_PIN, ledState);
        }

        // Retry every 3 seconds
        if (now - lastStreamRetry >= 3000) {
            lastStreamRetry = now;
            retryCount++;

            Serial.printf("[WATCHDOG AUTO-HEAL] Stream stalled (no samples for %lums). Attempt #%d...\n",
                          now - lastSampleProgressTime, retryCount);

            // Cleanly teardown socket & decoder buffers to prevent memory leaks
            audio.stopSong();
            delay(100);

            // Failover URL rotation if stalled repeatedly (every 3 attempts)
            if (retryCount % 3 == 0) {
                if (currentStreamUrl == STREAM_URL_PRIMARY) {
                    currentStreamUrl = STREAM_URL_BACKUP;
                    Serial.println("[AUDIO FAILOVER] Switching to Backup URL...");
                } else {
                    currentStreamUrl = STREAM_URL_PRIMARY;
                    Serial.println("[AUDIO FAILOVER] Switching to Primary URL...");
                }
            }

            Serial.printf("[AUDIO] Reconnecting to: %s (Free Heap: %d bytes)...\n", 
                          currentStreamUrl, ESP.getFreeHeap());
            
            audio.connecttohost(currentStreamUrl);
            lastSampleProgressTime = now; // Reset timer for new attempt
        }
    } else {
        // Stream is actively flowing: ensure retry count is clear
        retryCount = 0;
    }
}
