#include "ui_engine.h"
#include "EPD.h"
#include "EPD_Init.h"
#include "MassiveFont.h"
#include <WiFi.h>

static uint8_t ImageBW[27200];
static AppMode currentMode = APP_MODE_CLOCK;

// Glyph renderer using ClockGlyph PROGMEM arrays
static void drawGlyph(int x, int y, const ClockGlyph* font, char c, uint16_t color) {
  for (int i = 0; font[i].c != 0; i++) {
    if (font[i].c == c) {
      int w = font[i].w;
      int h = font[i].h;
      const uint8_t* data = font[i].data;
      int curr = 0;
      int bit_cnt = 0;
      for (int dy = 0; dy < h; dy++) {
        int py = y + dy;
        for (int dx = 0; dx < w; dx++) {
          if (bit_cnt == 0) {
            curr = pgm_read_byte(data++);
            bit_cnt = 8;
          }
          if (curr & 0x80) {
            int px = x + dx;
            if (px >= 0 && px < 792 && py >= 0 && py < 272) {
              Paint_SetPixel(px, py, color);
            }
          }
          curr <<= 1;
          bit_cnt--;
        }
      }
      return;
    }
  }
}

static int getGlyphWidth(const ClockGlyph* font, char c) {
  for (int i = 0; font[i].c != 0; i++) {
    if (font[i].c == c) return font[i].w;
  }
  return 0;
}

static void drawString(int x, int y, const ClockGlyph* font, const char* str, int spacing, uint16_t color) {
  int curX = x;
  int spaceW = 10;
  if (font == ui_glyphs) spaceW = 7;
  else if (font == header_glyphs) spaceW = 12;
  else if (font == text_glyphs) spaceW = 14;

  while (*str) {
    char c = *str++;
    if (c == ' ') {
      curX += spaceW;
    } else {
      drawGlyph(curX, y, font, c, color);
      curX += getGlyphWidth(font, c) + spacing;
    }
  }
}

// Vector Colon for massive clock
static void drawColon(int x, int startY) {
  int cx = x + 18;
  int cy1 = startY + 54;
  int cy2 = startY + 160;
  int r = 11;
  for (int dy = -r; dy <= r; dy++) {
    for (int dx = -r; dx <= r; dx++) {
      if (dx * dx + dy * dy <= r * r) {
        int px = cx + dx;
        int py1 = cy1 + dy;
        int py2 = cy2 + dy;
        if (px >= 0 && px < 792) {
          if (py1 >= 0 && py1 < 272) Paint_SetPixel(px, py1, BLACK);
          if (py2 >= 0 && py2 < 272) Paint_SetPixel(px, py2, BLACK);
        }
      }
    }
  }
}

// ==============================================================================
// PURE ICON NAVIGATION HUD (RIGHT GUTTER X: 760..791, ZERO TEXT LABELS)
// ==============================================================================
static void draw_pure_icon_hud(bool isClockMode) {
  // Vertical boundary divider hairline
  for (int y = 10; y <= 262; y++) {
    Paint_SetPixel(760, y, BLACK);
  }

  // 1. Top Button (GPIO 2, K3) -> 3-bar Hamburger Menu icon
  EPD_DrawRectangle(766, 18, 788, 44, BLACK, 0);
  EPD_DrawLine(770, 25, 784, 25, BLACK);
  EPD_DrawLine(770, 31, 784, 31, BLACK);
  EPD_DrawLine(770, 37, 784, 37, BLACK);

  // 2. Dial UP (GPIO 6) -> Solid Up Arrow
  for (int y = 0; y <= 16; y++) {
    int w = y;
    int curY = 72 + y;
    for (int x = 777 - w; x <= 777 + w; x++) {
      if (curY >= 72 && curY <= 88 && x >= 767 && x <= 787) Paint_SetPixel(x, curY, BLACK);
    }
  }

  // 3. Dial CENTER PUSH (GPIO 5) -> Solid Play/Select box
  EPD_DrawRectangle(765, 122, 789, 150, BLACK, 1);
  if (isClockMode) {
    // Vector Play triangle (white on black)
    for (int y = 0; y < 16; y++) {
      int len = (y < 8) ? y * 2 : (15 - y) * 2;
      for (int x = 772; x <= 772 + len; x++) {
        Paint_SetPixel(x, 128 + y, WHITE);
      }
    }
  } else {
    // Solid white circle
    EPD_DrawCircle(777, 136, 5, WHITE, 1);
  }

  // 4. Dial DOWN (GPIO 4) -> Solid Down Arrow
  for (int y = 0; y <= 16; y++) {
    int w = y;
    int curY = 204 - y;
    for (int x = 777 - w; x <= 777 + w; x++) {
      if (curY >= 188 && curY <= 204 && x >= 767 && x <= 787) Paint_SetPixel(x, curY, BLACK);
    }
  }

  // 5. Bottom Button (GPIO 1, K4) -> Wipe/Back icon
  EPD_DrawRectangle(766, 226, 788, 252, BLACK, 0);
  EPD_DrawCircle(777, 239, 6, BLACK, 0);
  EPD_DrawLine(782, 233, 784, 238, BLACK);
  EPD_DrawLine(780, 236, 784, 238, BLACK);
}

