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
#elif defined(BOARD_ESP32_GENERIC)
static const HardwareProfile HW = {
  "Generic ESP32 — verify pins for exact board",
  32, 33, true, true, 18, 23, 5, 16, 17, 4, -1, false
};
#else
static const HardwareProfile HW = {
  "ESP32-C3 + LCD5110 example",
  1, 5, true, true, 4, 6, 7, 2, 3, 10, -1, false
};
#endif

// ESP32-C3 example: GPIO0 is BOOT/strap; GPIO1=UP; GPIO2=LCD DC;
// GPIO3=LCD RST; GPIO4=LCD CLK; GPIO5=DOWN; GPIO6=LCD DIN;
// GPIO7=LCD CS; GPIO10=backlight. Check the exact C3 module.
// Classic T-Display: GPIO0=BTN1/BOOT; GPIO35=BTN2 (input-only, no internal pull-up);
// display GPIO18=SCLK, 19=MOSI, 5=CS, 16=DC, 23=RST, 4=BL.
// T-Display S3 typical pins: SCLK=18, MOSI=17, CS=6, DC=7, RST=5,
// BL=38, power-enable=15. Confirm the exact PCB revision.
