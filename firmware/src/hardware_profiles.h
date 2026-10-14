#pragma once
#include <Arduino.h>
#include "config.h"

struct HardwareProfile {
  const char* name;
  int buttonUp, buttonDown;
  bool upPullup, downPullup;
  int displaySclk, displayMosi, displayCs, displayDc, displayRst, displayBacklight, displayPower;
  bool hasBuiltInDisplay;
};

// Verify all pins against the exact PCB revision before wiring.
// Unlisted GPIOs are NOT automatically considered free.
#if defined(BOARD_TTGO_TDISPLAY)
static const HardwareProfile HW = {
  "LILYGO/TTGO T-Display classic ESP32",
  0, 35, true, false, 18, 19, 5, 16, 23, 4, -1, true
};
#elif defined(BOARD_TTGO_TDISPLAY_S3)
static const HardwareProfile HW = {
  "LILYGO T-Display S3 (verify PCB revision)",
  0, 14, true, true, 18, 17, 6, 7, 5, 38, 15, true
};
#else
static const HardwareProfile HW = {
  "ESP32-C3 + ST7735 128x160 (user-verified wiring; no separate backlight)",
  1, 0, true, true, 2, 3, 7, 6, 10, -1, -1, false
};
#endif

// ESP32-C3 user-verified wiring: SCLK=GPIO2, MOSI=GPIO3, DC=GPIO6, CS=GPIO7, RST=GPIO10.
// Buttons remain GPIO1/0.
// This 7-pin display has no separate BL pin; backlight is assumed to be wired on the module.
// GPIO2/8/9 are strapping pins; GPIO18/19 are USB on ESP32-C3. GPIO11-17 are reserved for module flash.
// Classic T-Display: GPIO0=BTN1/BOOT; GPIO35=BTN2 (input-only, no internal pull-up);
// display GPIO18=SCLK, 19=MOSI, 5=CS, 16=DC, 23=RST, 4=BL.
// T-Display S3 typical pins: SCLK=18, MOSI=17, CS=6, DC=7, RST=5,
// BL=38, power-enable=15. Confirm the exact PCB revision.