void ui_engine_init() {
  pinMode(PIN_EPD_PWR, OUTPUT);
  digitalWrite(PIN_EPD_PWR, HIGH);
  pinMode(PIN_TF_PWR, OUTPUT);
  digitalWrite(PIN_TF_PWR, HIGH);
  pinMode(PIN_POWER_LED, OUTPUT);
  digitalWrite(PIN_POWER_LED, HIGH);

  EPD_GPIOInit();
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);

  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();
  EPD_Clear_R26A6H();
}

void ui_set_mode(AppMode mode) {
  currentMode = mode;
}

AppMode ui_get_mode() {
  return currentMode;
}

void ui_force_full_wipe() {
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();
  EPD_Clear_R26A6H();
}

void ui_render_startup(const char* stepMsg, const char* subMsg) {
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);

  drawString(40, 30, header_glyphs, "CROWPANEL 5.79\" SMART DESK CONSOLE", 2, BLACK);
  EPD_DrawLine(40, 75, 750, 75, BLACK);
  EPD_DrawLine(40, 76, 750, 76, BLACK);

  drawString(40, 110, text_glyphs, stepMsg, 2, BLACK);
  if (subMsg && strlen(subMsg) > 0) {
    drawString(40, 155, ui_glyphs, subMsg, 1, BLACK);
  }

  EPD_DrawLine(40, 215, 750, 215, BLACK);
  drawString(40, 230, ui_glyphs, "INITIALIZING NETWORK & HARDWARE SUBSYSTEMS...", 1, BLACK);

  EPD_Display(ImageBW);
  EPD_PartUpdate();
}

extern const ClockGlyph split_time_digits[];

static void drawSplitColon(int x, int startY) {
  int cx = x + 12;
  int cy1 = startY + 38;
  int cy2 = startY + 98;
  int r = 7;
  for (int dy = -r; dy <= r; dy++) {
    for (int dx = -r; dx <= r; dx++) {
      if (dx * dx + dy * dy <= r * r) {
        int px = cx + dx;
        int py1 = cy1 + dy;
        int py2 = cy2 + dy;
        if (px >= 0 && px < 792) {
          if (py1 >= 0 && py1 < 272) Paint_SetPixel(px, py1, BLACK);
          if (py2 >= 0 && py2 < 272) Paint_SetPixel(px, py2, BLACK);
        }
      }
    }
  }
}

void ui_clear_for_transition() {
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();
  EPD_Clear_R26A6H();
}

