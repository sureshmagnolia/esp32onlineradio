# ESP32-S3 JC3248W535 Luxury Giant Vector Smart Clock & Desk Companion

A high-performance, ultra-clean Smart Desk Clock application tailored for the **ESP32-S3 JC3248W535 / JC3248W535C** 3.5" (480x320) IPS Capacitive Touch display board.

---

## 🌟 Key Features

1. **🕒 Dominant 142px High-Definition Glowing Vector Clock**:
   * Custom-engineered rounded vector segment engine.
   * Vivid Glowing Electric Cyan (`#00E5FF`) active segments on a 100% pure pitch-black OLED background.
   * Flashing neon colon separator.
   * Dedicated ticking Warm Gold (`#FFD600`) seconds counter and `AM/PM` badge.

2. **🌡️ Live Online Weather Forecast Capsule**:
   * Powered by Open-Meteo REST API (Auto-refreshes every 15 minutes).
   * Displays Live Temperature (°C), Weather Condition (e.g. Clear Sky, Partly Cloudy, Rain), and Relative Humidity.

3. **📰 Live Breaking News Continuous Marquee**:
   * Auto-refreshes top headlines every 5 minutes over Wi-Fi.
   * Seamless horizontal scrolling circular marquee ticker on the home screen.
   * Dedicated full reader tab to browse all top headlines.

4. **⏱️ Integrated Pomodoro Focus & Productivity Timer**:
   * 25-minute work intervals with animated visual progress bar.
   * Quick `+5 MIN` and `RESET` buttons.
   * Rotating daily motivational wisdom and productivity quotes.

5. **📶 Phone / PC Web Captive Portal Wi-Fi Setup**:
   * No clunky on-screen keyboards needed.
   * Tap **"SETUP WI-FI VIA PHONE / PC BROWSER"** in the Settings tab.
   * Connect your phone to hotspot `ESP32-SmartClock-Setup`, visit `http://192.168.4.1` in your browser, select your Wi-Fi, and enter the password. Credentials persist permanently in ESP32 Flash NVS.

6. **⚙️ Hardware Monitor & Backlight Control**:
   * Real-time Octal PSRAM & Internal Heap monitor.
   * Live system uptime counter.
   * Touch slider for display brightness (PWM Backlight).

---

## 🛠️ Hardware Specification

* **Board**: Guition JC3248W535 (or JC3248W535C)
* **MCU**: ESP32-S3-WROOM-1 (Dual-Core 240MHz, 16MB Quad Flash, 8MB Octal PSRAM)
* **Display**: 3.5" IPS 320x480 (Rotated 90° Landscape to 480x320), AXS15231B QSPI Driver
* **Touch**: AXS15231B Capacitive Multi-Touch
* **Graphics Library**: LVGL 8.4.0 with Double Framebuffer in Octal PSRAM

---

## 🚀 Build & Upload Instructions

### 1. Arduino CLI Command
To compile and upload via USB Serial (COM port, e.g. `COM8`):

```powershell
arduino-cli compile --upload -p COM8 `
  --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=opi,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,CPUFreq=240" `
  --libraries "d:\ESP32Radio\libraries" `
  "d:\ESP32Radio\ESP32S3_JC3248W535_SmartClock"
```

### 2. Arduino IDE Board Settings
* **Board**: `ESP32S3 Dev Module`
* **USB CDC On Boot**: `Enabled`
* **Flash Size**: `16MB (128Mb)`
* **Flash Mode**: `QIO 80MHz`
* **Partition Scheme**: `16M Flash (3MB APP/9.9MB FATFS)` or `app3M_fat9M_16MB`
* **PSRAM**: `OPI PSRAM`
* **CPU Frequency**: `240MHz (WiFi)`
