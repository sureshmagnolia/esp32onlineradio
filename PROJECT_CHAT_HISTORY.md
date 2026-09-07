# 📻 ESP32 Internet Radio to Bluetooth Project — Complete History & Log

**Date**: August 31, 2026  
**Primary Station**: Akashvani Thrissur (All India Radio Malayalam)  
**Target Speaker**: `AS23` (Bluetooth A2DP)  
**Master Storage Path**: `D:\ESP32Radio`  

---

## 📑 Table of Contents
1. [Project Overview & Architecture](#project-overview--architecture)
2. [Chronological Milestones & Troubleshooting Summary](#chronological-milestones--troubleshooting-summary)
3. [Complete Hardware Pinout & Wiring](#complete-hardware-pinout--wiring)
4. [Software & Library Configurations](#software--library-configurations)
5. [HLS AAC vs. Direct MP3 Analysis](#hls-aac-vs-direct-mp3-analysis)
6. [Future Expansion: Direct Speaker Connection (Removing Board 2)](#future-expansion-direct-speaker-connection-removing-board-2)
7. [Raw Chat Transcript Reference](#raw-chat-transcript-reference)

---

## 1. Project Overview & Architecture

### Goal
Stream live Akashvani Thrissur Malayalam radio using ESP32 boards and transmit the audio wirelessly to an **AS23** Bluetooth speaker with zero stutter and high reliability.

### The Dual-ESP32 I2S Wire Bridge Architecture
```
[Internet Cloud] 
       │ (Wi-Fi 2.4GHz: 'suresh2.4gExt' or 'suresh' - Auto Failover)
       ▼
┌─────────────────────────────────────────────────────────────┐
│                   ESP32 BOARD 1 (Wi-Fi)                     │
│  - Dual-SSID Auto-Failover & Auto-Reconnect Engine          │
│  - WiFi.setSleep(false) [Zero-Stutter Active RF]            │
│  - HTTP/HTTPS SSL Client & Helix MP3 Decoder                │
│  - I2S Master Clock & Data Source (44.1 kHz / 16-bit)       │
└───────────────────────┬─────────────────────────────────────┘
                        │  I2S Wire Bridge (32-bit slot timing)
                        │  • BCLK (GPIO 27 ──> GPIO 14)
                        │  • LRC  (GPIO 26 ──> GPIO 15)
                        │  • DOUT (GPIO 25 ──> GPIO 32)
                        │  • GND  (GND     ─── GND)
                        ▼
┌─────────────────────────────────────────────────────────────┐
│                 ESP32 BOARD 2 (Bluetooth)                  │
│  - I2S Hardware Slave Receiver (32-bit slot compatible)     │
│  - Dual-Core RTOS Audio Task Queue                          │
│  - Bluetooth A2DP Source / Transmitter                      │
│  - Target Speaker: "AS23" (Auto-reconnect & Persistent)     │
└───────────────────────┬─────────────────────────────────────┘
                        │ (Bluetooth Classic A2DP Stereo)
                        ▼
               🔊 [AS23 Bluetooth Speaker]
```

---

## 2. Chronological Milestones & Troubleshooting Summary

1. **Architecture Selection**:
   - Single ESP32 cannot run Wi-Fi streaming + Bluetooth A2DP audio source concurrently without RF collisions, buffer underruns, and heap exhaustion.
   - Solved using 2 ESP32 boards connected via digital I2S.
2. **I2S Slot Width Synchronization**:
   - Board 1 (`ESP32-audioI2S`) outputs 16-bit PCM in 32-bit I2S frame slots (`I2S_SLOT_MODE_STEREO`, 32-bit per channel).
   - Board 2 (`ESP32-A2DP`) was configured as an I2S Slave with matching 32-bit slot alignment.
3. **Physical Wire Verification**:
   - Initial silence was diagnosed to a loose jumper wire between boards. Once reseated with a common ground, audio played loud and clear.
4. **Akashvani Thrissur MP3 Relay Integration**:
   - Integrated the direct high-performance MP3 relay `https://airrelay.onrender.com/thrissur.mp3`.
5. **Direct HLS AAC MPEG-TS Experimentation**:
   - Tested direct parsing of CloudFront MPEG-TS HLS stream (`f70fdeca437dc326.m3u8`).
   - Demuxed video PID 256 and audio PID 257.
   - Identified that FAAD2 AAC-LC decoding requires ~90–110 KB of contiguous working heap for 2048-point MDCT synthesis filter banks, exceeding non-PSRAM ESP32 internal SRAM during active TLS.
   - Verified that the 64Kbps MP3 relay is the optimal stream for non-PSRAM ESP32-WROOM boards.
6. **Dual-SSID & Zero-Stutter Resilience**:
   - Added `WiFiMulti` for automatic selection between `suresh2.4gExt` and `suresh`.
   - Disabled modem sleep (`WiFi.setSleep(false)`) to eliminate packet jitter.
   - Added automatic stream retry with `audio.stopSong()` heap-flush to guarantee 24/7 stability.

---

## 3. Complete Hardware Pinout & Wiring

| Board 1 Pin (Wi-Fi) | Board 2 Pin (Bluetooth) | Function |
|---|---|---|
| **GPIO 27** | **GPIO 14** | I2S Bit Clock (`BCLK`) |
| **GPIO 26** | **GPIO 15** | I2S Word Select / Left-Right Clock (`LRC`) |
| **GPIO 25** | **GPIO 32** | I2S Digital Audio Data (`DOUT` $\rightarrow$ `DIN`) |
| **GND** | **GND** | **Common Ground Reference (Mandatory)** |

---

## 4. Software & Library Configurations

* **Board 1 Sketch**: `D:\ESP32Radio\ESP32_Board1_WiFi\ESP32_Board1_WiFi.ino`
* **Board 2 Sketch**: `D:\ESP32Radio\ESP32_Board2_Bluetooth\ESP32_Board2_Bluetooth.ino`
* **Partition Scheme**: `Huge APP (3MB No OTA/1MB SPIFFS)` for both boards.
* **Libraries Included in `D:\ESP32Radio\libraries`**:
  * `ESP32-audioI2S`: Pre-configured for non-PSRAM operation, 32-bit slot I2S, and custom HTTP callbacks.
  * `ESP32-A2DP`: Pre-configured for slave I2S input and persistent reconnection to `AS23`.

---

## 5. Future Expansion: Direct Speaker Connection (Removing Board 2)

If you wish to remove Board 2 and wire Board 1 directly to a speaker system:

### Option A: I2S Hi-Fi DAC with 3.5mm Jack (`PCM5102A` ~ ₹180)
* Connect to **GPIO 27 (BCK), GPIO 26 (LCK), GPIO 25 (DIN), 5V, GND**.
* Plug standard 3.5mm AUX cable into any home theater or speaker.

### Option B: I2S 3W Amplifier Module (`MAX98357A` ~ ₹120)
* Connect to **GPIO 27 (BCLK), GPIO 26 (LRC), GPIO 25 (DIN), 5V, GND**.
* Connect `+` and `-` speaker output terminals directly to raw speaker drivers (4Ω/8Ω).

### Option C: ESP32 Built-in DAC (Zero Extra Cost)
* Connect **GPIO 25 (Right), GPIO 26 (Left), GND** through 10µF DC-blocking capacitors into an AUX jack.
* Call `audio.setPinout(0, 0, 0, 0, true);` in Board 1 code.

---

## 6. Raw Chat Transcript Reference

The complete machine-readable and verbatim chat logs are stored in:
* `D:\ESP32Radio\history_and_logs\transcript.jsonl` (Compact chronological log)
* `D:\ESP32Radio\history_and_logs\transcript_full.jsonl` (Complete untruncated transcript)

---

## 7. Waveshare ESP32-S3 Touch LCD 2.8" Standalone Radio & 3D Enclosure (September 2026)

- **Hardware**: Waveshare ESP32-S3-Touch-LCD-2.8 (ST7789 2.8" SPI display, CST328 touch, PCM5101 I2S DAC, 16MB Flash, 8MB Octal PSRAM).
- **Physical Layout**: Screen oriented in landscape with `BOOT`, `RESET`, and `PWR` buttons along the top edge ($Y=0$).
- **Multi-Function `BOOT` (GPIO 0)**:
  - Single click: Next station / SD track.
  - Double click: Previous station / SD track.
  - Long press / hold: Smooth volume ramp (+1 every 220ms up to 21).
  - Standby: RTC wakeup to resume playback.
- **Enclosure**: Complete parametric 3D desktop enclosure model with internal mounting bosses, speaker grille, and virtual assembly audit in `3D_Enclosure_Waveshare28/`.

---

## 8. JC3248W535 3.5" Smart Radio: Everyday Auto-On Alarm & Standby Recovery (September 2026)

- **Problem**:
  - The daily auto-on alarm worked on Day 1, but failed on subsequent days.
  - Investigation identified two root causes:
    1. **Standby Power Recovery**: When power was cut while in Standby mode, the ESP32-S3 rebooted with clock at epoch 1970 (no Wi-Fi/NTP) and immediately re-entered deep sleep without re-arming the RTC alarm wakeup timer.
    2. **Casual Touch Disarming**: Tapping volume sliders or station buttons set `alarmActivePlaying = false;`, which permanently disabled the auto-off timer and prevented recurring alarm re-arming.
- **Fixes Applied & Verified on Device (COM8)**:
  1. **Standby Silent NTP Recovery**: When booting into standby mode, the system silently connects to Wi-Fi with backlight OFF, synchronizes time via NTP, calculates exact seconds until next alarm, arms `esp_sleep_enable_timer_wakeup()`, and enters deep sleep.
  2. **Dedicated Dismiss Actions**: Only explicit **Pause** (`btn_play_cb`) or **Power Off** (`btn_sleep_cb`) terminates an active alarm session. Volume changes and station browsing remain non-destructive.
  3. **Background Wi-Fi Watchdog**: Added a 20s health check in `loop()` so the radio never stays disconnected overnight when left powered on.
  4. **Auto-On Robustness**: Minimum volume guarantee (level 12), clean audio pipeline flush, and connection retry before starting playback.

---

## 9. Golden Baseline ("AllIsWell") Repository Consolidation (September 2026)

- **Single Golden Folder**: `ESP32S3_JC3248W535_Radio_AllIsWell/` is established as the sole canonical codebase for the 3.5" radio.
- **Obsolete Folders Removed from Git Tracking**:
  - `ESP32S3_JC3248W535_Radio/` (redundant duplicate)
  - `ESP32S3_JC3248W535_Radio_Dev/` (redundant duplicate)
  - `ESP32S3_3.5_Display_Board_SmartClock/` (legacy experimental clock)
  - `3D_Enclosure/` (obsolete prototype, superseded by `3D_Enclosure_JC3248W535/` and `Laser_Cuts/`)
  - Temporary root scrap scripts (`PAGE_INDEX.*`, `capture_boot.py`, `test_*.py`).
- **Retained & Maintained**:
  - `ESP32S3_JC3248W535_Radio_AllIsWell/` (JC3248W535 Golden Baseline)
  - `ESP32S3_Waveshare28_Radio/` (Waveshare 2.8" Standalone Radio)
  - `3D_Enclosure_JC3248W535/` & `Laser_Cuts/` (JC3248W535 Cabinet & Laser files)
  - `3D_Enclosure_Waveshare28/` (Waveshare 2.8" 3D Enclosure)
  - `libraries/` (Patched ESP32-audioI2S, LVGL 8.4, ESP32-A2DP)
  - `stations_full.json` & `generate_stations_header.py` (Station DB management)