// ==============================================================================
// MODE 1: ADAPTIVE DIGITAL CLOCK (FULLSCREEN OR SPLIT RADIO TICKER)
// ==============================================================================
void ui_update_clock(const struct tm& timeinfo, bool radioPlaying, const char* radioStation, int volume, const char* radioTrack) {
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);
  draw_pure_icon_hud(true);

  int hour12 = timeinfo.tm_hour % 12;
  if (hour12 == 0) hour12 = 12;
  char timeStr[10];
  snprintf(timeStr, sizeof(timeStr), "%2d:%02d", hour12, timeinfo.tm_min);
  const char* ampmStr = (timeinfo.tm_hour >= 12) ? "PM" : "AM";

  const char* daysOfWeek[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
  const char* dayStr = daysOfWeek[timeinfo.tm_wday % 7];

  const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  char dateStr[30];
  snprintf(dateStr, sizeof(dateStr), "%02d %s %04d", timeinfo.tm_mday, months[timeinfo.tm_mon % 12], timeinfo.tm_year + 1900);

  if (!radioPlaying) {
    // -------------------------------------------------------------
    // FULL-SCREEN CLOCK (WHEN NO RADIO PLAYING - 100% OF 272 PIXELS)
    // -------------------------------------------------------------
    int startX = 24;
    int startY = 24;
    int x = startX;

    for (int i = 0; timeStr[i] != '\0'; i++) {
      char c = timeStr[i];
      if (c == ' ') {
        x += 65;
      } else if (c == ':') {
        drawColon(x, startY);
        x += 38;
      } else {
        drawGlyph(x, startY, massive_time_digits, c, BLACK);
        x += getGlyphWidth(massive_time_digits, c) + 12;
      }
    }

    // Vertical Divider
    EPD_DrawLine(520, 20, 520, 252, BLACK);
    EPD_DrawLine(521, 20, 521, 252, BLACK);

    // Right Column
    int rx = 542;
    drawString(rx, 24, ampm_glyphs, ampmStr, 4, BLACK);
    drawString(rx, 114, text_glyphs, dayStr, 3, BLACK);
    drawString(rx, 186, text_glyphs, dateStr, 3, BLACK);

    // WiFi status dot
    EPD_DrawCircle(rx + 4, 242, 3, BLACK, 1);
    char wifiBuf[32];
    snprintf(wifiBuf, sizeof(wifiBuf), "WIFI: %s", WiFi.SSID().c_str());
    drawString(rx + 15, 233, ui_glyphs, wifiBuf, 1, BLACK);

  } else {
    // -------------------------------------------------------------
    // SPLIT SCREEN (CLOCK UPPER + LIVE RADIO TICKER LOWER)
    // -------------------------------------------------------------
    int startX = 24;
    int startY = 18;
    int x = startX;

    for (int i = 0; timeStr[i] != '\0'; i++) {
      char c = timeStr[i];
      if (c == ' ') {
        x += 45;
      } else if (c == ':') {
        drawSplitColon(x, startY);
        x += 28;
      } else {
        drawGlyph(x, startY, split_time_digits, c, BLACK);
        x += getGlyphWidth(split_time_digits, c) + 10;
      }
    }

    // Vertical Divider
    EPD_DrawLine(440, 14, 440, 175, BLACK);

    // Right Column
    int rx = 458;
    drawString(rx, 16, ampm_glyphs, ampmStr, 3, BLACK);
    drawString(rx, 72, text_glyphs, dayStr, 2, BLACK);
    drawString(rx, 112, text_glyphs, dateStr, 2, BLACK);
    drawString(rx, 150, ui_glyphs, "WIFI: CONNECTED", 1, BLACK);

    // Horizontal Divider for Radio Bar
    EPD_DrawLine(16, 182, 754, 182, BLACK);
    EPD_DrawLine(16, 183, 754, 183, BLACK);

    // Inverted LIVE pill
    EPD_DrawRectangle(16, 194, 88, 244, BLACK, 1);
    drawString(30, 210, header_glyphs, "LIVE", 1, WHITE);

    // Station Name & Track
    drawString(102, 194, header_glyphs, radioStation, 2, BLACK);
    char trackBuf[64];
    snprintf(trackBuf, sizeof(trackBuf), "%.50s", radioTrack);
    drawString(102, 226, ui_glyphs, trackBuf, 1, BLACK);

    // Volume & Audio Box on Right
    EPD_DrawRectangle(580, 192, 752, 246, BLACK, 0);
    char volBuf[32];
    snprintf(volBuf, sizeof(volBuf), "VOL %d%%", volume);
    drawString(590, 200, ui_glyphs, volBuf, 1, BLACK);
    drawString(590, 222, ui_glyphs, "STEREO [OK]", 1, BLACK);
  }

  EPD_Display(ImageBW);
  EPD_PartUpdate();
  EPD_UpdatePrev(ImageBW);
}

