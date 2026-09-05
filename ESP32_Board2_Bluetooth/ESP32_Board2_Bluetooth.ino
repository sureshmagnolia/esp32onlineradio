/**
 * =========================================================================================
 * Project: ESP32 Internet Radio - Board 2 (Bluetooth A2DP Transmitter)
 * Board Target: ESP32-WROOM-32 (Standard Dual-Core Dev Board)
 * Architecture: Method 1 - I2S Wire Bridge (I2S Slave Receiver -> A2DP Source)
 * 
 * Hardware I2S Pinout from Board 1:
 *  - GPIO 27  <- I2S Bit Clock (BCLK / BCK)
 *  - GPIO 26  <- I2S Word Select / Left-Right Clock (LRC / WS)
 *  - GPIO 25  <- I2S Serial Data In (DIN)  <-- Connects to Board 1 GPIO 25 (DOUT)
 *  - GND      <- Common Ground with Board 1
 * =========================================================================================
 */

#include <Arduino.h>
#include <driver/i2s.h>
#include "BluetoothA2DPSource.h"

// -----------------------------------------------------------------------------
// Configuration & Target Bluetooth Device
// -----------------------------------------------------------------------------
const char* TARGET_BT_SPEAKER_NAME = "AS23";

// Onboard Status LED (GPIO 2 on standard ESP32 boards)
#define STATUS_LED_PIN        2

// I2S Hardware Input Pins (Board 2 acts as I2S Slave Receiver)
#define I2S_BCLK_PIN          27   // Bit Clock Input from Board 1 (GPIO 27)
#define I2S_LRC_PIN           26   // Word Select / LRC Input from Board 1 (GPIO 26)
#define I2S_DIN_PIN           25   // Serial Data Input from Board 1 DOUT (GPIO 25)

#define I2S_PORT_NUM          I2S_NUM_0
#define I2S_SAMPLE_RATE       44100
#define I2S_BUFFER_COUNT      8
#define I2S_BUFFER_LEN        512

// -----------------------------------------------------------------------------
// Global Objects & Watchdog Tracking
// -----------------------------------------------------------------------------
BluetoothA2DPSource a2dp_source;

unsigned long lastBtStatusCheck  = 0;
const unsigned long BT_CHECK_INTERVAL_MS = 5000; // Check BT connection every 5s
bool isBluetoothConnected        = false;

// -----------------------------------------------------------------------------
// Forward Function Declarations
// -----------------------------------------------------------------------------
void initI2SSlaveReceiver();
int32_t get_i2s_audio_data(Frame *frame, int32_t frame_count);
void bt_connection_state_callback(esp_a2d_connection_state_t state, void *obj);
void bt_audio_state_callback(esp_a2d_audio_state_t state, void *obj);

// =============================================================================
// Setup Routine
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialize onboard status LED
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    // Visual power-on blink (3 quick flashes)
    for (int i = 0; i < 3; i++) {
        digitalWrite(STATUS_LED_PIN, HIGH);
        delay(100);
        digitalWrite(STATUS_LED_PIN, LOW);
        delay(100);
    }

    Serial.println("\n==================================================");
    Serial.println(" ESP32 Internet Radio - Board 2 (I2S -> BT A2DP)  ");
    Serial.println("==================================================");
    Serial.printf("[SETUP] Target Bluetooth Speaker: '%s'\n", TARGET_BT_SPEAKER_NAME);
    Serial.printf("[SETUP] I2S RX Pinout -> BCLK: %d, LRC: %d, DIN: %d\n",
                  I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DIN_PIN);

    // 1. Initialize ESP32 I2S hardware peripheral as Slave Receiver
    initI2SSlaveReceiver();

    // 2. Register callbacks for connection & audio state transitions
    a2dp_source.set_on_connection_state_changed(bt_connection_state_callback);
    a2dp_source.set_on_audio_state_changed(bt_audio_state_callback);

    // 3. Configure auto-reconnect behaviour
    a2dp_source.set_auto_reconnect(true);

    // 4. Start Bluetooth A2DP Source with external I2S data stream callback
    Serial.println("[BT] Starting Bluetooth A2DP Source discovery and stream...");
    a2dp_source.start(TARGET_BT_SPEAKER_NAME, get_i2s_audio_data);
}

// =============================================================================
// Main Loop
// =============================================================================
void loop() {
    unsigned long currentMillis = millis();

    // LED Status Indicator:
    // Solid ON when connected, Blinking (500ms toggle) when scanning/reconnecting
    if (a2dp_source.is_connected()) {
        digitalWrite(STATUS_LED_PIN, HIGH); // Solid ON when streaming to speaker
    } else {
        // Blink every 500ms while scanning
        static unsigned long lastLedToggle = 0;
        static bool ledState = false;
        if (currentMillis - lastLedToggle >= 500) {
            lastLedToggle = currentMillis;
            ledState = !ledState;
            digitalWrite(STATUS_LED_PIN, ledState ? HIGH : LOW);
        }
    }

    // Periodic watchdog to ensure speaker remains connected
    if (currentMillis - lastBtStatusCheck >= BT_CHECK_INTERVAL_MS) {
        lastBtStatusCheck = currentMillis;

        if (!a2dp_source.is_connected()) {
            Serial.printf("[BT STATUS] Speaker '%s' is not connected. Scanning & Reconnecting...\n", 
                          TARGET_BT_SPEAKER_NAME);
            a2dp_source.reconnect();
        }
    }

    delay(20);
}

