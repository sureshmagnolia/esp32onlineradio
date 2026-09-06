# Waveshare ESP32-S3 Touch LCD 2.8" Hi-Fi Stereo Radio — Full Project Context & Architecture

## 1. Executive Summary
This directory contains the complete, standalone, production-tested firmware for the **Waveshare ESP32-S3-Touch-LCD-2.8** development board. It ports **all features** from the 3.5" JC3248W535 Internet Radio project while being 100% physically and logically isolated in its own folder (`ESP32S3_Waveshare28_Radio/`).

---

## 2. Hardware Specifications

| Subsystem | Hardware Component | Interface | Key Pins / Characteristics |
| :--- | :--- | :--- | :--- |
| **MCU & Memory** | ESP32-S3-WROOM-1 | Dual-Core Xtensa LX7 @ 240MHz | 16MB Flash (`qio`), 8MB Octal PSRAM (`opi`) |
| **Display** | 2.8" IPS LCD ($320 \times 240$) | ST7789 SPI Bus | MOSI: 45, SCLK: 40, CS: 42, DC: 41, RST: 39, BL: 5 (LEDC PWM) |
| **Touchscreen** | Capacitive Touch | CST328 / CST3530 I2C | SDA: 1, SCL: 3, INT: 4 (RTC), RST: 2 (Addr: 0x1A) |
| **Audio DAC** | TI PCM5101A 32-bit Hi-Fi DAC | I2S 3-Wire Bus | BCLK: 13, LRCK/WS: 12, DOUT: 14 |
| **Audio Output** | Dual 2030 Acoustic Cavity Speakers | Pure 2-Channel Stereo | `audio.forceMono(false);` |
| **MicroSD (TF)** | Push-Push Socket | SD_MMC (1-bit bus) | CLK: 2, CMD: 1, D0: 42 (Mount: `/sdcard`) |
| **Battery ADC** | Onboard Resistor Divider (2:1) | SAR ADC1 | GPIO 6 (`BAT_ADC_PIN`), 3.3V–4.2V range |
| **Buttons** | Tactile Microswitches | GPIO / EN / SYS_EN | Button 1 (BOOT: GPIO 0), Button 2 (RESET: EN), Button 3 (PWR: SYS_EN) |

---

## 3. Physical & Visual Orientation
* **Landscape Mode**: $320 \times 240$ pixels.
* **Top-Edge Component Alignment**:
  * The physical PCB edge with the **Buttons** and **MicroSD card socket** is positioned at the **TOP** of the visual screen ($Y = 0$, top status bar).
  * ST7789 SPI panel: `swap_xy = true`, `mirror(false, true)`.
  * CST328 touch coordinates: `screen_x = raw_y`, `screen_y = 239 - raw_x`.

---

## 4. Button Architecture & Deep Sleep Engine

* **Button 1 (`BOOT` / `GPIO 0`) Multi-Function Actions**:
  * **Single Touch (< 500ms)**: Next station (Radio mode) / Next audio track (SD mode).
  * **Double Touch (< 350ms gap)**: Previous station (Radio mode) / Previous audio track (SD mode).
  * **Hold / Long Press (> 600ms)**: Continuous Volume Up ramping (+1 step every 220ms up to max 21).
  * **Deep Sleep Wakeup**: Armed via `esp_sleep_enable_ext1_wakeup((1ULL << 4) | (1ULL << 0), ESP_EXT1_WAKEUP_ANY_LOW)` with RTC pull-ups.
* **Button 2 (`RESET` / `EN`)**:
  * Sealed inside the 3D printed front cabinet (no external plunger hole) to eliminate accidental hard reboots while handling the radio.
* **Button 3 (`PWR` / `SYS_EN`)**:
  * Dedicated hardware sliding/push battery power toggle switch.
* **Touchscreen Wakeup**:
  * Touching the capacitive screen (GPIO 4 INT) in deep sleep triggers instant wakeup with a 8-second "Resume Radio" confirmation dialog.

---

## 5. Firmware Software Modules

1. **`ESP32S3_Waveshare28_Radio.ino`**:
   * Main system orchestrator.
   * FreeRTOS LVGL display thread (Core 1) with thread-safe mutex locks (`bsp_display_lock` / `bsp_display_unlock`).
   * Non-blocking audio stream pump on Core 0.
   * Fast stream live state detection, 10-attempt connection recovery engine.
   * Embedded Web Remote Portal on port 80 with DNS captive portal for zero-config Wi-Fi onboarding (`http://192.168.4.1` or `http://<ip>`).
   * Hardware RTC Timer Auto-On Alarm & Sleep Timer with auto-off playback limits.
2. **`esp_bsp.c` & `esp_bsp.h`**:
   * Hardware Board Support Package.
   * ST7789 SPI driver via ESP-IDF `esp_lcd_panel_io_spi` and `esp_lcd_new_panel_st7789`.
   * CST328 capacitive touch controller over I2C.
   * Smooth 5kHz LEDC PWM backlight control.
3. **`button_handler.cpp` & `button_handler.h`**:
   * State-machine debounced button engine supporting single click, double click, long press, and continuous hold repeat.
4. **`pincfg.h`**:
   * Centralized pin definitions, rotation constants, and peripheral addresses.
5. **`stations_db.h`**:
   * 277 curated internet radio stations spanning All-India Radio (AIR), regional languages (Hindi, Tamil, Malayalam, Telugu, Kannada, Marathi, Punjabi, Bengali, etc.), devotional, classical, news, and world channels.
6. **`platformio.ini`**:
   * PlatformIO configuration with 16MB Flash, 8MB Octal PSRAM (`qio_opi`), and custom partition layout.

---

## 6. Build & Verification Status

Compiled cleanly using `arduino-cli`:
```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi --libraries "d:\ESP32Radio\libraries" "d:\ESP32Radio\ESP32S3_Waveshare28_Radio\ESP32S3_Waveshare28_Radio.ino"
```
* **Flash Usage**: 2,497,126 bytes (79% of 3MB APP partition).
* **RAM Usage**: 112,984 bytes (34% of 327KB internal SRAM; SSL buffers allocated dynamically from 8MB PSRAM).
* **Exit Code**: `0` (Clean Build).

---

## 7. 3D CAD Enclosure Integration (`3D_Enclosure_Waveshare28/`)
* **Dimensions**: $142.0\text{ mm} \times 62.0\text{ mm} \times 32.0\text{ mm}$.
* **Dual Acoustic Cavity Chambers**: Custom-tuned rear acoustic chambers for 2030 stereo speakers.
* **Continuous Flat Bottom Wall**: No unsightly scallop cutouts; MicroSD card sits only $2.5\text{ mm}$ from outer surface and springs $1.5\text{ mm}$ outside for easy pinch removal.
* **Active Plungers**: Exactly 2 captive plungers (`BOOT` and `BAT_PWR`); `RESET` hole permanently sealed.
* **Interactive 3D WebGL Viewer**: [`interactive_assembly_viewer.html`](file:///d:/ESP32Radio/3D_Enclosure_Waveshare28/interactive_assembly_viewer.html) provides complete real-time 3D exploded view inspection in any browser.
