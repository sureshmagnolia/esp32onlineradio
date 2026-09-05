# ESP32-S3 JC3248W535 Internet & SD Radio - "AllIsWell" Golden Baseline Backup

## Status: VERIFIED & WORKING PERFECTLY
- **Stream Formats Supported**:
  - **HLS (.m3u8) Streams**: Live AAC & MPEG-TS (.ts) streams with seamless inter-chunk transitions (zero stutter, zero silence gaps). Verified on **Akashvani Thrissur**, Vividh Bharati, Live News 24x7, etc.
  - **MP3 HTTPS Streams**: Direct Icecast/Shoutcast HTTPS/HTTP streams.
  - **SD Card Local Audio**: MP3/AAC local playback from MicroSD.
  - **Bluetooth Audio**: A2DP receiver mode.
- **Display & Touch**: 3.5" 320x480 IPS display with capacitive touch (AXS15231B touch controller + QSPI LCD driver + LVGL 8.x).
- **Power & Sleep**: Deep sleep standby with touch screen wakeup prompt.
- **Web Remote Control**: Built-in REST API and web controller at `http://<device-ip>/`.

---

## Key Technical Breakthroughs in this Version
1. **HTTP 302 Redirection**:
   - `Audio.cpp` handles 302 redirects (e.g. `radio.wavespb.com` -> `d1cvqgmbcpg5yn.cloudfront.net`) with clean socket close and target SSL renegotiation.
2. **HLS Gap & Stutter Elimination**:
   - Fixed `readPlayListData()` playlist truncation bug (`else if (m_audioFileSize)`).
   - Replaced static 10-second wait timer with dynamic fast polling (500–1500ms).
   - Interleaved `performAudioTask()` inside HTTP header reads and TS demux loops to maintain uninterrupted I2S DMA feeding.
3. **16-Bit MSB Alignment for NS4168 Amplifier**:
   - FAAD2 AAC decoder configured for 16-bit PCM output (`FAAD_FMT_16BIT`) shifted up 16 bits for MSB-aligned I2S slots.
4. **PSRAM Memory Stability**:
   - Octal PSRAM configured for mbedtls SSL buffers (`mbedtls_platform_set_calloc_free`), avoiding internal SRAM exhaustion.

---

## Build & Upload Commands

### Compile
```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB --libraries d:\ESP32Radio\libraries d:\ESP32Radio\ESP32S3_JC3248W535_Radio
```

### Upload
```powershell
arduino-cli upload -p COM8 --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB d:\ESP32Radio\ESP32S3_JC3248W535_Radio
```
