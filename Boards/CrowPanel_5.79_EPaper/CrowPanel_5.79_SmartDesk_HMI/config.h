#pragma once
#include <Arduino.h>

// ==============================================================================
// HARDWARE PIN DEFINITIONS (Elecrow CrowPanel 5.79" ESP32-S3)
// ==============================================================================
// Power Control Pins
#define PIN_EPD_PWR       7    // Display Power Converter Enable (Active HIGH)
#define PIN_TF_PWR        42   // TF Card & Peripherals Power Enable (Active HIGH)
#define PIN_POWER_LED     41   // Green Status LED

// Navigation Buttons & Dial Switch (ALL on the RIGHT side of the panel)
#define PIN_MENU_KEY      2    // K3: Upper Tactile Button (Menu / Home)
#define PIN_EXIT_KEY      1    // K4: Lower Tactile Button (Back / Wipe Clear)
#define PIN_DIAL_UP       6    // K5: Dial Wheel Roller UP
#define PIN_DIAL_OK       5    // K5: Dial Wheel Center PUSH / Click
#define PIN_DIAL_DN       4    // K5: Dial Wheel Roller DOWN

// I2S Digital Audio Output (Expansion Header Pins -> External I2S DAC MAX98357A / PCM5102A)
#define PIN_I2S_BCLK      17   // I2S Bit Clock
#define PIN_I2S_LRC       18   // I2S Word Select / Left-Right Clock
#define PIN_I2S_DOUT      19   // I2S Serial Data Out

// E-Paper Panel Geometry
#define DISPLAY_WIDTH     792
#define DISPLAY_HEIGHT    272

// Timezone (India Standard Time UTC +5:30)
#define DEFAULT_GMT_OFFSET_SEC      19800
#define DEFAULT_DAYLIGHT_OFFSET_SEC 0

// WiFi AP List
struct WifiCredential {
  const char* ssid;
  const char* pass;
};

const WifiCredential DEFAULT_WIFI_NETWORKS[] = {
  {"suresh", "alangium"},
  {"suresh2.4gExt", "alangium"}
};
const size_t NUM_DEFAULT_WIFI = sizeof(DEFAULT_WIFI_NETWORKS) / sizeof(DEFAULT_WIFI_NETWORKS[0]);

// Radio Station Structure
struct RadioStation {
  const char* name;
  const char* url;
  const char* genre;
};

const RadioStation DEFAULT_STATIONS[] = {
  {"BBC WORLD SERVICE",  "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service", "News / Global"},
  {"RADIO PARADISE",     "http://stream.radioparadise.com/mp3-192",                "Eclectic / Rock"},
  {"RP MELLOW MIX",      "http://stream.radioparadise.com/mellow-192",             "Acoustic / Chill"},
  {"CLASSIC FM",         "http://media-the.musicradio.com/ClassicFMMP3",           "Classical"},
  {"LINN JAZZ",          "http://radio.linn.co.uk:8000/autodj",                    "Jazz / High-Res"},
  {"CHILL TRAP LOBBY",   "http://stream.zeno.fm/f3wvbbqmdg8uv",                    "Lo-Fi / Beats"},
  {"THRISSR AIR",        "https://airrelay.onrender.com/thrissur.mp3",             "Malayalam / Regional"},
  {"SMOOTH LOUNGE",      "http://198.58.98.83:8258/stream",                        "Lounge / Ambient"}
};
const size_t NUM_DEFAULT_STATIONS = sizeof(DEFAULT_STATIONS) / sizeof(DEFAULT_STATIONS[0]);

// Status Presets for Desk Nameplate
const char* const STATUS_PRESETS[] = {
  "IN A MEETING — DO NOT DISTURB",
  "DEEP FOCUS — PLEASE DO NOT DISTURB",
  "AVAILABLE — FEEL FREE TO TALK",
  "ON A CALL — BACK SHORTLY",
  "LUNCH BREAK — BACK AT 2:00 PM",
  "WORKING REMOTELY TODAY"
};
const size_t NUM_STATUS_PRESETS = sizeof(STATUS_PRESETS) / sizeof(STATUS_PRESETS[0]);
