#pragma once
#include <Arduino.h>
#include "config.h"

void audio_engine_init();
void audio_play_station(int index);
void audio_toggle_play();
void audio_set_volume(int vol); // 0 - 100
int  audio_get_volume();
void audio_next_station();
void audio_prev_station();

bool audio_is_playing();
int  audio_get_current_station_idx();
const char* audio_get_station_name();
const char* audio_get_track_title();
