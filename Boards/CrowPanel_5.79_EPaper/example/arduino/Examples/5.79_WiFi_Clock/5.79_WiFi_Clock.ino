#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <SPI.h>
#include "time.h"
#include "EPD_Init.h"
#include "EPD.h"
#include "MassiveFont.h"

uint8_t ImageBW[27200];
WiFiMulti wifiMulti;

const long  gmtOffset_sec = 19800; // India Standard Time (UTC +5:30)
const int   daylightOffset_sec = 0;
int currentMinute = -1;
unsigned long lastNtpSync = 0;

void drawGlyph(int x, int y, const ClockGlyph* font, char c, uint16_t color) {
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

int getGlyphWidth(const ClockGlyph* font, char c) {
  for (int i = 0; font[i].c != 0; i++) {
    if (font[i].c == c) return font[i].w;
  }
  return 0;
}

void drawString(int x, int y, const ClockGlyph* font, const char* str, int spacing, uint16_t color) {
  int curX = x;
  int spaceW = 12;
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

void drawColon(int x, int startY) {
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

void renderStartupFrame(const char* wifiStatus, const char* ntpStatus, const char* footerMsg) {
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);

  // Title Header
  drawString(60, 30, header_glyphs, "CROWPANEL 5.79\" E-PAPER CLOCK", 2, BLACK);

  // Header divider line (thickness: 2px)
  for (int x = 60; x <= 732; x++) {
    Paint_SetPixel(x, 78, BLACK);
    Paint_SetPixel(x, 79, BLACK);
  }

  // Step 1: System Boot & Hardware
  drawString(60, 100, ui_glyphs, "> SYSTEM BOOT & HARDWARE INITIALIZATION... OK", 1, BLACK);

  // Step 2: WiFi Connection Status
  if (wifiStatus && strlen(wifiStatus) > 0) {
    drawString(60, 138, ui_glyphs, wifiStatus, 1, BLACK);
  }

  // Step 3: NTP Time Sync Status
  if (ntpStatus && strlen(ntpStatus) > 0) {
    drawString(60, 176, ui_glyphs, ntpStatus, 1, BLACK);
  }

  // Footer divider line & Message
  for (int x = 60; x <= 732; x++) {
    Paint_SetPixel(x, 220, BLACK);
  }
  if (footerMsg && strlen(footerMsg) > 0) {
    drawString(60, 230, ui_glyphs, footerMsg, 1, BLACK);
  }
}

void renderClockImage(const struct tm& timeinfo) {
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);

  // Format Time strings
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

  // 1. Draw Massive Time on the left
  int startX = 38;
  int startY = 28;
  int x = startX;

  for (int i = 0; timeStr[i] != '\0'; i++) {
    char c = timeStr[i];
    if (c == ' ') {
      x += 65; // balanced spacing for single digit hour
    } else if (c == ':') {
      drawColon(x, startY);
      x += 38;
    } else {
      drawGlyph(x, startY, massive_time_digits, c, BLACK);
      x += getGlyphWidth(massive_time_digits, c) + 12;
    }
  }

  // 2. Vertical Divider Line
  int divX = 530;
  for (int y = 24; y <= 252; y++) {
    Paint_SetPixel(divX, y, BLACK);
    Paint_SetPixel(divX + 1, y, BLACK);
  }

  // 3. Right Column: AM/PM, Day of Week, Date
  int rx = 555;
  drawString(rx, 26, ampm_glyphs, ampmStr, 4, BLACK);
  drawString(rx, 114, text_glyphs, dayStr, 3, BLACK);
  drawString(rx, 186, text_glyphs, dateStr, 3, BLACK);

  // 4. Subtle Connection Status Indicator under Date
  if (WiFi.status() == WL_CONNECTED) {
    // Solid dot indicator
    int dotX = rx + 3;
    int dotY = 242;
    int r = 3;
    for (int dy = -r; dy <= r; dy++) {
      for (int dx = -r; dx <= r; dx++) {
        if (dx * dx + dy * dy <= r * r) {
          Paint_SetPixel(dotX + dx, dotY + dy, BLACK);
        }
      }
    }
    char statusBuf[40];
    snprintf(statusBuf, sizeof(statusBuf), "WIFI: %s", WiFi.SSID().c_str());
    drawString(rx + 14, 233, ui_glyphs, statusBuf, 1, BLACK);
  } else {
    // Hollow dot indicator
    int dotX = rx + 3;
    int dotY = 242;
    int r = 3;
    for (int dy = -r; dy <= r; dy++) {
      for (int dx = -r; dx <= r; dx++) {
        int d2 = dx * dx + dy * dy;
        if (d2 <= r * r && d2 >= (r - 1) * (r - 1)) {
          Paint_SetPixel(dotX + dx, dotY + dy, BLACK);
        }
      }
    }
    drawString(rx + 14, 233, ui_glyphs, "OFFLINE", 1, BLACK);
  }
}

bool syncNtpStartup(struct tm* outTime) {
  Serial.println("Configuring NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, "time.google.com", "asia.pool.ntp.org", "pool.ntp.org");
  for (int i = 0; i < 30; i++) {
    if (getLocalTime(outTime, 500)) {
      Serial.printf("NTP synced: %02d:%02d:%02d\n", outTime->tm_hour, outTime->tm_min, outTime->tm_sec);
      lastNtpSync = millis();
      return true;
    }
    delay(200);
  }
  Serial.println("NTP sync timeout");
  return false;
}

void syncNtpBackground() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 500)) {
    lastNtpSync = millis();
  }
}

