# 📻 Dual ESP32 Internet Radio to Bluetooth Bridge (Optimum Build)

**Target Station**: Akashvani Thrissur (All India Radio Malayalam)  
**Stream Format**: High-Fidelity 64Kbps MP3 Relay (`https://airrelay.onrender.com/thrissur.mp3`)  
**Target Bluetooth Speaker**: `AS23`  

---

## 🏗️ Architecture Overview

The system uses **two standard ESP32-WROOM-32 boards** connected via an **I2S Hardware Bridge**:

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
                        │  I2S Wire Bridge
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

## 🔌 Hardware Wiring (Board-to-Board)

| Signal Name | Board 1 (Wi-Fi Sender) | Board 2 (BT Receiver) | Wire Function / Note |
|---|---|---|---|
| **BCLK (Bit Clock)** | **GPIO 27** | **GPIO 14** | Master I2S Clock |
| **LRC (Word Select)** | **GPIO 26** | **GPIO 15** | Left/Right Word Frame Clock |
| **DATA (Audio Out $\rightarrow$ In)** | **GPIO 25** | **GPIO 32** | Serial Digital PCM Audio Stream |
| **COMMON GROUND** | **GND** | **GND** | **CRITICAL**: Common reference ground |

---

## 📁 Directory Structure

```
D:\ESP32Radio\
├── ESP32S3_JC3248W535_SmartClock\ <-- 3.5" IPS Smart Desk Clock (Giant Vector Clock, Weather, News Marquee, Focus Timer, Web Wi-Fi Portal)
├── ESP32_Board1_WiFi\
│   └── ESP32_Board1_WiFi.ino      <-- Board 1 Firmware (Wi-Fi, Multi-SSID, MP3 Decoder, I2S Master)
├── ESP32_Board2_Bluetooth\
│   └── ESP32_Board2_Bluetooth.ino <-- Board 2 Firmware (I2S Slave Receiver, Bluetooth A2DP Source)
├── libraries\
│   ├── ESP32-audioI2S\            <-- Patched audio library with 32-bit slot & HLS TS support
│   ├── ESP32-A2DP\                <-- High-performance Bluetooth A2DP library
│   └── lvgl\                      <-- LVGL 8.4.0 Graphics Library
└── README.md                      <-- This Guide
```

---

## ⚙️ Key Technical Features & Optimizations

1. **Dual-SSID Auto-Failover (`WiFiMulti`)**:
   - Registered SSIDs: `"suresh2.4gExt"` and `"suresh"` (Password: `"alangium"`).
   - Automatically selects the strongest access point on boot and seamlessly fails over if one network drops.
2. **Zero-Stutter Wi-Fi Streaming**:
   - `WiFi.setSleep(false)` prevents Wi-Fi modem sleep, guaranteeing zero packet latency and rock-solid audio continuity.
3. **Stream Auto-Recovery & Backup Fallback**:
   - Continuously monitors audio buffer flow. If the server drops or network stalls, automatically reconnects every 3 seconds without rebooting.
4. **Visual Status LED (GPIO 2)**:
   - **Solid ON**: Audio actively streaming and playing.
   - **Medium Blink (500 ms)**: Reconnecting to audio stream.
   - **Fast Blink (250 ms)**: Wi-Fi searching / reconnecting.

---

## 🚀 How to Flash via Arduino IDE / CLI

### Board 1 (Wi-Fi Receiver)
- **Port**: COM6
- **Partition Scheme**: `Huge APP (3MB No OTA/1MB SPIFFS)`
- **Sketch**: `D:\ESP32Radio\ESP32_Board1_WiFi\ESP32_Board1_WiFi.ino`

### Board 2 (Bluetooth Transmitter)
- **Port**: COM5 (or designated port)
- **Partition Scheme**: `Huge APP (3MB No OTA/1MB SPIFFS)`
- **Sketch**: `D:\ESP32Radio\ESP32_Board2_Bluetooth\ESP32_Board2_Bluetooth.ino`
