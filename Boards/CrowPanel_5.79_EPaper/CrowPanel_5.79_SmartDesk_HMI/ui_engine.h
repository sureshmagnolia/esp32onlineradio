#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"

enum AppMode {
  APP_MODE_CLOCK = 0,
  APP_MODE_RADIO,
  APP_MODE_NAMEBOARD,
  APP_MODE_MENU
};

struct NameboardData {
  char name[48];
  char title[64];
  char org[64];
  char status[64];
};

void ui_engine_init();
void ui_set_mode(AppMode mode);
AppMode ui_get_mode();

void ui_update_clock(const struct tm& timeinfo, bool radioPlaying, const char* radioStation, int volume, const char* radioTrack);
void ui_update_radio(const char* station, const char* track, int volume, int stationIdx, int totalStations);
void ui_update_nameboard(const NameboardData& data);
void ui_update_menu(int selectedIndex);

void ui_render_startup(const char* stepMsg, const char* subMsg);
void ui_force_full_wipe();
void ui_clear_for_transition();
