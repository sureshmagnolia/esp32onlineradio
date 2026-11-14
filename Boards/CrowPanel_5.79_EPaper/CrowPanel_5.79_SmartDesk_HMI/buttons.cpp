#include "buttons.h"

struct ButtonState {
  uint8_t pin;
  bool lastState;
  unsigned long lastDebounceTime;
};

static ButtonState btnMenu = {PIN_MENU_KEY, HIGH, 0};
static ButtonState btnExit = {PIN_EXIT_KEY, HIGH, 0};
static ButtonState btnUp   = {PIN_DIAL_UP,  HIGH, 0};
static ButtonState btnOk   = {PIN_DIAL_OK,  HIGH, 0};
static ButtonState btnDn   = {PIN_DIAL_DN,  HIGH, 0};

void buttons_init() {
  pinMode(PIN_MENU_KEY, INPUT_PULLUP);
  pinMode(PIN_EXIT_KEY, INPUT_PULLUP);
  pinMode(PIN_DIAL_UP,  INPUT_PULLUP);
  pinMode(PIN_DIAL_OK,  INPUT_PULLUP);
  pinMode(PIN_DIAL_DN,  INPUT_PULLUP);
}

static bool check_button(ButtonState& b) {
  bool current = digitalRead(b.pin);
  if (current != b.lastState) {
    b.lastDebounceTime = millis();
    b.lastState = current;
  }
  if ((millis() - b.lastDebounceTime) > 35) {
    if (current == LOW) {
      // Wait for release or debounce
      while(digitalRead(b.pin) == LOW) {
        delay(5);
      }
      return true;
    }
  }
  return false;
}

ButtonEvent buttons_poll() {
  if (check_button(btnMenu)) return BTN_MENU;
  if (check_button(btnExit)) return BTN_BACK;
  if (check_button(btnUp))   return BTN_UP;
  if (check_button(btnOk))   return BTN_OK;
  if (check_button(btnDn))   return BTN_DOWN;
  return BTN_NONE;
}
