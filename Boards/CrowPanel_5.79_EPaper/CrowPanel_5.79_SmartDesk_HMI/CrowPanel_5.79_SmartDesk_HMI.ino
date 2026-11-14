#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include "time.h"

#include "config.h"
#include "buttons.h"
#include "ui_engine.h"
#include "audio_engine.h"
#include "web_server.h"

static WiFiMulti wifiMulti;
static int currentMinute = -1;
static unsigned long lastClockTick = 0;
static unsigned long lastRadioTick = 0;
static int menuCursorIndex = 0;
static int statusPresetIndex = 0;

static void handleButtonEvents() {
  ButtonEvent evt = buttons_poll();
  if (evt == BTN_NONE) return;

  AppMode mode = ui_get_mode();

  switch (evt) {
    case BTN_MENU: // K3 (GPIO 2)
      Serial.println("[BTN] MENU Pressed");
      if (mode == APP_MODE_MENU) {
        ui_clear_for_transition();
        ui_set_mode(APP_MODE_CLOCK);
        currentMinute = -1; // force clock re-render
      } else {
        ui_clear_for_transition();
        ui_set_mode(APP_MODE_MENU);
        ui_update_menu(menuCursorIndex);
      }
      break;

    case BTN_BACK: // K4 (GPIO 1)
      Serial.println("[BTN] BACK Pressed");
      if (mode != APP_MODE_CLOCK) {
        ui_clear_for_transition();
        ui_set_mode(APP_MODE_CLOCK);
        currentMinute = -1; // force clock re-render
      } else {
        // In clock mode, BACK button triggers anti-ghosting full screen wipe
        Serial.println("[BTN] Triggering full ghost-free wipe");
        ui_force_full_wipe();
        currentMinute = -1;
      }
      break;

    case BTN_UP: // K5-up (GPIO 6)
      Serial.println("[BTN] DIAL UP");
      if (mode == APP_MODE_MENU) {
        menuCursorIndex = (menuCursorIndex - 1 + 5) % 5;
        ui_update_menu(menuCursorIndex);
      } else if (mode == APP_MODE_CLOCK) {
        if (audio_is_playing()) {
          audio_set_volume(audio_get_volume() + 5);
          currentMinute = -1;
        }
      } else if (mode == APP_MODE_RADIO) {
        audio_next_station();
        ui_update_radio(audio_get_station_name(), audio_get_track_title(), audio_get_volume(), audio_get_current_station_idx(), NUM_DEFAULT_STATIONS);
      } else if (mode == APP_MODE_NAMEBOARD) {
        statusPresetIndex = (statusPresetIndex + 1) % NUM_STATUS_PRESETS;
        NameboardData nb = web_server_get_nameboard_data();
        strncpy(nb.status, STATUS_PRESETS[statusPresetIndex], sizeof(nb.status) - 1);
        web_server_set_nameboard_data(nb);
        ui_update_nameboard(nb);
      }
      break;

    case BTN_DOWN: // K5-down (GPIO 4)
      Serial.println("[BTN] DIAL DOWN");
      if (mode == APP_MODE_MENU) {
        menuCursorIndex = (menuCursorIndex + 1) % 5;
        ui_update_menu(menuCursorIndex);
      } else if (mode == APP_MODE_CLOCK) {
        if (audio_is_playing()) {
          audio_set_volume(audio_get_volume() - 5);
          currentMinute = -1;
        }
      } else if (mode == APP_MODE_RADIO) {
        audio_prev_station();
        ui_update_radio(audio_get_station_name(), audio_get_track_title(), audio_get_volume(), audio_get_current_station_idx(), NUM_DEFAULT_STATIONS);
      } else if (mode == APP_MODE_NAMEBOARD) {
        statusPresetIndex = (statusPresetIndex - 1 + NUM_STATUS_PRESETS) % NUM_STATUS_PRESETS;
        NameboardData nb = web_server_get_nameboard_data();
        strncpy(nb.status, STATUS_PRESETS[statusPresetIndex], sizeof(nb.status) - 1);
        web_server_set_nameboard_data(nb);
        ui_update_nameboard(nb);
      }
      break;

    case BTN_OK: // K5-click (GPIO 5)
      Serial.println("[BTN] DIAL OK Click");
      if (mode == APP_MODE_MENU) {
        ui_clear_for_transition();
        if (menuCursorIndex == 0) {
          ui_set_mode(APP_MODE_CLOCK);
          currentMinute = -1;
        } else if (menuCursorIndex == 1) {
          ui_set_mode(APP_MODE_RADIO);
          if (!audio_is_playing()) audio_toggle_play();
          ui_update_radio(audio_get_station_name(), audio_get_track_title(), audio_get_volume(), audio_get_current_station_idx(), NUM_DEFAULT_STATIONS);
        } else if (menuCursorIndex == 2) {
          ui_set_mode(APP_MODE_NAMEBOARD);
          ui_update_nameboard(web_server_get_nameboard_data());
        } else if (menuCursorIndex == 3) {
          statusPresetIndex = (statusPresetIndex + 1) % NUM_STATUS_PRESETS;
          NameboardData nb = web_server_get_nameboard_data();
          strncpy(nb.status, STATUS_PRESETS[statusPresetIndex], sizeof(nb.status) - 1);
          web_server_set_nameboard_data(nb);
          ui_set_mode(APP_MODE_NAMEBOARD);
          ui_update_nameboard(nb);
        } else if (menuCursorIndex == 4) {
          ui_set_mode(APP_MODE_RADIO);
          ui_update_radio(audio_get_station_name(), audio_get_track_title(), audio_get_volume(), audio_get_current_station_idx(), NUM_DEFAULT_STATIONS);
        }
      } else if (mode == APP_MODE_CLOCK) {
        audio_toggle_play();
        ui_clear_for_transition();
        currentMinute = -1;
      } else if (mode == APP_MODE_RADIO) {
        audio_toggle_play();
        ui_update_radio(audio_get_station_name(), audio_get_track_title(), audio_get_volume(), audio_get_current_station_idx(), NUM_DEFAULT_STATIONS);
      } else if (mode == APP_MODE_NAMEBOARD) {
        // Cycle to next status preset
        statusPresetIndex = (statusPresetIndex + 1) % NUM_STATUS_PRESETS;
        NameboardData nb = web_server_get_nameboard_data();
        strncpy(nb.status, STATUS_PRESETS[statusPresetIndex], sizeof(nb.status) - 1);
        web_server_set_nameboard_data(nb);
        ui_update_nameboard(nb);
      }
      break;

    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n=======================================================");
  Serial.println("  CROWPANEL 5.79\" SMART DESK CONSOLE STARTING");
  Serial.println("  Internet Radio + Fullscreen Clock + Tabletop Plaque");
  Serial.println("=======================================================");

  // 1. Initialize Display & Buttons
  ui_engine_init();
  buttons_init();

  // 2. Startup Frame
  ui_render_startup("STEP 1: CONNECTING TO WI-FI...", "Checking SSIDs: suresh, suresh2.4gExt");

  // 3. Setup WiFi
  for (size_t i = 0; i < NUM_DEFAULT_WIFI; i++) {
    wifiMulti.addAP(DEFAULT_WIFI_NETWORKS[i].ssid, DEFAULT_WIFI_NETWORKS[i].pass);
  }

  int wifiAttempts = 0;
  while (wifiMulti.run() != WL_CONNECTED && wifiAttempts < 30) {
    delay(400);
    wifiAttempts++;
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WIFI] Connected to %s, IP: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    char ipMsg[64];
    snprintf(ipMsg, sizeof(ipMsg), "WiFi: %s  •  IP: %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    ui_render_startup("STEP 2: NTP TIME SYNCHRONIZATION...", ipMsg);
  } else {
    Serial.println("[WIFI] WiFi connect failed, continuing offline");
    ui_render_startup("STEP 2: OFFLINE MODE", "WiFi not connected, starting local clock");
  }

  // 4. Configure NTP Time
  configTime(DEFAULT_GMT_OFFSET_SEC, DEFAULT_DAYLIGHT_OFFSET_SEC, "time.google.com", "asia.pool.ntp.org", "pool.ntp.org");
  struct tm timeinfo;
  for (int i = 0; i < 20; i++) {
    if (getLocalTime(&timeinfo, 200)) {
      Serial.printf("[NTP] Synced time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      break;
    }
    delay(100);
  }

  // 5. Initialize Core 0 Audio Engine & Web Server
  audio_engine_init();
  web_server_init();

  // 6. Transition to Default Mode (Fullscreen Clock)
  ui_force_full_wipe();
  ui_set_mode(APP_MODE_CLOCK);
  currentMinute = -1; // Trigger immediate render
}

void loop() {
  // Handle HTTP REST API & Web App requests
  web_server_handle();

  // Handle right-edge physical buttons & dial wheel
  handleButtonEvents();

  AppMode mode = ui_get_mode();

  // Clock Update Check
  if (mode == APP_MODE_CLOCK) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    bool radioPlaying = audio_is_playing();
    static bool lastRadioPlaying = false;
    if (radioPlaying != lastRadioPlaying) {
      lastRadioPlaying = radioPlaying;
      ui_clear_for_transition();
      currentMinute = -1; // force immediate clean render
    }

    // Check if minute changed, or if radio playback state changed, or periodic ticker
    if (timeinfo.tm_min != currentMinute || (radioPlaying && (millis() - lastRadioTick > 15000))) {
      currentMinute = timeinfo.tm_min;
      lastRadioTick = millis();
      ui_update_clock(
        timeinfo,
        radioPlaying,
        audio_get_station_name(),
        audio_get_volume(),
        audio_get_track_title()
      );
    }
  } else if (mode == APP_MODE_RADIO) {
    // Radio screen update (every 10s for track metadata refresh)
    if (millis() - lastRadioTick > 10000) {
      lastRadioTick = millis();
      ui_update_radio(
        audio_get_station_name(),
        audio_get_track_title(),
        audio_get_volume(),
        audio_get_current_station_idx(),
        NUM_DEFAULT_STATIONS
      );
    }
  }

  delay(20);
}
