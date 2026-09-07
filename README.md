# 📻 ESP32 & ESP32-S3 Online Radio Suite

A comprehensive, production-tested suite of internet radio firmwares, custom 3D enclosures, and laser-cutting designs for ESP32 and ESP32-S3 microcontrollers.

---

## 📑 Repository Structure & Components

```
D:\ESP32Radio\
├── ESP32S3_JC3248W535_Radio_AllIsWell\  ⭐ Canonical Golden Baseline for Guition 3.5" Touch Radio
├── ESP32S3_Waveshare28_Radio\          📻 Standalone Firmware for Waveshare 2.8" Touch Radio
├── 3D_Enclosure_JC3248W535\            🖨️ 3D Enclosure Models & Virtual Fit Audits (JC3248W535)
├── Laser_Cuts\                         ⚡ Laser Cutting Profiles (Creality Falcon 5W: DXF, SVG, G-code)
├── 3D_Enclosure_Waveshare28\           🖨️ 3D Enclosure & Assembly Models (Waveshare 2.8")
├── libraries\                          📚 Custom Patched Libraries (ESP32-audioI2S, LVGL 8.4, ESP32-A2DP)
├── stations_full.json                  🌐 Complete Master Radio Station Database (JSON)
├── generate_stations_header.py         🛠️ Station Header Generator (Generates stations_db.h)
├── read_serial.py                      🔍 Lightweight ESP32 CDC Serial Monitor Script
└── README.md                           📖 Master Repository Guide
```

---

## 🌟 1. Flagship: ESP32-S3 JC3248W535 3.5" Smart Touch Radio ("AllIsWell")

