#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "lvgl.h"
#include "pincfg.h"

#define EXAMPLE_LCD_QSPI_H_RES  320
#define EXAMPLE_LCD_QSPI_V_RES  240

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int task_priority;
    int task_stack;
    int task_affinity;
} lvgl_port_cfg_t;

#define ESP_LVGL_PORT_INIT_CONFIG() { \
    .task_priority = 2, \
    .task_stack = 6144, \
    .task_affinity = 1, \
}

typedef struct {
    lvgl_port_cfg_t lvgl_port_cfg;
    uint32_t buffer_size;
    lv_disp_rot_t rotate;
} bsp_display_cfg_t;

lv_disp_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);
bool bsp_display_lock(uint32_t timeout_ms);
void bsp_display_unlock(void);
void bsp_display_brightness_set(int brightness_percent);
void bsp_display_backlight_on(void);
void bsp_display_backlight_off(void);

#ifdef __cplusplus
}
#endif
