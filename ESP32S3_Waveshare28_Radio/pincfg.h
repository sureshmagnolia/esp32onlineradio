#pragma once

// ============================================================================
// Waveshare ESP32-S3 Touch LCD 2.8" Pin & Hardware Configuration
// Dedicated for 320x240 Landscape Radio Enclosure
// ============================================================================

// --- Display Resolution ---
#define LCD_WIDTH               320
#define LCD_HEIGHT              240
#define TFT_WIDTH               320
#define TFT_HEIGHT              240

// Orientation Mode:
// When 1, the physical edge with the 3 buttons (BOOT, RESET, PWR) and the
// MicroSD card socket is positioned at the TOP of the visual screen.
#define DISPLAY_ROTATION_BUTTONS_TOP   1

#if DISPLAY_ROTATION_BUTTONS_TOP
  #define ST7789_ROTATION_VAL          1
  #define TOUCH_SWAP_XY                1
  #define TOUCH_INVERT_X               0
  #define TOUCH_INVERT_Y               1
#else
  #define ST7789_ROTATION_VAL          3
  #define TOUCH_SWAP_XY                1
  #define TOUCH_INVERT_X               1
  #define TOUCH_INVERT_Y               0
#endif

// --- ST7789 SPI LCD Interface ---
#define LCD_SPI_HOST            SPI2_HOST
#define LCD_PIN_MOSI            45
#define LCD_PIN_SCLK            40
#define LCD_PIN_CS              42
#define LCD_PIN_DC              41
#define LCD_PIN_RST             39
#define LCD_PIN_BL              5   // Backlight PWM control (Active HIGH)
#define TFT_BLK                 5
#define TFT_BLK_ON_LEVEL        HIGH

// --- Capacitive Touch (CST328 / CST3530) I2C ---
#define TOUCH_I2C_HOST          0
#define TOUCH_PIN_SDA           1
#define TOUCH_PIN_SCL           3
#define TOUCH_PIN_INT           4
#define TOUCH_PIN_RST           2
#define TOUCH_PIN_NUM_INT       4
#define TOUCH_I2C_ADDR          0x1A // CST328 default address (or 0x5A)

// --- PCM5101 I2S Audio Decoder ---
#define I2S_PIN_BCLK            13  // Bit Clock
#define I2S_PIN_LRCK            12  // Word Select / L-R Clock
#define I2S_PIN_DOUT            14  // Serial Audio Data Output
#define AUDIO_I2S_BCK_IO        13
#define AUDIO_I2S_LRCK_IO       12
#define AUDIO_I2S_DO_IO         14

// --- MicroSD (TF) Card Interface ---
#define SD_PIN_MISO             42
#define SD_PIN_MOSI             1
#define SD_PIN_CLK              2
#define SD_PIN_CS               4   // Shared or IO expander EXIO4
#define SD_MMC_CLK              SD_PIN_CLK
#define SD_MMC_CMD              SD_PIN_MOSI
#define SD_MMC_D0               SD_PIN_MISO

// --- Battery ADC Measurement Pin ---
#define BAT_ADC_PIN             6   // Onboard battery monitor ADC (or GPIO 5/6)

// --- Onboard Hardware Physical Buttons ---
#define BUTTON_PIN_BOOT         0   // Active LOW, with hardware pull-up
// Note: RESET is connected directly to EN pin (hardware restart)
// Note: PWR is connected to battery power management / SYS_EN

// --- Onboard Sensors & RTC (Shared I2C Bus on GPIO 1/3) ---
#define RTC_PCF85063_ADDR       0x51
#define IMU_QMI8658_ADDR        0x6B