// =============================================================================
// I2S Configuration & Data Acquisition
// =============================================================================

/**
 * Configure I2S Peripheral as SLAVE RX.
 * In slave mode, BCLK and LRC are driven by Board 1 to guarantee perfect phase synchronization.
 */
void initI2SSlaveReceiver() {
    Serial.println("[I2S] Configuring I2S Port 0 as Slave Receiver (32-bit slot)...");

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_SLAVE | I2S_MODE_RX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // Matches Board 1 32-bit slot format
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = I2S_BUFFER_COUNT,
        .dma_buf_len = I2S_BUFFER_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRC_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE, // No TX needed on Board 2
        .data_in_num = I2S_DIN_PIN         // Audio data input from Board 1
    };

    esp_err_t err = i2s_driver_install(I2S_PORT_NUM, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[I2S ERROR] Failed to install I2S driver! (0x%x)\n", err);
        return;
    }

    err = i2s_set_pin(I2S_PORT_NUM, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[I2S ERROR] Failed to set I2S pins! (0x%x)\n", err);
        return;
    }

    Serial.println("[I2S] Slave receiver initialized successfully.");
}

/**
 * A2DP Audio Callback:
 * Fetches raw 32-bit slot PCM audio from Board 1, extracts 16-bit audio, and delivers to Bluetooth SBC encoder.
 */
int32_t get_i2s_audio_data(Frame *frame, int32_t frame_count) {
    static int32_t rx_buffer[1024]; // 512 stereo samples (L+R = 1024 words)
    size_t samples_to_read = frame_count * 2; // L + R
    if (samples_to_read > 1024) samples_to_read = 1024;
    size_t bytes_to_read = samples_to_read * sizeof(int32_t);
    size_t bytes_read = 0;
    
    // Read 32-bit slot PCM bytes directly from DMA buffer with a 20ms timeout
    esp_err_t result = i2s_read(I2S_PORT_NUM, (void*)rx_buffer, bytes_to_read, &bytes_read, pdMS_TO_TICKS(20));

    if (result == ESP_OK && bytes_read > 0) {
        size_t frames_received = bytes_read / (2 * sizeof(int32_t));
        for (size_t i = 0; i < frames_received; i++) {
            // High 16 bits of the 32-bit slot contain the decoded audio sample
            frame[i].channel1 = (int16_t)(rx_buffer[i * 2] >> 16);     // Left
            frame[i].channel2 = (int16_t)(rx_buffer[i * 2 + 1] >> 16); // Right
        }
        return frames_received;
    }

    // If Board 1 has not started streaming yet or buffer is dry, supply silence
    memset(frame, 0, frame_count * sizeof(Frame));
    return frame_count;
}

// =============================================================================
// Bluetooth Event Callbacks
// =============================================================================

/**
 * Callback when Bluetooth connection status changes (Connecting, Connected, Disconnected)
 */
void bt_connection_state_callback(esp_a2d_connection_state_t state, void *obj) {
    switch (state) {
        case ESP_A2D_CONNECTION_STATE_DISCONNECTED:
            Serial.println("\n[BT EVENT] Connection State: DISCONNECTED");
            isBluetoothConnected = false;
            break;
        case ESP_A2D_CONNECTION_STATE_CONNECTING:
            Serial.println("[BT EVENT] Connection State: CONNECTING...");
            break;
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            Serial.printf("\n[BT EVENT] >>> SUCCESS: Connected to '%s'! <<<\n\n", TARGET_BT_SPEAKER_NAME);
            isBluetoothConnected = true;
            break;
        case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
            Serial.println("[BT EVENT] Connection State: DISCONNECTING...");
            break;
        default:
            Serial.printf("[BT EVENT] Connection State: %d\n", state);
            break;
    }
}

/**
 * Callback when Bluetooth audio stream status changes (Started, Stopped)
 */
void bt_audio_state_callback(esp_a2d_audio_state_t state, void *obj) {
    switch (state) {
        case ESP_A2D_AUDIO_STATE_STOPPED:
            Serial.println("[BT AUDIO] Stream State: STOPPED / SUSPENDED");
            break;
        case ESP_A2D_AUDIO_STATE_STARTED:
            Serial.println("[BT AUDIO] Stream State: TRANSMITTING AUDIO TO SPEAKER");
            break;
        default:
            Serial.printf("[BT AUDIO] Stream State: %d\n", state);
            break;
    }
}
