# 📻 Elecrow CrowPanel 1.28" ESP32-S3 HMI Rotary Studio Radio
*Branch: `GoldRoundDisplay` — Gold Bezel Round Display Internet Radio*

Production-grade, clean-aesthetic Internet Radio firmware designed specifically for the **Elecrow CrowPanel 1.28" ESP32-S3 HMI Rotary Touch Display** (`DHE38128D`).

---

## 📑 Repository Structure

```
D:\ESP32Radio\
├── CrowPanel_1.28_ESP32_HMI_Radio\      ⭐ Complete Firmware for CrowPanel 1.28" Rotary Display Radio
│   ├── CrowPanel_1.28_ESP32_HMI_Radio.ino # Main Dual-Core Firmware (LVGL 8.3, LovyanGFX, I2S Audio)
│   ├── CST816D.cpp / CST816D.h          # Capacitive touch controller driver with swipe gesture recognition
│   ├── stations_db.h                    # Verified Malayalam starter stations with active CloudFront mirrors
│   ├── web_index.h                      # Embedded Web Remote with master online catalog direct stream playback
│   ├── edifier_logo.h                   # 240x240 boot splash bitmap array
│   ├── README.md                        # Detailed architecture & pinout guide
│   └── simulator/                       # Browser-based hardware digital twin
├── libraries\                           📚 Pre-configured hardware libraries (LovyanGFX, Adafruit_NeoPixel, lvgl, lv_conf.h)
├── stations.json                        🌐 608-station master online catalog fetched by the Web Remote
├── stations_full.json                   🌐 Complete station database
├── generate_stations_header.py          🛠️ Station header generator script
├── read_serial.py                       🔍 CDC Serial monitor script
└── README.md                            📖 Master Repository Guide
```

---

## ⚡ Key Features

1. **Dual-Core FreeRTOS Architecture**:
   - **Core 0 (Audio Task)**: `Audio::loop()` execution with **96 KB HLS jitter buffer** in 8MB PSRAM (6–8 seconds playback cushion).
   - **Core 1 (UI & System Task)**: 60 FPS LVGL 8.3 rendering, CST816D capacitive touch reads, rotary dial decoding, and embedded HTTP web server. Knob response latency is `<50 ms`.

2. **Power & Standby Engine**:
   - **Cold Power-On Starts in Standby**: When power is plugged in, the device starts directly in **Standby Mode** with a soft glowing red pilot LED on the dial. The screen backlight stays dark (0%) and audio remains unstarted.
   - **4-Second Hold Power ON**: Press and hold the knob push-button for 4 seconds to turn ON. The 5 WS2812B NeoPixels display a clockwise rotating chase animation with progressive fill, followed by an emerald green flash confirmation before clean boot.
   - **4-Second Hold Power OFF**: Holding the knob push-button for 4 seconds while playing displays the "POWER OFF" progress overlay, cleanly fades out the backlight, shuts down the audio engine, and returns to Standby Red LED Mode.
   - **Everyday Auto-On Alarm & Sleep Timer**: RTC timer wakeup automatically boots and begins playing at the scheduled alarm time.

3. **Audio Streaming & Failover**:
   - Full support for HLS (`.m3u8`), AAC, and MP3 streams.
   - Automatic CloudFront CDN failover recovery for Akashvani stations.
   - 10 starter Malayalam favorites with verified active distribution mirrors.

4. **Embedded Web Remote Control**:
   - Responsive web interface accessible at `http://<device-ip>/`.
   - Live playback status, volume slider, mute, channel switcher, sleep timer manager, and custom favorite manager.
   - **Master Online Catalog**: Browse and directly stream 608+ regional and national stations without needing to save them to favorites first.

---

## 🛠️ Build & Flash Instructions

Using `arduino-cli`:

```bash
# 1. Compile
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB --libraries "libraries" CrowPanel_1.28_ESP32_HMI_Radio

# 2. Upload (replace COM10 with your serial port)
arduino-cli upload -p COM10 --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB CrowPanel_1.28_ESP32_HMI_Radio
```
