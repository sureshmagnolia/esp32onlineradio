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
  while (*str) {
    char c = *str++;
    if (c == ' ') {
      curX += 16;
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
  for (int y = 24; y <= 248; y++) {
    Paint_SetPixel(divX, y, BLACK);
    Paint_SetPixel(divX + 1, y, BLACK);
  }

  // 3. Right Column: AM/PM, Day of Week, Date
  int rx = 555;
  drawString(rx, 26, ampm_glyphs, ampmStr, 4, BLACK);
  drawString(rx, 114, text_glyphs, dayStr, 3, BLACK);
  drawString(rx, 186, text_glyphs, dateStr, 3, BLACK);
}

void syncNtp() {
  Serial.println("Configuring NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, "time.google.com", "asia.pool.ntp.org", "pool.ntp.org");
  struct tm timeinfo;
  for (int i = 0; i < 30; i++) {
    if (getLocalTime(&timeinfo, 500)) {
      Serial.printf("NTP synced: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      lastNtpSync = millis();
      return;
    }
    delay(200);
  }
  Serial.println("NTP sync timeout");
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

  // Connect to WiFi
  wifiMulti.addAP("suresh", "alangium");
  wifiMulti.addAP("suresh2.4gExt", "alangium");

  Serial.println("Connecting to WiFi...");
  int wifiAttempts = 0;
  while (wifiMulti.run() != WL_CONNECTED && wifiAttempts < 20) {
    delay(500);
    wifiAttempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected!");
    syncNtp();
  } else {
    Serial.println("WiFi connect failed, will retry in background");
  }

  // Get current time or fallback
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 1000)) {
    // If NTP hasn't responded yet, fallback to sensible placeholder
    timeinfo.tm_hour = 19;
    timeinfo.tm_min = 55;
    timeinfo.tm_sec = 0;
    timeinfo.tm_mday = 8;
    timeinfo.tm_mon = 9;
    timeinfo.tm_year = 126; // 2026
    timeinfo.tm_wday = 4;   // Thursday
  }

  currentMinute = timeinfo.tm_min;

  // Render and perform clean partial update against cleared 0x26 white buffer
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
      syncNtp();
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
