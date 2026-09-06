#include "button_handler.h"

ButtonHandler::ButtonHandler(uint8_t pin, bool activeLow)
    : _pin(pin),
      _activeLow(activeLow),
      _callback(nullptr),
      _lastState(!activeLow),
      _lastChangeTime(0),
      _pressStartTime(0),
      _lastRepeatTime(0),
      _isPressed(false),
      _longPressTriggered(false),
      _clickCount(0),
      _lastClickReleaseTime(0) {}

void ButtonHandler::begin(ButtonCallback callback) {
    _callback = callback;
    pinMode(_pin, _activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
    _lastState = digitalRead(_pin);
}

void ButtonHandler::setCallback(ButtonCallback callback) {
    _callback = callback;
}

void ButtonHandler::update() {
    uint32_t now = millis();
    bool rawReading = digitalRead(_pin);
    bool currentLogicalState = (_activeLow) ? (rawReading == LOW) : (rawReading == HIGH);

    // State transition with debounce
    if (currentLogicalState != _isPressed) {
        if ((now - _lastChangeTime) >= DEBOUNCE_MS) {
            _lastChangeTime = now;
            _isPressed = currentLogicalState;

            if (_isPressed) {
                // Button just pressed down
                _pressStartTime = now;
                _lastRepeatTime = now;
                _longPressTriggered = false;
            } else {
                // Button just released
                if (!_longPressTriggered) {
                    _clickCount++;
                    _lastClickReleaseTime = now;
                }
            }
        }
    }

    // Check for Long Press / Hold with continuous repeat while held
    if (_isPressed) {
        if (!_longPressTriggered) {
            if ((now - _pressStartTime) >= LONG_PRESS_MS) {
                _longPressTriggered = true;
                _clickCount = 0; // Cancel single/double click
                _lastRepeatTime = now;
                if (_callback) {
                    _callback(ButtonEvent::LONG_PRESS);
                    _callback(ButtonEvent::HOLD_REPEAT);
                }
            }
        } else {
            // Repeat event every interval while still held down
            if ((now - _lastRepeatTime) >= REPEAT_INTERVAL_MS) {
                _lastRepeatTime = now;
                if (_callback) {
                    _callback(ButtonEvent::HOLD_REPEAT);
                }
            }
        }
    }

    // Check for Single vs Double click timeout after release
    if (!_isPressed && _clickCount > 0) {
        if (_clickCount == 1) {
            if ((now - _lastClickReleaseTime) >= DOUBLE_CLICK_GAP_MS) {
                _clickCount = 0;
                if (_callback) {
                    _callback(ButtonEvent::SINGLE_CLICK);
                }
            }
        } else if (_clickCount >= 2) {
            _clickCount = 0;
            if (_callback) {
                _callback(ButtonEvent::DOUBLE_CLICK);
            }
        }
    }
}
