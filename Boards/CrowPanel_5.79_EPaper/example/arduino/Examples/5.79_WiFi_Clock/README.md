# CrowPanel 5.79" ESP32 E-Paper Digital Clock

A high-resolution, full-screen digital clock for the **Elecrow CrowPanel 5.79" ESP32 E-Paper HMI** (792×272 display with dual SSD1683 controller chips).

---

## Features

1. **Interactive Startup Checklist**:
   - Hardware & system boot verification.
   - Live Wi-Fi connection progression displaying connected SSID and assigned local IP address.
   - NTP time synchronization status (India Standard Time UTC+5:30).
   - Clean 2-second confirmation hold before transitioning to the clock.

2. **Ghost-Free E-Paper Screen Transition**:
   - Actively clears startup checklist pixels using partial update.
   - Re-initializes `FastMode1` and executes a hardware screen wipe (`EPD_Display_Clear()`, `EPD_Update()`, `EPD_Clear_R26A6H()`).
   - Leaves **zero residual ghosting** behind the massive clock digits.

3. **High-Impact Vector Typography (DIN 1451 / Bahnschrift)**:
   - **Massive Time Digits**: 215px cap-height custom vector font maximizing vertical display space (792×272 resolution).
   - **Centered Colon Dots**: Symmetrically aligned with the time numerals.
   - **Right-Column Layout**:
     - Large AM/PM indicator.
     - Day of the week.
     - Formatted Date (`DD MMM YYYY`).
     - Subtle live Wi-Fi status indicator (`● WIFI: <SSID>`).

4. **Dual-Chip SSD1683 Refresh Management**:
   - Master and Slave display driver synchronization using `EPD_PartUpdate()` (`0xDC`) to avoid master-only half-screen updates.
   - Persistent differential RAM tracking (`0x26` / `0xA6`) across minute updates for fast partial refreshes without screen flashing.

---

## Hardware Specifications

- **Controller**: ESP32-S3 (QFN56) Dual-Core LX7 @ 240MHz
- **Memory**: 8MB Flash, 8MB Octal SPI PSRAM
- **Display**: 5.79-inch Monochrome E-Paper Display (792 × 272 pixels)
- **Driver IC**: Dual SSD1683 (Master + Slave)
- **Orientation**: 180° rotation (`Rotation = 180`)

---

## Flashing Instructions

### Using `arduino-cli`

```bash
# Compile
arduino-cli compile -b esp32:esp32:esp32s3:CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi \
  --library ../../libraries/Adafruit_GFX_Library \
  --library ../../libraries/Adafruit_BusIO \
  5.79_WiFi_Clock.ino

# Upload (replace COM7 with your port)
arduino-cli upload -b esp32:esp32:esp32s3:CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi \
  -p COM7 \
  5.79_WiFi_Clock.ino
```

---

## Regenerating Fonts

The font assets are generated using Python and Pillow from the Windows system `bahnschrift.ttf` font:

```bash
python generate_clock_assets.py
```
This script produces `MassiveFont.h` containing all 1-bit PROGMEM bit arrays and `ClockGlyph` lookup tables.
