# EDIFIER 1.28" ESP32-S3 HMI Rotary Touch Studio Radio Firmware
*A Passion for Sound — High-Fidelity Internet Radio & Digital Twin*

Production-grade, clean-aesthetic Internet Radio firmware designed specifically for the **Elecrow CrowPanel 1.28" ESP32-S3 HMI Rotary Display** (`DHE38128D`) and paired with a browser-based hardware digital twin simulator.

---

## 1. Interaction & Control Architecture

The radio features an intuitive, uncluttered, and high-end acoustic interface:

```
                          ┌──────────────────────────┐
                          │   TOP CHANNEL CAPSULE    │
                          │      [ CH 01 OF 07 ]     │
                          └─────────────┬────────────┘
                                        │
        ┌───────────────────────────────┼───────────────────────────────┐
        ▼                               ▼                               ▼
  [ ◀ BUTTON ]                  [ STATION TITLE ]                 [ ▶ BUTTON ]
Previous Station             Centered Bold Typography             Next Station
(or Swipe Right)               (Tap to Play / Pause)            (or Swipe Left)
        ▲                               │                               ▲
        └───────────────────────────────┼───────────────────────────────┘
                                        │
                          ┌─────────────┴────────────┐
                          │  CONCENTRIC VOLUME ARC   │
                          │   Outer Ring (0%–100%)   │
                          └─────────────┬────────────┘
                                        │
                          ┌─────────────┴────────────┐
                          │    SOFT MARQUEE TICKER   │
                          │ EDIFIER • 🔊 85% • IP... │
                          └──────────────────────────┘
```

### Controls Summary
| Action | Hardware Input | Function |
| :--- | :--- | :--- |
| **Volume Up / Down** | **Rotate Knurled Dial** | Dedicated exclusively to volume (0–21 / 0%–100%) with concentric perimeter arc feedback |
| **Next Station** | **Touch `▶` Button** or **Swipe Left** | Tunes to next station with instantaneous HLS/AAC/MP3 stream switch |
| **Previous Station** | **Touch `◀` Button** or **Swipe Right** | Tunes to previous station |
| **Play / Pause** | **Knob Single-Click** or **Tap Screen Center** | Instantly toggles audio streaming |
| **Ambient Light Cycle** | **Knob Long Press (> 800ms)** | Cycles NeoPixel modes: Breathing Cyan, Volume VU, Rainbow, Off |
| **Hardware Reset** | **Red RST Button** | Triggers cold boot with authentic EDIFIER boot splash screen |

---

## 2. Directory Structure & Files

All project files are fully self-contained in `d:\ESP32Radio\CrowPanel_1.28_ESP32_HMI_Radio`:

```
CrowPanel_1.28_ESP32_HMI_Radio/
├── CrowPanel_1.28_ESP32_HMI_Radio.ino   # Main ESP32-S3 firmware (LVGL 8.4, LovyanGFX, I2S Audio)
├── edifier_logo.h                       # Authentic 240x240 monochrome boot splash bitmap array (PROGMEM)
├── stations_db.h                        # 10 verified Malayalam starter stations (Direct CloudFront CDN mirrors)
├── CST816D.h / CST816D.cpp              # Capacitive touch controller driver with swipe gesture recognition
├── web_index.h                          # Embedded Web Remote server & SoftAP Captive Portal page
├── README.md                            # Complete documentation & wiring guide
└── simulator/                           # Interactive Hardware Digital Twin
    ├── index.html                       # Master Studio Simulator (240x240 touch twin, knob, NeoPixels, audio)
    ├── web_remote.html                  # 192.168.4.1 Web Remote client twin
    ├── server.py                        # Multi-threaded Python simulation server
    ├── edifier_emblem.svg               # Official 3-bar acoustic wing vector emblem
    └── edifier_logo.svg                 # Official EDIFIER vector emblem + wordmark
```

---

## 3. Audio Wiring Guide (MAX98357A 3W Mono Amp)

The CrowPanel 1.28" does not have an onboard speaker amplifier. Connect an external **MAX98357A I2S Mono DAC/Amp** using the two accessible 4-pin JST/MX1.25 ports (**I2C** and **UART**) on the rear of the board—**no soldering to the fragile 0.5mm FPC ribbon is required**:

