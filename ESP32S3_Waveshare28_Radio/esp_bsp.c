#include "esp_bsp.h"
#include "pincfg.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "BSP_WS28";

static esp_lcd_panel_handle_t s_panel_handle = NULL;
static SemaphoreHandle_t s_lvgl_mutex = NULL;
static TaskHandle_t s_lvgl_task_handle = NULL;
static lv_disp_drv_t s_disp_drv;
static lv_indev_drv_t s_indev_drv;
static lv_disp_draw_buf_t s_draw_buf;
static lv_color_t *s_buf1 = NULL;
static lv_color_t *s_buf2 = NULL;

static int s_brightness_percent = 100;

static void lvgl_task(void *pvParameter);
static void disp_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
static void touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
static bool read_cst328_touch(uint16_t *x, uint16_t *y);

lv_disp_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg) {
    ESP_LOGI(TAG, "Starting Waveshare 2.8\" Display (Buttons & Card Reader at TOP)...");

    s_lvgl_mutex = xSemaphoreCreateMutex();
    if (!s_lvgl_mutex) {
        ESP_LOGE(TAG, "Failed to create LVGL mutex");
        return NULL;
    }

    // 1. Initialize Backlight via LEDC PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = LCD_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0, // start dark
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel);

    // 2. Initialize SPI Bus
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_SCLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * 40 * sizeof(uint16_t)
    };
    esp_err_t ret = spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI bus init failed: 0x%x", ret);
        return NULL;
    }

    // 3. Panel IO Configuration
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = 60 * 1000 * 1000, // 60 MHz
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_config, &io_handle));

    // 4. ST7789 Panel Driver
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &s_panel_handle));

    esp_lcd_panel_reset(s_panel_handle);
    esp_lcd_panel_init(s_panel_handle);
    esp_lcd_panel_invert_color(s_panel_handle, true);

    // Swap Axes and Mirror to place Buttons & Card Reader at the TOP of the display
#if DISPLAY_ROTATION_BUTTONS_TOP
    esp_lcd_panel_swap_xy(s_panel_handle, true);
    esp_lcd_panel_mirror(s_panel_handle, false, true);
    esp_lcd_panel_set_gap(s_panel_handle, 0, 0);
#else
    esp_lcd_panel_swap_xy(s_panel_handle, true);
    esp_lcd_panel_mirror(s_panel_handle, true, false);
    esp_lcd_panel_set_gap(s_panel_handle, 0, 0);
#endif

    esp_lcd_panel_disp_on_off(s_panel_handle, true);

    // 5. Initialize I2C for CST328 Touch
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TOUCH_PIN_SDA,
        .scl_io_num = TOUCH_PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = { .clk_speed = 400000 },
    };
    i2c_param_config(I2C_NUM_0, &i2c_conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    if (TOUCH_PIN_RST >= 0) {
        gpio_set_direction((gpio_num_t)TOUCH_PIN_RST, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)TOUCH_PIN_RST, 0);
        vTaskDelay(pdMS_TO_TICKS(15));
        gpio_set_level((gpio_num_t)TOUCH_PIN_RST, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // 6. Initialize LVGL
    lv_init();
    size_t buf_pixels = LCD_WIDTH * 40;
    s_buf1 = (lv_color_t *)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_buf2 = (lv_color_t *)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!s_buf1) {
        s_buf1 = (lv_color_t *)malloc(buf_pixels * sizeof(lv_color_t));
        s_buf2 = NULL;
    }
    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, buf_pixels);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = LCD_WIDTH;
    s_disp_drv.ver_res = LCD_HEIGHT;
    s_disp_drv.flush_cb = disp_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_t *disp = lv_disp_drv_register(&s_disp_drv);

    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&s_indev_drv);

    // 7. Start Dedicated LVGL Thread
    xTaskCreatePinnedToCore(
        lvgl_task,
        "lvgl_task",
        cfg->lvgl_port_cfg.task_stack ? cfg->lvgl_port_cfg.task_stack : 6144,
        NULL,
        cfg->lvgl_port_cfg.task_priority ? cfg->lvgl_port_cfg.task_priority : 2,
        &s_lvgl_task_handle,
        cfg->lvgl_port_cfg.task_affinity >= 0 ? cfg->lvgl_port_cfg.task_affinity : 1
    );

    return disp;
}

static void lvgl_task(void *pvParameter) {
    while (1) {
        if (bsp_display_lock(10)) {
            lv_timer_handler();
            bsp_display_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

bool bsp_display_lock(uint32_t timeout_ms) {
    if (!s_lvgl_mutex) return false;
    TickType_t ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return (xSemaphoreTake(s_lvgl_mutex, ticks) == pdTRUE);
}

void bsp_display_unlock(void) {
    if (s_lvgl_mutex) {
        xSemaphoreGive(s_lvgl_mutex);
    }
}

void bsp_display_brightness_set(int brightness_percent) {
    if (brightness_percent < 0) brightness_percent = 0;
    if (brightness_percent > 100) brightness_percent = 100;
    s_brightness_percent = brightness_percent;

    uint32_t duty = (uint32_t)(s_brightness_percent * 255 / 100);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void bsp_display_backlight_on(void) {
    bsp_display_brightness_set(100);
}

void bsp_display_backlight_off(void) {
    bsp_display_brightness_set(0);
}

static void disp_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    esp_lcd_panel_draw_bitmap(s_panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_p);
    lv_disp_flush_ready(disp_drv);
}

static void touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    uint16_t raw_x = 0, raw_y = 0;
    bool touched = read_cst328_touch(&raw_x, &raw_y);

    if (touched) {
        data->state = LV_INDEV_STATE_PR;

#if DISPLAY_ROTATION_BUTTONS_TOP
        int16_t screen_x = raw_y;
        int16_t screen_y = 239 - raw_x;
#else
        int16_t screen_x = 319 - raw_y;
        int16_t screen_y = raw_x;
#endif

        if (screen_x < 0) screen_x = 0;
        if (screen_x >= LCD_WIDTH) screen_x = LCD_WIDTH - 1;
        if (screen_y < 0) screen_y = 0;
        if (screen_y >= LCD_HEIGHT) screen_y = LCD_HEIGHT - 1;

        data->point.x = screen_x;
        data->point.y = screen_y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

static bool read_cst328_touch(uint16_t *x, uint16_t *y) {
    uint8_t write_buf[2] = {0xD0, 0x00};
    uint8_t read_buf[7] = {0};

    esp_err_t ret = i2c_master_write_read_device(I2C_NUM_0, TOUCH_I2C_ADDR, write_buf, 2, read_buf, 7, pdMS_TO_TICKS(20));
    if (ret != ESP_OK) {
        return false;
    }

    uint8_t fingerNum = read_buf[0] & 0x0F;
    if (fingerNum == 0) {
        return false;
    }

    uint16_t touchX = ((read_buf[1] & 0x0F) << 8) | read_buf[2];
    uint16_t touchY = ((read_buf[3] & 0x0F) << 8) | read_buf[4];

    *x = touchX;
    *y = touchY;
    return true;
}