// ==============================================================================
// MODE 2: DEDICATED FULL-SCREEN INTERNET RADIO PLAYER
// ==============================================================================
void ui_update_radio(const char* station, const char* track, int volume, int stationIdx, int totalStations) {
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);
  draw_pure_icon_hud(false);

  // Header
  drawString(20, 12, ui_glyphs, "INTERNET RADIO STREAMER", 1, BLACK);
  char presetBuf[32];
  snprintf(presetBuf, sizeof(presetBuf), "PRESET %02d / %02d", stationIdx + 1, totalStations);
  drawString(560, 12, ui_glyphs, presetBuf, 1, BLACK);
  EPD_DrawLine(16, 36, 754, 36, BLACK);
  EPD_DrawLine(16, 37, 754, 37, BLACK);

  // Callsign Badge
  EPD_DrawRectangle(20, 48, 105, 128, BLACK, 1);
  drawString(36, 62, ui_glyphs, "RADIO", 1, WHITE);
  drawString(28, 92, ui_glyphs, "STREAM", 1, WHITE);

  // Station & Track Info
  drawString(122, 48, header_glyphs, station, 2, BLACK);
  char trackBuf[80];
  snprintf(trackBuf, sizeof(trackBuf), "Now: %.55s", track);
  drawString(122, 92, text_glyphs, trackBuf, 1, BLACK);
  drawString(122, 134, ui_glyphs, "Format: MP3/AAC Stereo  •  Core 0 I2S Audio Active", 1, BLACK);

  // Divider
  EPD_DrawLine(16, 172, 754, 172, BLACK);

  // Graphical Volume Bar
  char volLbl[32];
  snprintf(volLbl, sizeof(volLbl), "OUTPUT VOLUME: %d%%", volume);
  drawString(20, 186, ui_glyphs, volLbl, 1, BLACK);
  EPD_DrawRectangle(20, 212, 320, 246, BLACK, 0);
  int fillW = (300 * volume) / 100;
  if (fillW > 0) {
    EPD_DrawRectangle(22, 214, 22 + fillW, 244, BLACK, 1);
  }

  // Telemetry Box
  EPD_DrawRectangle(340, 184, 752, 254, BLACK, 0);
  drawString(352, 194, ui_glyphs, "CORE 0: FreeRTOS Audio Streamer Task [ACTIVE]", 1, BLACK);
  drawString(352, 214, ui_glyphs, "DAC: MAX98357A I2S (GPIO 17, 18, 19)", 1, BLACK);
  char ipBuf[48];
  snprintf(ipBuf, sizeof(ipBuf), "IP: %s (Web App Active)", WiFi.localIP().toString().c_str());
  drawString(352, 234, ui_glyphs, ipBuf, 1, BLACK);

  EPD_Display(ImageBW);
  EPD_PartUpdate();
  EPD_UpdatePrev(ImageBW);
}

