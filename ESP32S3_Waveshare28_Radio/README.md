# Waveshare ESP32-S3 Touch LCD 2.8" Hi-Fi Radio

Dedicated standalone firmware for the **Waveshare ESP32-S3-Touch-LCD-2.8** development board, delivering a full-featured internet radio with LVGL touchscreen controls, PCM5101 I2S audio decoding, and physical button support.

> [!IMPORTANT]
> **Strict Isolation**: This project is 100% self-contained within this directory (`ESP32S3_Waveshare28_Radio/`) and does **not** touch or depend on any code from the 3.5-inch JC3248W535 module.

---

## 📐 Display & Hardware Orientation

* **Orientation**: Landscape ($320 \times 240$).
* **Buttons & Card Reader Placement**: **AT THE TOP OF THE DISPLAY**.
  * The physical edge containing the 3 buttons (`BOOT`, `RESET`, `PWR`) and the MicroSD card socket is oriented at the **top edge** of the radio display ($Y = 0$, top status bar).
  * Touch screen coordinates are calibrated and mapped to this exact orientation.

---

## 🎛️ Physical Hardware Buttons (`BOOT` / `GPIO0`)

| Action | Function | Behavior |
| :--- | :--- | :--- |
| **Single Touch** | **Next Station / SD Track** | Advances to the next radio station or next audio track. |
| **Double Touch** | **Previous Station / SD Track** | Returns to the previous radio station or previous track. |
| **Hold / Long Press** | **Volume Up Ramp** | Continuous volume increment (+1 every 220ms) up to maximum 21. |
| **Deep Sleep Standby** | **System Wakeup** | Hardware RTC interrupt wakes device back up to resume live playback. |
| **`RESET` (Middle)** | **Enclosure Sealed** | The middle button hole is permanently sealed in the 3D printed cabinet to eliminate accidental hard reboots. |
| **`PWR` (Button 3)** | **Hardware Battery Power** | Dedicated hardware sliding/push power toggle switch for battery circuit (`SYS_EN`). |

---

## 📌 Pinout Map (Waveshare 2.8")

| Subsystem | Signal | ESP32-S3 Pin | Notes |
| :--- | :--- | :--- | :--- |
| **ST7789 LCD** | MOSI | `GPIO 45` | SPI Data |
| | SCLK | `GPIO 40` | SPI Clock |
| | CS | `GPIO 42` | Chip Select |
| | DC | `GPIO 41` | Data / Command |
| | RST | `GPIO 39` | Reset |
| | BL | `GPIO 5` | Backlight PWM (Active HIGH) |
| **Touch (CST328)** | SDA | `GPIO 1` | I2C Data (Address 0x1A) |
| | SCL | `GPIO 3` | I2C Clock |
| | INT | `GPIO 4` | Interrupt |
| | RST | `GPIO 2` | Touch Reset |
| **Audio (PCM5101)** | BCLK | `GPIO 13` | I2S Bit Clock |
| | LRCK / WS | `GPIO 12` | Word Select |
| | DOUT | `GPIO 14` | Serial Data Out |
| **Button** | BOOT | `GPIO 0` | Active LOW with pull-up |

---

## 🚀 How to Compile & Flash

### Option A: VS Code + PlatformIO (Recommended)
1. Open this folder in VS Code with the PlatformIO extension installed.
2. Update your Wi-Fi credentials in `ESP32S3_Waveshare28_Radio.ino` (`WIFI_SSID` and `WIFI_PASS`).
3. Click the PlatformIO **Build** (✓) and **Upload** (→) icons.

### Option B: Arduino IDE 2.x
1. Open [`ESP32S3_Waveshare28_Radio.ino`](file:///d:/ESP32Radio/ESP32S3_Waveshare28_Radio/ESP32S3_Waveshare28_Radio.ino).
2. Install libraries via Library Manager:
   * **LVGL** (v8.3.11)
   * **ESP32-audioI2S** (by schreibfaul1)
3. Select Board: **ESP32S3 Dev Module**.
4. Configure Tools Menu:
   * **Flash Size**: 16MB (128Mb)
   * **Partition Scheme**: 16MB Flash (3MB APP / 9.9MB FATFS or Default 16MB)
   * **PSRAM**: OPI PSRAM
   * **USB CDC On Boot**: Enabled
5. Click **Upload**.
