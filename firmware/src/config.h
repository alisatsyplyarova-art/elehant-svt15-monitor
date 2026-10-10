#pragma once

// Select exactly one board; default is ESP32-C3.
// #define BOARD_TTGO_TDISPLAY
// #define BOARD_TTGO_TDISPLAY_S3
// #define BOARD_ESP32_GENERIC
#if !defined(BOARD_ESP32_C3) && !defined(BOARD_TTGO_TDISPLAY) && !defined(BOARD_TTGO_TDISPLAY_S3) && !defined(BOARD_ESP32_GENERIC)
  #define BOARD_ESP32_C3
#endif

// Select one display. TTGO profiles default to their onboard ST7789.
// #define DISPLAY_LCD5110
// #define DISPLAY_SSD1306
// #define DISPLAY_SH1106
// #define DISPLAY_SSD1309
// #define DISPLAY_ST7735
// #define DISPLAY_ST7789
// #define DISPLAY_NONE
#if !defined(DISPLAY_LCD5110) && !defined(DISPLAY_SSD1306) && !defined(DISPLAY_SH1106) && !defined(DISPLAY_SSD1309) && !defined(DISPLAY_ST7735) && !defined(DISPLAY_ST7789) && !defined(DISPLAY_NONE)
  #if defined(BOARD_TTGO_TDISPLAY) || defined(BOARD_TTGO_TDISPLAY_S3)
    #define DISPLAY_ST7789
  #else
    #define DISPLAY_LCD5110
  #endif
#endif

#define MAX_METERS 40
#define HISTORY_RETENTION_DAYS 65
#define AP_DEFAULT_SSID "SVT15-Monitor"
#define AP_DEFAULT_PASSWORD "88118811"
#define AP_DEFAULT_IP "192.168.4.1"
#define AP_RECOVERY_HOLD_MS 5000UL
#define BUTTON_DEBOUNCE_MS 35UL
#define BUTTON_LONG_PRESS_MS 3000UL
#define BACKLIGHT_TIMEOUT_MS 300000UL
