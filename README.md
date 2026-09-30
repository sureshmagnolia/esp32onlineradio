# ESP32-S3 SI4732 Portable Radio (ATS-Mini Edition)

A high-performance, full-featured portable DSP radio receiver powered by the **ESP32-S3** microcontroller and Silicon Labs **SI4732** RF tuner, featuring an integrated 1.14" ST7789 color display, physical rotary encoder navigation, an offline-capable Wi-Fi Web Remote, and a dedicated **DXing Signal Hunter Suite**.

---

## Highlights & Unique Features

### 1. Full-Featured Mobile Web Remote SPA
- **Automatic SoftAP Fallback:** When not connected to an existing Wi-Fi network, the device automatically broadcasts an open Wi-Fi network named ATS-Mini-Remote (access interface at http://192.168.4.1).
- **Wi-Fi Manager:** Scan nearby 2.4 GHz Wi-Fi networks and connect directly from your phone or browser.
- **Top Taskbar IP Display:** Live local IP address is continuously displayed on the radio's physical display top taskbar before the Wi-Fi icon.
- **Complete Remote Control:**
  - Real-time frequency input, step tuning, and BFO fine pitch vernier slider.
  - Complete band selector across all 28 supported HF/MW/LW/FM broadcast and amateur bands.
  - Demodulation mode switching (AM, FM, USB, LSB).
  - Audio volume, mute, squelch threshold, and hardware RF attenuator / AGC control.
  - Live S-meter telemetry (dBµV signal level, SNR, and battery voltage).
  - 99 programmable memory presets.

### 2. DXing Signal Hunter Suite
- **Live Station Identification & EiBi Schedule:** Real-time query matching tuned frequency and UTC time against the international EiBi shortwave broadcast database to display station name, country, transmitter power, and broadcast language.
- **One-Click DX Logbook:** Instant logging of frequency, mode, RSSI, SNR, station name, RST/SINPO signal report, and notes, with instant **CSV and ADIF export** compatible with amateur radio logging software.
- **Curated Utility & Oceanic ATC Quick-Tuning:** One-click tuning tables for North Atlantic, Pacific, and Indian Ocean long-range Oceanic Air Traffic Control (MWARA) and automated continuous aviation weather (VOLMET) in USB.

### 3. Hardware Navigation & Stability
- **Instant 2.5-Second Dial Hold-to-Cancel:** Holding the rotary encoder push-button for 2.5 seconds provides immediate cancel/back navigation without needing to release the dial.
- **Thread-Safe Seek Engine:** Asynchronous command queueing from network requests to the main loop prevents FreeRTOS task watchdog trips and display bus collisions.
- **GMT+5:30 (IST) Time Auto-Sync:** Default timezone set to UTC+5:30 with automatic browser time and timezone synchronization upon opening the Web Remote.

---

## Hardware Specifications
- **MCU:** ESP32-S3 (Dual-Core Xtensa LX7, 240 MHz, OSPI/QSPI PSRAM)
- **Tuner IC:** Silicon Labs SI4732-A10 (AM / LW / MW / SW / SSB / FM)
- **Display:** 1.14" ST7789 IPS LCD (240x135)
- **Input:** Incremental rotary encoder with push-button
- **Audio:** I2S / PWM DAC amplifier with speaker & 3.5mm stereo headphone output
- **Power:** LiPo battery with USB-C charging and hardware battery level monitoring

---

## Frequency Coverage
| Band Category | Frequency Range | Supported Modes |
| :--- | :--- | :--- |
| **FM Broadcast** | 64.0 – 108.0 MHz | WFM (Stereo / RDS) |
| **Long Wave (LW)** | 150 – 520 kHz | AM / CW |
| **Medium Wave (MW)** | 520 – 1710 kHz | AM |
| **Short Wave (SW)** | 1.71 – 30.0 MHz | AM / USB / LSB |
| **Aviation (Oceanic ATC / Volmet)** | 2.8 – 22.0 MHz | USB |
| **Amateur Radio Bands** | 160m, 80m, 40m, 30m, 20m, 17m, 15m, 12m, 10m | LSB / USB / CW |
| **CB Radio** | 25.0 – 28.0 MHz (Channels 1–40) | AM / USB / LSB |

---

## Building and Flashing
Compile and flash using rduino-cli:
`ash
# Compile and upload to ESP32-S3 OSPI
arduino-cli compile --profile esp32s3-ospi -p COM11 -u ats-mini
`

---

## License
Open-source personal project for educational and amateur radio use.
