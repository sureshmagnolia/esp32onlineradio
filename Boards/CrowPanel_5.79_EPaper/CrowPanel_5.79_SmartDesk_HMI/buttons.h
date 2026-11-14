#pragma once
#include <Arduino.h>
#include "config.h"

enum ButtonEvent {
  BTN_NONE = 0,
  BTN_MENU,       // K3 (GPIO 2): Upper tactile button
  BTN_BACK,       // K4 (GPIO 1): Lower tactile button
  BTN_UP,         // K5 (GPIO 6): Dial roller UP
  BTN_OK,         // K5 (GPIO 5): Dial center PUSH
  BTN_DOWN        // K5 (GPIO 4): Dial roller DOWN
};

void buttons_init();
ButtonEvent buttons_poll();