### Pin Connection Table

| MAX98357A Pin | CrowPanel Port | CrowPanel Pin Label | ESP32-S3 GPIO | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **VIN** | **I2C Port** | `5V` (or `VCC`) | 5V Power Rail | Power supply (2.5V–5.5V, use 5V for 3.2W output) |
| **GND** | **I2C Port** | `GND` | Common Ground | System Ground |
| **BCLK** | **I2C Port** | **`SCL`** | **GPIO 39** | I2S Bit Clock (Clock to Clock) |
| **LRC** (or WS) | **I2C Port** | **`SDA`** | **GPIO 38** | I2S Word Select / Left-Right Clock |
| **DIN** | **UART Port** | **`TX`** | **GPIO 43** | I2S Serial Audio Data (Transmit to In) |
| **GAIN** | *Leave Open* | — | — | Floating = +12dB gain (ideal for 3W 4Ω/8Ω speaker) |
| **SD** (Shutdown) | *Leave Open* | — | — | Floating = mixes (Left + Right)/2 to mono output |

> [!TIP]
> **Speaker Connection**: Wire your 4Ω or 8Ω speaker directly to the MAX98357 screw terminal block (`+` and `-`). Never connect speaker `-` to ground as the MAX98357 uses a bridge-tied load (BTL) differential output.

---

## 4. Hardware Pinout Reference

| Component | Signal | GPIO | Function / Notes |
| :--- | :--- | :--- | :--- |
| **Power Rails** | `PIN_PWR_EN1` | **GPIO 1** | Power Rail 1 (Driven HIGH) |
| | `PIN_PWR_EN2` | **GPIO 2** | Power Rail 2 (Driven HIGH) |
| | `PIN_PWR_LED` | **GPIO 40** | Green power LED |
| **GC9A01 LCD** | `LCD_DC` | **GPIO 3** | Data / Command |
| | `LCD_CS` | **GPIO 9** | SPI Chip Select |
| | `LCD_SCLK` | **GPIO 10** | SPI Clock |
| | `LCD_MOSI` | **GPIO 11** | SPI MOSI |
| | `LCD_RST` | **GPIO 14** | Hardware Reset |
| | `LCD_BL` | **GPIO 46** | Backlight PWM (5 kHz) |
| **CST816D Touch** | `TP_SDA` | **GPIO 6** | I2C SDA |
| | `TP_SCL` | **GPIO 7** | I2C SCL |
| | `TP_INT` | **GPIO 5** | Touch Interrupt |
| | `TP_RST` | **GPIO 13** | Touch Reset |
| **EC3501 Knob** | `ENCODER_A` | **GPIO 45** | Quadrature Phase A |
| | `ENCODER_B` | **GPIO 42** | Quadrature Phase B |
| | `ENCODER_SW` | **GPIO 41** | Center Push Switch (Active LOW) |
| **NeoPixel Ring** | `NEOPIXEL_PIN` | **GPIO 48** | 5x WS2812B RGB LEDs |

---

## 5. How to Compile & Flash

### Prerequisites
* **Board Core**: `esp32:esp32` (v3.0.0+ recommended)
* **Libraries** (located in `d:\ESP32Radio\libraries`):
  * `LovyanGFX`
  * `lvgl` (v8.4.0)
  * `ESP32-audioI2S`
  * `Adafruit_NeoPixel`

### CLI Command
```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB --libraries "d:\ESP32Radio\libraries" "d:\ESP32Radio\CrowPanel_1.28_ESP32_HMI_Radio"
```

### Upload Command
```bash
arduino-cli upload -p COMx --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB "d:\ESP32Radio\CrowPanel_1.28_ESP32_HMI_Radio"
```

---

## 6. Running the Master Studio Simulator

To test all UI interactions, audio playback, and remote features locally in the browser:
```bash
python d:\ESP32Radio\CrowPanel_1.28_ESP32_HMI_Radio\simulator\server.py
```
Open **`http://localhost:8080/simulator/index.html`** in your browser.
