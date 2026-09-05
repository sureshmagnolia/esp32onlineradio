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