The **primary flagship** standalone internet radio powered by the **Guition JC3248W535** (ESP32-S3 with 16MB Flash, 8MB Octal PSRAM, 3.5" 320x480 IPS display with AXS15231B touch controller, and NS4168 I2S amplifier).

- **Folder**: [`ESP32S3_JC3248W535_Radio_AllIsWell/`](file:///d:/ESP32Radio/ESP32S3_JC3248W535_Radio_AllIsWell)
- **Detailed Documentation**: [`README_AllIsWell.md`](file:///d:/ESP32Radio/ESP32S3_JC3248W535_Radio_AllIsWell/README_AllIsWell.md)
- **Audio Capabilities**:
  - **HLS (.m3u8) Live AAC & MPEG-TS**: Seamless inter-chunk transitions on live Indian streams (Akashvani Thrissur, FM Rainbow Kochi, Vividh Bharati, AIR Malayalam, etc.).
  - **Direct MP3 / AAC HTTPS Streams**: High-stability Icecast / Shoutcast webstreams.
  - **MicroSD Audio Player**: Local MP3/AAC playback with auto-advance and shuffle.
  - **10-Attempt Connection Retry Engine**: Automatically retries failed network connections spaced by 2.5s without interrupting live playback.
- **Everyday Auto-On Alarm & Sleep Timer**:
  - Direct touch modal accessible by tapping the clock card.
  - Hour, Minute, interactive AM/PM touch toggle button, and duration selector (`15m`, `30m`, `45m`, `60m`, `90m`, `120m`, `Continuous`).
  - **Standby Power Recovery**: Silently syncs NTP over Wi-Fi with screen off after cold boot during standby to accurately arm hardware RTC deep sleep wakeup.
  - **Background Wi-Fi Watchdog**: Health check every 20s ensures radio stays online 24/7 when idle.
  - **Casual Touch Friendly**: Volume adjustments or station browsing during alarm playback do not cancel the auto-off session; only explicit **Pause** or **Power Off** terminates the alarm.
- **Web Remote Controller (`http://<ip>/`)**:
  - REST interface for volume, station selection, timer settings, and custom station management.
- **Zero-Crash Architecture**:
  - FreeRTOS HTTP task runs on PSRAM stack (`MALLOC_CAP_SPIRAM`), while all Flash/NVS writes are safely dispatched to Core 1 (`loopTask`), completely preventing ESP32-S3 cache disable panics.

### Build & Upload (JC3248W535):
```powershell
# Compile
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB --libraries d:\ESP32Radio\libraries d:\ESP32Radio\ESP32S3_JC3248W535_Radio_AllIsWell

# Upload (adjust COM port)
arduino-cli upload -p COM8 --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB d:\ESP32Radio\ESP32S3_JC3248W535_Radio_AllIsWell
```

---

## 📻 2. Waveshare ESP32-S3 Touch LCD 2.8" Hi-Fi Radio

Standalone firmware dedicated to the **Waveshare ESP32-S3-Touch-LCD-2.8** development board.

- **Folder**: [`ESP32S3_Waveshare28_Radio/`](file:///d:/ESP32Radio/ESP32S3_Waveshare28_Radio)
- **Detailed Documentation**: [`README.md`](file:///d:/ESP32Radio/ESP32S3_Waveshare28_Radio/README.md)
- **Hardware Profile**:
  - Display: 2.8" ST7789 SPI LCD ($320 \times 240$) in landscape orientation (buttons at top edge).
  - Touch: CST328 capacitive touch controller on I2C.
  - Audio: PCM5101 I2S Hi-Fi DAC.
  - Multi-function physical `BOOT` button: Single tap = Next, Double tap = Prev, Long press = Volume ramp, Standby = Wakeup.
- **Enclosure**: Complete desktop printable enclosure model in [`3D_Enclosure_Waveshare28/`](file:///d:/ESP32Radio/3D_Enclosure_Waveshare28).

---

## 🖨️ 3. 3D Enclosures & Laser Cutting

1. **JC3248W535 Enclosure & Speaker Retrofit**:
   - Located in [`3D_Enclosure_JC3248W535/`](file:///d:/ESP32Radio/3D_Enclosure_JC3248W535).
   - Includes full 3D interactive assembly visualizations, dimensional audits, and board mounting files.
2. **Creality Falcon 5W Laser Cutting Profiles**:
   - Located in [`Laser_Cuts/`](file:///d:/ESP32Radio/Laser_Cuts).
   - Contains DXF, SVG, and optimized G-code files for cutting the speaker door display bezels:
     - **Profile A** (`85x56mm`): Flush body fit.
     - **Profile B** (`75x51mm`): Screen window cutout.
     - **Profile C** (`96x63mm`): Full front glass window.
   - Includes 1:1 printable calibration PDF template and alignment visualizer.
3. **Waveshare 2.8" Enclosure**:
   - Located in [`3D_Enclosure_Waveshare28/`](file:///d:/ESP32Radio/3D_Enclosure_Waveshare28).
   - STL and OpenSCAD parametric files with virtual fit verification scripts.

---

## 🛠️ Station Database Management

- **Master Database**: [`stations_full.json`](file:///d:/ESP32Radio/stations_full.json) contains the complete, curated station directory (genres, streams, bitrates, logos).
- **Generator Script**: [`generate_stations_header.py`](file:///d:/ESP32Radio/generate_stations_header.py) compiles the JSON database into the optimized C++ header [`stations_db.h`](file:///d:/ESP32Radio/ESP32S3_JC3248W535_Radio_AllIsWell/stations_db.h) with PROGMEM storage for instant access.

To regenerate station headers:
```powershell
python generate_stations_header.py
```

---

## ⚠️ Developer & AI Guidelines

1. **Single Source of Truth**:
   - `ESP32S3_JC3248W535_Radio_AllIsWell/` is the **only** active codebase for the 3.5" radio. Do not create secondary dev or backup folders in Git tracking.
2. **No Flash Writes on PSRAM Stack**:
   - Any Web Server task running on external PSRAM stack must **never** call `Preferences` or flash writes directly. Queue the changes into thread-safe pending structs for Core 1's `loop()` to commit safely.
3. **Audio Task Integrity**:
   - Never place blocking calls or delays in `loop()`, as it directly feeds I2S audio frames.
