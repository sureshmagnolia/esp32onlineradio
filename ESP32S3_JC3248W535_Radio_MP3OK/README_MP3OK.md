# ESP32-S3 JC3248W535 Internet Radio - [VERIFIED MP3 OK VERSION]

## Status: VERIFIED WORKING & TESTED
- **Date:** September 5, 2026
- **Tested Stream:** `https://airrelay.onrender.com/thrissur.mp3` (Thrissur Station, Malayalam)
- **Audio Output:** Clear, continuous, high-quality audio playback through the onboard digital I2S amplifier & speaker.
- **Stability:** Zero core panics / zero LoadProhibited crashes.
  - FreeRTOS audio task conflict resolved by driving `audio.loop()` synchronously in `loop()`.
  - Display non-ASCII character renderer sanitized (em-dash fixed).
  - Wi-Fi connection and station database fully intact.

## Files:
- `ESP32S3_JC3248W535_Radio_MP3OK.ino` - Main Arduino sketch (matches folder name for direct opening in Arduino IDE).
- `ESP32S3_JC3248W535_Radio.ino` - Main sketch with original naming for PlatformIO.
- `stations_db.h` - Contains built-in station database including Thrissur (Custom MP3 URL at index 0).
- All BSP drivers, LVGL configuration, and pin configurations (`pincfg.h`).

This folder is preserved as a clean golden backup. Future feature work or experimental changes should be made on the working directory `ESP32S3_JC3248W535_Radio`.
