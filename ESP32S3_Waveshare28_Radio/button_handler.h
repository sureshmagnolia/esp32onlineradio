#pragma once
#include <Arduino.h>

enum class ButtonEvent {
    NONE,
    SINGLE_CLICK,
    DOUBLE_CLICK,
    LONG_PRESS,
    HOLD_REPEAT
};

typedef void (*ButtonCallback)(ButtonEvent event);

class ButtonHandler {
public:
    ButtonHandler(uint8_t pin, bool activeLow = true);
    void begin(ButtonCallback callback = nullptr);
    void update();
    void setCallback(ButtonCallback callback);

private:
    uint8_t _pin;
    bool _activeLow;
    ButtonCallback _callback;

    bool _lastState;
    uint32_t _lastChangeTime;
    uint32_t _pressStartTime;
    uint32_t _lastRepeatTime;
    bool _isPressed;
    bool _longPressTriggered;
    uint8_t _clickCount;
    uint32_t _lastClickReleaseTime;

    static constexpr uint32_t DEBOUNCE_MS = 35;
    static constexpr uint32_t LONG_PRESS_MS = 600;
    static constexpr uint32_t REPEAT_INTERVAL_MS = 220;
    static constexpr uint32_t DOUBLE_CLICK_GAP_MS = 350;
};