// ==============================================================================
// MODE 3: TABLETOP EXECUTIVE DESK NAMEPLATE
// ==============================================================================
void ui_update_nameboard(const NameboardData& data) {
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);
  draw_pure_icon_hud(false);

  // Double decorative plaque border
  EPD_DrawRectangle(14, 10, 754, 262, BLACK, 0);
  EPD_DrawRectangle(18, 14, 750, 258, BLACK, 0);

  // Executive Name & Designation
  drawString(36, 22, header_glyphs, data.name, 3, BLACK);
  drawString(36, 78, text_glyphs, data.title, 2, BLACK);
  drawString(36, 118, ui_glyphs, data.org, 1, BLACK);

  // Divider
  EPD_DrawLine(36, 156, 590, 156, BLACK);
  EPD_DrawLine(36, 157, 590, 157, BLACK);

  // Status Pill (Inverted Black Banner)
  EPD_DrawRectangle(36, 175, 590, 242, BLACK, 1);
  EPD_DrawCircle(56, 208, 6, WHITE, 1); // White bullet
  char statBuf[80];
  snprintf(statBuf, sizeof(statBuf), "STATUS: %.45s", data.status);
  drawString(72, 198, header_glyphs, statBuf, 1, WHITE);

  // Right Side: QR Code Block (X: 625..720)
  int qx = 625;
  int qy = 22;
  EPD_DrawRectangle(qx - 2, qy - 2, qx + 94, qy + 94, BLACK, 0);
  for (int r = 0; r < 9; r++) {
    for (int c = 0; c < 9; c++) {
      if ((r < 3 && c < 3) || (r < 3 && c > 5) || (r > 5 && c < 3)) {
        EPD_DrawRectangle(qx + c * 10 + 1, qy + r * 10 + 1, qx + (c + 1) * 10 - 2, qy + (r + 1) * 10 - 2, BLACK, 1);
      } else if ((r * 7 + c * 3) % 4 == 0) {
        EPD_DrawRectangle(qx + c * 10 + 1, qy + r * 10 + 1, qx + (c + 1) * 10 - 2, qy + (r + 1) * 10 - 2, BLACK, 1);
      }
    }
  }
  drawString(qx + 10, qy + 100, ui_glyphs, "SCAN VCARD", 1, BLACK);

  // Mini Desktop Clock in Corner
  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  int h12 = ti.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  char miniClock[16];
  snprintf(miniClock, sizeof(miniClock), "%d:%02d %s", h12, ti.tm_min, (ti.tm_hour >= 12) ? "PM" : "AM");
  drawString(qx, 185, text_glyphs, miniClock, 1, BLACK);

  EPD_Display(ImageBW);
  EPD_PartUpdate();
  EPD_UpdatePrev(ImageBW);
}

// ==============================================================================
// MODE 4: INSTANT POPUP NAVIGATION OVERLAY MENU
// ==============================================================================
void ui_update_menu(int selectedIndex) {
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);
  draw_pure_icon_hud(false);

  // Outer Box
  EPD_DrawRectangle(40, 10, 752, 262, BLACK, 0);

  // Header Banner
  EPD_DrawRectangle(42, 12, 750, 48, BLACK, 1);
  drawString(60, 20, header_glyphs, "SYSTEM NAVIGATION MENU", 2, WHITE);
  drawString(540, 24, ui_glyphs, "[DIAL: SCROLL | CLICK: OK]", 1, WHITE);

  const char* menuItems[] = {
    "1.  DIGITAL CLOCK & RADIO TICKER",
    "2.  INTERNET RADIO PLAYER (FULL SCREEN)",
    "3.  TABLETOP EXECUTIVE NAMEPLATE",
    "4.  NEXT STATUS PRESET (MEETING / DND)",
    "5.  DEVICE INFO & WEB APP LINK"
  };

  int yPos = 60;
  for (int i = 0; i < 5; i++) {
    if (i == selectedIndex) {
      EPD_DrawRectangle(50, yPos - 2, 740, yPos + 34, BLACK, 1);
      drawString(76, yPos + 6, header_glyphs, menuItems[i], 2, WHITE);
    } else {
      drawString(76, yPos + 6, header_glyphs, menuItems[i], 2, BLACK);
    }
    yPos += 38;
  }

  EPD_Display(ImageBW);
  EPD_PartUpdate();
  EPD_UpdatePrev(ImageBW);
}
