# 📻 ESP32 Multi-Board Audio & HMI Ecosystem

Master development workspace for custom ESP32/ESP32-S3 Internet Radios, Smart Clocks, and HMI displays across diverse hardware boards.

---

## 🗂️ Clean Repository Organization

All hardware boards and firmware are isolated inside `Boards/` so codes and dependencies **never mix up**. Shared Arduino/ESP-IDF libraries, master station catalogs, and tools reside in `Common/`.

```
D:\ESP32Radio\
├── Boards\
│   ├── CrowPanel_5.79_EPaper\          ⭐ Elecrow CrowPanel 5.79" E-Paper HMI (SSD1683 Dual-controller)
│   │   ├── CrowPanel_5.79_SmartDesk_HMI\ # Desk Clock + Web Radio + Nameboard + Web Server
│   │   ├── example\arduino\Examples\5.79_WiFi_Clock\ # Standalone NTP WiFi Clock
│   │   ├── tools_and_previews\          # Display crop previews, HUD generators & mock tests
│   │   ├── example\                     # Official manufacturer SDK and demos
│   │   └── CROWPANEL_5.79_MASTER_GUIDE.md # Pinout, display timings & documentation
│   │
│   ├── CrowPanel_1.28_Rotary\          ⭐ Elecrow CrowPanel 1.28" Rotary Touch Dial (GC9A01 + CST816D)
│   │   ├── CrowPanel_1.28_ESP32_HMI_Radio\ # Dual-Core Studio Radio, LVGL 8.3, Web Remote & Simulator
│   │   ├── 3D_Printable_Speaker_Base\      # Custom 3D printable podium mount, MAX98357A chamber & wiring
│   │   └── Hardware_and_SDK\            # Schematics, datasheets, 3D files & factory demos
│   │
│   ├── JC3248W535_3.5_Touch\           ⭐ Guition 3.5" Capacitive Touch Display (AXS15231B 320x480)
│   │   ├── ESP32S3_JC3248W535_Radio_AllIsWell\ # Primary PlatformIO Radio firmware
│   │   ├── ESP32S3_JC3248W535_Radio_fixed\     # Fixed audio/display variant
│   │   ├── ESP32S3_JC3248W535_Radio_MP3OK\     # MP3 playback variant
│   │   ├── ESP32S3_JC3248W535_SmartClock\      # Dedicated Smart Clock firmware
│   │   ├── Hardware_and_SDK\                   # Factory documentation, schematics & manual
│   │   └── Enclosures\                         # 3D printable enclosures (standard & 3" speaker)
│   │
│   ├── ATS_Mini_SI4732\                ⭐ ATS-Mini / SI4732 All-Band DSP Radio
│   │   ├── 1_Official_ATS_Mini_Stock\   # Stock v2.27 firmware backup, official binaries & restore tool
│   │   └── 2_Hybrid_SI4732_WebRadio\    # Custom hybrid FM/AM/SSB + Web Radio firmware
│   │
│   ├── Waveshare_2.8_Touch\            ⭐ Waveshare 2.8" Capacitive Touch Display
│   │   └── 3D_Enclosure_Waveshare28\    # 3D printable enclosure assets
│   │
│   ├── M5Stack_Core2\                  ⭐ M5Stack Core2 ESP32
│   │   └── M5Core2_AIR_Radio\           # M5Core2 internet radio sketch
│   │
│   ├── Headless_Modular_ESP32\         ⭐ Displayless / Modular ESP32 Radios
│   │   ├── DualBoard_WiFi_BT\           # Board 1 (WiFi Streamer) + Board 2 (Bluetooth Sink)
│   │   └── SingleBoard_Headless\        # HeadlessBoard, DisplaylessHack & TTGO-TDisplay radio
│   │
│   └── Retro_Gaming\                   ⭐ Retro Emulation
│       └── Esp32-S3-nes-emulator\       # NES emulator ported to ESP32-S3
│
├── Common\                             📚 Shared Resources (One Source of Truth)
│   ├── libraries\                      # Shared Arduino libraries: LovyanGFX, lvgl, ESP32-audioI2S, ESP32-A2DP, Adafruit_NeoPixel
│   ├── data\                           # Master 5,700+ station online catalogs (stations.json, stations_full.json)
│   ├── tools\                          # Header generator (generate_stations_header.py), serial monitor (read_serial.py)
│   └── backups\                        # Library historical snapshots
│
├── Fabrication\                        🛠️ Enclosure & Cutting Designs
│   └── Laser_Cuts\                     # Creality Falcon laser cutting designs (SVG, DXF, GCODE)
│
├── libraries\                          🔗 Directory Junction to Common\libraries (backward compatibility)
└── README.md                           📖 Master Repository Architecture Guide
```

---

## 🛠️ Build & Compilation Reference

### 1. CrowPanel 5.79" SmartDesk HMI / WiFi Clock
Using `arduino-cli`:
```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi \
  --library "Common/libraries/ESP32-audioI2S" \
  Boards/CrowPanel_5.79_EPaper/CrowPanel_5.79_SmartDesk_HMI/CrowPanel_5.79_SmartDesk_HMI.ino
```

### 2. CrowPanel 1.28" Rotary Studio Radio
Using `arduino-cli`:
```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB \
  --libraries "Common/libraries" \
  Boards/CrowPanel_1.28_Rotary/CrowPanel_1.28_ESP32_HMI_Radio/CrowPanel_1.28_ESP32_HMI_Radio.ino
```

### 3. JC3248W535 3.5" Radio
Using `PlatformIO`:
```bash
cd Boards/JC3248W535_3.5_Touch/ESP32S3_JC3248W535_Radio_AllIsWell
pio run -e esp32s3_jc3248w535
```
*(A local directory junction inside `Boards/JC3248W535_3.5_Touch/libraries` routes any `../libraries` reference directly to `Common/libraries/`).*
