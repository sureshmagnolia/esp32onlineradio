#include "audio_engine.h"
#include "Audio.h"

static Audio audio;
static bool isPlaying = false;
static int currentStation = 0;
static int currentVolumePercent = 80;
static char trackTitle[128] = "Live Internet Stream";
static portMUX_TYPE audioMux = portMUX_INITIALIZER_UNLOCKED;

// FreeRTOS Task on Core 0
static void vAudioTask(void* pvParameters) {
  for (;;) {
    if (isPlaying) {
      audio.loop();
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void audio_engine_init() {
  audio.setPinout(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DOUT);
  audio_set_volume(currentVolumePercent);

  // Pin audio engine to Core 0 with priority 5
  xTaskCreatePinnedToCore(
    vAudioTask,
    "AudioTask",
    8192,
    NULL,
    5,
    NULL,
    0 // Core 0
  );
  Serial.println("[AUDIO] Core 0 Audio Task initialized");
}

void audio_play_station(int index) {
  if (index < 0 || index >= (int)NUM_DEFAULT_STATIONS) index = 0;
  
  portENTER_CRITICAL(&audioMux);
  currentStation = index;
  isPlaying = true;
  snprintf(trackTitle, sizeof(trackTitle), "Connecting to %s...", DEFAULT_STATIONS[index].name);
  portEXIT_CRITICAL(&audioMux);

  audio.stopSong();
  Serial.printf("[AUDIO] Tuning station %d: %s -> %s\n", index, DEFAULT_STATIONS[index].name, DEFAULT_STATIONS[index].url);
  audio.connecttohost(DEFAULT_STATIONS[index].url);
}

void audio_toggle_play() {
  if (isPlaying) {
    audio.stopSong();
    isPlaying = false;
    portENTER_CRITICAL(&audioMux);
    snprintf(trackTitle, sizeof(trackTitle), "Radio Paused");
    portEXIT_CRITICAL(&audioMux);
    Serial.println("[AUDIO] Paused");
  } else {
    audio_play_station(currentStation);
  }
}

void audio_set_volume(int vol) {
  if (vol < 0) vol = 0;
  if (vol > 100) vol = 100;
  currentVolumePercent = vol;
  // ESP32-audioI2S volume scale is 0 to 21
  int hwVol = map(vol, 0, 100, 0, 21);
  audio.setVolume(hwVol);
  Serial.printf("[AUDIO] Volume: %d%% (HW: %d)\n", vol, hwVol);
}

int audio_get_volume() {
  return currentVolumePercent;
}

void audio_next_station() {
  int nextIdx = (currentStation + 1) % NUM_DEFAULT_STATIONS;
  audio_play_station(nextIdx);
}

void audio_prev_station() {
  int prevIdx = (currentStation - 1 + NUM_DEFAULT_STATIONS) % NUM_DEFAULT_STATIONS;
  audio_play_station(prevIdx);
}

bool audio_is_playing() {
  return isPlaying;
}

int audio_get_current_station_idx() {
  return currentStation;
}

const char* audio_get_station_name() {
  return DEFAULT_STATIONS[currentStation].name;
}

const char* audio_get_track_title() {
  return trackTitle;
}

// Callbacks from ESP32-audioI2S
void audio_showstreamtitle(const char *info) {
  if (info && strlen(info) > 0) {
    portENTER_CRITICAL(&audioMux);
    strncpy(trackTitle, info, sizeof(trackTitle) - 1);
    trackTitle[sizeof(trackTitle) - 1] = '\0';
    portEXIT_CRITICAL(&audioMux);
    Serial.printf("[STREAM META] %s\n", info);
  }
}
