#include "web_server.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include "web_ui.h"
#include "audio_engine.h"
#include "ui_engine.h"

static WebServer server(80);
static NameboardData currentNameboard = {
  "SURESH KUMAR",
  "Principal IoT & Embedded Systems Architect",
  "Magnolia Technologies Inc. • Lab 402",
  "IN A MEETING — DO NOT DISTURB"
};

void web_server_set_nameboard_data(const NameboardData& data) {
  currentNameboard = data;
}

NameboardData web_server_get_nameboard_data() {
  return currentNameboard;
}

static void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

static void handleStatus() {
  StaticJsonDocument<512> doc;
  
  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  int h12 = ti.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d %s", h12, ti.tm_min, (ti.tm_hour >= 12) ? "PM" : "AM");
  
  const char* daysOfWeek[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
  const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  char dateBuf[32];
  snprintf(dateBuf, sizeof(dateBuf), "%s, %02d %s %04d", daysOfWeek[ti.tm_wday % 7], ti.tm_mday, months[ti.tm_mon % 12], ti.tm_year + 1900);

  doc["time"] = timeBuf;
  doc["date"] = dateBuf;
  doc["mode"] = (int)ui_get_mode();
  doc["radio_playing"] = audio_is_playing();
  doc["radio_station"] = audio_get_station_name();
  doc["radio_track"] = audio_get_track_title();
  doc["volume"] = audio_get_volume();
  doc["station_index"] = audio_get_current_station_idx();
  
  doc["nb_name"] = currentNameboard.name;
  doc["nb_title"] = currentNameboard.title;
  doc["nb_org"] = currentNameboard.org;
  doc["nb_status"] = currentNameboard.status;

  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

static void handleMode() {
  if (server.hasArg("set")) {
    String m = server.arg("set");
    ui_clear_for_transition();
    if (m == "clock") {
      ui_set_mode(APP_MODE_CLOCK);
    } else if (m == "radio") {
      ui_set_mode(APP_MODE_RADIO);
      ui_update_radio(audio_get_station_name(), audio_get_track_title(), audio_get_volume(), audio_get_current_station_idx(), NUM_DEFAULT_STATIONS);
    } else if (m == "nameboard") {
      ui_set_mode(APP_MODE_NAMEBOARD);
      ui_update_nameboard(currentNameboard);
    }
  }
  server.send(200, "text/plain", "OK");
}

static void handleRadio() {
  if (server.hasArg("cmd")) {
    String cmd = server.arg("cmd");
    if (cmd == "toggle") {
      audio_toggle_play();
    } else if (cmd == "next") {
      audio_next_station();
    } else if (cmd == "prev") {
      audio_prev_station();
    } else if (cmd == "play" && server.hasArg("id")) {
      int id = server.arg("id").toInt();
      audio_play_station(id);
    } else if (cmd == "vol" && server.hasArg("val")) {
      int v = server.arg("val").toInt();
      audio_set_volume(v);
    }
  }
  server.send(200, "text/plain", "OK");
}

static void handleNameboard() {
  if (server.hasArg("name")) strncpy(currentNameboard.name, server.arg("name").c_str(), sizeof(currentNameboard.name) - 1);
  if (server.hasArg("title")) strncpy(currentNameboard.title, server.arg("title").c_str(), sizeof(currentNameboard.title) - 1);
  if (server.hasArg("org")) strncpy(currentNameboard.org, server.arg("org").c_str(), sizeof(currentNameboard.org) - 1);
  if (server.hasArg("status")) strncpy(currentNameboard.status, server.arg("status").c_str(), sizeof(currentNameboard.status) - 1);

  ui_clear_for_transition();
  ui_set_mode(APP_MODE_NAMEBOARD);
  ui_update_nameboard(currentNameboard);
  server.send(200, "text/plain", "OK");
}

static void handleWipe() {
  ui_force_full_wipe();
  server.send(200, "text/plain", "OK");
}

void web_server_init() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/mode", HTTP_POST, handleMode);
  server.on("/api/radio", HTTP_POST, handleRadio);
  server.on("/api/nameboard", HTTP_POST, handleNameboard);
  server.on("/api/wipe", HTTP_POST, handleWipe);

  server.begin();
  Serial.println("[WEB] Server started on port 80");
}

void web_server_handle() {
  server.handleClient();
}