void setup() {
  Serial.begin(115200);

  // Enable peripheral power GPIOs
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);
  pinMode(42, OUTPUT);
  digitalWrite(42, HIGH);
  pinMode(41, OUTPUT);
  digitalWrite(41, HIGH);
  delay(100);

  // Hardware full clear on boot to wipe any previous ghosting
  EPD_GPIOInit();
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();
  EPD_Clear_R26A6H();

  // Show Initial Startup Screen: Hardware initialized & starting WiFi
  renderStartupFrame("> CONNECTING TO WI-FI (suresh)...", "", "INITIALIZING SYSTEM & NETWORK...");
  EPD_Display(ImageBW);
  EPD_PartUpdate();
  EPD_UpdatePrev(ImageBW);

  // Connect to WiFi
  wifiMulti.addAP("suresh", "alangium");
  wifiMulti.addAP("suresh2.4gExt", "alangium");

  Serial.println("Connecting to WiFi...");
  int wifiAttempts = 0;
  while (wifiMulti.run() != WL_CONNECTED && wifiAttempts < 25) {
    delay(400);
    wifiAttempts++;
  }

  char wifiStatus[90];
  char ntpStatus[90];
  struct tm timeinfo;
  bool timeValid = false;

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected!");
    snprintf(wifiStatus, sizeof(wifiStatus), "> WI-FI CONNECTED: %s [%s]", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    
    // Update screen with WiFi connected & now syncing NTP
    renderStartupFrame(wifiStatus, "> SYNCING NTP TIME (UTC+5:30 IST)...", "SYNCHRONIZING CLOCK WITH ATOMIC TIME...");
    EPD_Display(ImageBW);
    EPD_PartUpdate();
    EPD_UpdatePrev(ImageBW);

    timeValid = syncNtpStartup(&timeinfo);
    if (timeValid) {
      int hour12 = timeinfo.tm_hour % 12;
      if (hour12 == 0) hour12 = 12;
      const char* ampm = (timeinfo.tm_hour >= 12) ? "PM" : "AM";
      snprintf(ntpStatus, sizeof(ntpStatus), "> NTP TIME SYNCED: %02d:%02d %s (IST)... OK", hour12, timeinfo.tm_min, ampm);
      renderStartupFrame(wifiStatus, ntpStatus, "READY! LAUNCHING DIGITAL CLOCK INTERFACE...");
    } else {
      snprintf(ntpStatus, sizeof(ntpStatus), "> NTP SYNC TIMEOUT (RETRYING IN BACKGROUND)");
      renderStartupFrame(wifiStatus, ntpStatus, "STARTING CLOCK...");
    }
  } else {
    Serial.println("WiFi connect failed, will retry in background");
    snprintf(wifiStatus, sizeof(wifiStatus), "> WI-FI CONNECTION FAILED (RETRYING IN BACKGROUND)");
    snprintf(ntpStatus, sizeof(ntpStatus), "> USING BACKUP TIME");
    renderStartupFrame(wifiStatus, ntpStatus, "STARTING CLOCK...");
  }

  // Final startup frame update
  EPD_Display(ImageBW);
  EPD_PartUpdate();
  EPD_UpdatePrev(ImageBW);

  // Allow user to view the completed startup checklist
  delay(2000);

  if (!timeValid) {
    if (!getLocalTime(&timeinfo, 1000)) {
      // Fallback
      timeinfo.tm_hour = 20;
      timeinfo.tm_min = 45;
      timeinfo.tm_sec = 0;
      timeinfo.tm_mday = 8;
      timeinfo.tm_mon = 9;
      timeinfo.tm_year = 126; // 2026
      timeinfo.tm_wday = 4;   // Thursday
    }
  }

  currentMinute = timeinfo.tm_min;

  // Clean transition to main clock:
  // 1. Actively erase all startup checklist pixels to white via partial update
  Paint_Clear(WHITE);
  EPD_Display(ImageBW);
  EPD_PartUpdate();
  EPD_UpdatePrev(ImageBW);
  delay(150);

  // 2. Full hardware reset & global clear to eliminate all micro-capsule ghosting
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();
  EPD_Clear_R26A6H();

  // 3. Render main clock and sync to differential memory
  renderClockImage(timeinfo);
  EPD_Display(ImageBW);
  EPD_PartUpdate();
  EPD_UpdatePrev(ImageBW);
  Serial.println("Initial clock frame displayed");
}

void loop() {
  // Check and maintain WiFi connection
  if (wifiMulti.run() == WL_CONNECTED) {
    if (millis() - lastNtpSync > 3600000 || lastNtpSync == 0) {
      syncNtpBackground();
    }
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 100)) {
    if (timeinfo.tm_min != currentMinute) {
      Serial.printf("Minute changed to %02d - updating display\n", timeinfo.tm_min);
      renderClockImage(timeinfo);
      EPD_Display(ImageBW);
      EPD_PartUpdate();
      EPD_UpdatePrev(ImageBW);
      currentMinute = timeinfo.tm_min;
    }
  }

  delay(1000);
}
