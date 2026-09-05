# 📻 ESP32-S3 JC3248W535 Internet & SD Radio — "AllIsWell" Golden Baseline

## 🌟 Status: VERIFIED & WORKING PERFECTLY (September 2026)

This repository contains the production-grade firmware for the **Guition JC3248W535 (ESP32-S3 16MB Flash / 8MB Octal PSRAM + 3.5" 320x480 IPS Display with AXS15231B Touch)** internet and local audio radio player.

---

## 🎯 Key Features & Capabilities

1. **Audio Streaming Engine**:
   - **HLS (.m3u8) Live AAC & MPEG-TS (.ts)**: Verified on Indian live radio streams (Akashvani Thrissur, FM Rainbow Kochi, Vividh Bharati, AIR Malayalam, etc.) with seamless inter-chunk transitions and zero stutter.
   - **Direct MP3 / AAC HTTPS Streams**: Icecast / Shoutcast webstreams with HTTPS SSL.
   - **MicroSD Audio Player**: Local MP3/AAC playback with auto-advance and shuffle mode.
   - **10-Attempt Connection Retry Engine**: Automatic connection retries (spaced by 2.5s) upon connection failures or buffering stalls (>12s) before declaring a station offline, without interrupting ongoing playback.
2. **Interactive 3.5" Touch UI (LVGL 8.x + AXS15231B)**:
   - **Modern Split Dashboard**: Live station display, station metadata, volume control, mute, audio visualizer, battery gauge, and digital clock.
   - **On-Device Auto-On Alarm & Sleep Timer**:
     - Dedicated touch modal accessible directly by tapping the clock card.
     - Hour (`+`/`-`), Minute (`+`/`-`), Enable/Disable toggle, and Duration selector (`15m`, `30m`, `45m`, `60m`, `90m`, `120m`, `Continuous`).
     - Dynamic clock badge: `🔔 06:30AM (30m)` or `🔔 OFF (Tap)`.
     - When alarm triggers, display backlight turns on, radio begins playback, and automatically shuts off after the set duration. Any user touch cancels auto-off.
3. **Web Remote Controller (`http://<device-ip>/`)**:
   - Clean, lightweight REST-driven web application served directly from flash memory.
   - Volume slider, station playback, status polling, and timer management.
   - **Custom Stations Management**: Add, edit, delete, and tune into personal user radio streams.
   - **Wi-Fi Settings intentionally removed**: Device operates cleanly without exposing sensitive Wi-Fi setup to the remote interface.

---

## 🧠 Architectural Highlights & Critical AI Guidelines

When modifying or extending this codebase, **any AI or developer must adhere to these established rules**:

### 1. Dual-Core Task Separation
- **Core 0**: Handles Wi-Fi networking, TCP/IP stack (LwIP), and the LVGL display rendering task (`lvgl_port_task`).
- **Core 1**: Dedicated to `loopTask` running `audio.loop()` feeding I2S DMA to the NS4168 amplifier.
- **Rule**: Never run heavy CPU-intensive loops or blocking operations on Core 1 inside `loop()`.

### 2. The ESP32-S3 PSRAM Flash Write Trap (CRITICAL)
- **Problem**: On ESP32-S3, writing to SPI Flash / NVS (`Preferences::putString`, `Preferences::putInt`, etc.) temporarily disables cache to external PSRAM (`MALLOC_CAP_SPIRAM`). If a FreeRTOS task whose stack is in PSRAM hits a flash write, accessing the stack pointer (`SP`) immediately causes a hardware panic:
  `Guru Meditation Error: Core 1 panic'ed (Cache disabled but cached memory region accessed)`
- **Solution**:
  - The HTTP web server task stack is allocated in PSRAM (`MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT`) with a 10KB stack to preserve all internal SRAM for I2S DMA.
  - **The web server task MUST NEVER write to Flash/NVS directly.**
  - All web actions that modify flash (such as adding custom stations or saving timer preferences) write to thread-safe structs (`pendingAddStation`, `pendingTimerSave`, `pendingDeleteStationIdx`) and set a `pending = true` flag.
  - Core 1's `loop()` (whose stack resides in Internal SRAM) executes the actual flash write safely.

### 3. I2S DMA Memory Preservation
- Do not allocate large buffers in internal SRAM. The web server uses PSRAM, and mbedtls SSL buffers use external PSRAM via custom platform allocators (`mbedtls_platform_set_calloc_free`), preventing internal SRAM exhaustion.

---

## 🛠️ Build & Flash Instructions

### Prerequisites
- **Arduino CLI** installed and added to `PATH`.
- **ESP32 Arduino Core** v3.x (`esp32:esp32`).
- **Libraries Directory**: `d:\ESP32Radio\libraries` (includes customized `ESP32-audioI2S`).

### Board Configuration / FQBN
```powershell
esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB
```

### 1. Compile
```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB --libraries d:\ESP32Radio\libraries d:\ESP32Radio\ESP32S3_JC3248W535_Radio_AllIsWell
```

### 2. Upload / Flash
```powershell
arduino-cli upload -p COM8 --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB d:\ESP32Radio\ESP32S3_JC3248W535_Radio_AllIsWell
```

---

## 📌 Hardware Pinout Reference (JC3248W535)

| Periperhal | Pin | Description |
|---|---|---|
| **I2S BCLK** | GPIO 47 | Bit Clock to NS4168 Amplifier |
| **I2S LRC** | GPIO 21 | Word Select (Left/Right Clock) |
| **I2S DOUT** | GPIO 14 | Serial Audio Data |
| **I2S MCLK** | -1 | Not connected (BCLK derived) |
| **LCD QSPI** | GPIO 11, 12, 13, 15, 16, 17 | AXS15231B QSPI Interface |
| **LCD Backlight** | GPIO 48 | Backlight PWM control |
| **Touch I2C** | GPIO 4, 8 | AXS15231B Capacitive Touch SDA/SCL |
| **MicroSD** | GPIO 38, 39, 40 | SD_MMC 1-line mode |
