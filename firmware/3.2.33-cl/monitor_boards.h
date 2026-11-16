// Платы: распиновка и параметры памяти. Плата выбирается BOARD_TYPE в начале файла .ino.
// Новая плата = новый блок #elif ниже (кнопки, пины дисплеев, параметры памяти).
#pragma once
#include <Arduino.h>

#ifndef BOARD_TYPE
  #error "BOARD_TYPE не задан: выбор платы в начале файла .ino"
#endif

// ======================= 1: ESP32-C3 (экономия ОЗУ) =======================
// Arduino IDE: Tools -> Board -> ESP32C3 Dev Module
#if BOARD_TYPE == BOARD_ESP32C3
  #ifndef CONFIG_IDF_TARGET_ESP32C3
    #error "Выбрана плата ESP32-C3 (BOARD_TYPE 1), а в Arduino IDE другая: Tools -> Board -> ESP32C3 Dev Module"
  #endif
  #define BOARD_NAME "ESP32-C3"
  #define SVT_SERIAL Serial0          // UART0

  // --- память: режим экономии ОЗУ ---
  #define SVT_LOW_RAM        1
  #define SVT_MAX_METERS     40
  #define SVT_EVQ_SIZE       4        // очередь пакетов для печати
  #define SVT_SCAN_STACK     6144     // стек задачи BLE-сканирования
  #define SVT_AP_MAX_CLIENTS 3        // клиентов точки доступа
  #define SVT_HISTORY_DAYS   30       // срок истории по умолчанию (этап с историей)

  // --- кнопки (INPUT_PULLUP, второй вывод на GND) ---
  #define BTN_UP_PIN    1
  #define BTN_DOWN_PIN  5

  // --- LCD5110: модуль вставлен в контакты платы 17-24 ---
  // 17 GND | 18 BL (на 3.3V) | 19 VCC | 20 CLK | 21 DIN | 22 DC | 23 CE | 24 RST
  // Контакт 19 = GPIO2, поэтому VCC дисплея питается с GPIO2 (HIGH, ток дисплея < 1 мА).
  #define LCD_VCC 2
  #define LCD_CLK 3
  #define LCD_DIN 10
  #define LCD_DC  6
  #define LCD_CS  7                   // вывод CE дисплея
  #define LCD_RST 11                  // если не работает (GPIO11 связан с флеш-памятью): -1
  #define LCD_BL  -1                  // подсветка: BL -> 3.3V

  // --- TFT (ST7735 / ST7789): модуль вставлен в контакты платы 17-23 ---
  // 17 GND | 18 VCC (3.3V) | 19 SCK | 20 SDA | 21 RES | 22 RS(DC) | 23 CS
  #define TFT_SCK   2
  #define TFT_MOSI  3
  #define TFT_RST   10
  #define TFT_DC    6
  #define TFT_CS    7

// ======================= 2: ESP32 (классический, DevKit) =======================
// Arduino IDE: Tools -> Board -> ESP32 Dev Module
// Пины выбраны вне загрузочных (0, 2, 5, 12, 15) и флеш-пинов (6-11); проверьте со своей платой.
#elif BOARD_TYPE == BOARD_ESP32
  #ifndef CONFIG_IDF_TARGET_ESP32
    #error "Выбрана плата ESP32 (BOARD_TYPE 2), а в Arduino IDE другая: Tools -> Board -> ESP32 Dev Module"
  #endif
  #define BOARD_NAME "ESP32"
  #define SVT_SERIAL Serial           // UART0 (USB-UART платы)

  // --- память: 520 КБ ОЗУ, двухъядерный ---
  #define SVT_LOW_RAM        0
  #define SVT_MAX_METERS     40
  #define SVT_EVQ_SIZE       8
  #define SVT_SCAN_STACK     8192
  #define SVT_AP_MAX_CLIENTS 4
  #define SVT_HISTORY_DAYS   65

  // --- кнопки (INPUT_PULLUP, второй вывод на GND) ---
  #define BTN_UP_PIN    32
  #define BTN_DOWN_PIN  33

  // --- LCD5110: VCC и BL подключить к 3.3V платы ---
  #define LCD_VCC -1                  // питание не с пина
  #define LCD_CLK 18
  #define LCD_DIN 23
  #define LCD_DC  25
  #define LCD_CS  26                  // вывод CE дисплея
  #define LCD_RST 27
  #define LCD_BL  -1                  // подсветка: BL -> 3.3V

  // --- TFT (ST7735 / ST7789): VCC на 3.3V, аппаратный SPI (VSPI) ---
  #define TFT_SCK   18
  #define TFT_MOSI  23
  #define TFT_RST   27
  #define TFT_DC    25
  #define TFT_CS    26

// ======================= 3: ESP32-T (T-Display, встроенный ЖК 1.14") =======================
// Плата T-Display на классическом ESP32 (USB-UART CH9102F), встроенный ST7789 135x240.
// Arduino IDE: Tools -> Board -> ESP32 Dev Module (или TTGO T1 / LilyGo T-Display).
// Дисплей встроенный: DISPLAY_TYPE принудительно 3, внешние дисплеи не используются.
#elif BOARD_TYPE == BOARD_ESP32T
  #ifndef CONFIG_IDF_TARGET_ESP32
    #error "Выбрана плата ESP32-T (BOARD_TYPE 3, классический ESP32), а в Arduino IDE другая: Tools -> Board -> ESP32 Dev Module"
  #endif
  #define BOARD_NAME "ESP32-T (T-Display)"
  #define SVT_SERIAL Serial
  #define SVT_BUILTIN_TFT    1

  #define SVT_LOW_RAM        0
  #define SVT_MAX_METERS     40
  #define SVT_EVQ_SIZE       8
  #define SVT_SCAN_STACK     8192
  #define SVT_AP_MAX_CLIENTS 4
  #define SVT_HISTORY_DAYS   65

  // --- две кнопки платы ---
  // GPIO35 (правая) без внутренней подтяжки: на плате есть внешняя
  // GPIO0 (левая, BOOT): при включении с нажатой кнопкой плата уходит в загрузчик
  #define BTN_UP_PIN    35
  #define BTN_UP_MODE   INPUT
  #define BTN_DOWN_PIN  0
  #define BTN_DOWN_MODE INPUT_PULLUP

  // --- встроенный ST7789 135x240 ---
  #define TFT_SCK   18
  #define TFT_MOSI  19
  #define TFT_CS    5
  #define TFT_DC    16
  #define TFT_RST   23
  #define TFT_BL    4                 // подсветка, HIGH = включена
  #define ST7789_W  135
  #define ST7789_H  240
  #define TFT_ROTATION 1              // альбомная 240x135

// ======================= 4: ESP32-S (ESP32-S3 Dual Type-C) =======================
// Плата ESP32-S3 с двумя разъёмами Type-C (UART и родной USB).
// Arduino IDE: Tools -> Board -> ESP32S3 Dev Module.
// Serial0 = порт UART (разъём с USB-UART). Для родного USB включить Tools -> USB CDC On Boot
// и заменить SVT_SERIAL на Serial.
// Не используются: 0, 3, 45, 46 (загрузочные), 19, 20 (USB), 26-37 (флеш и PSRAM), 43, 44 (UART0).
#elif BOARD_TYPE == BOARD_ESP32S
  #ifndef CONFIG_IDF_TARGET_ESP32S3
    #error "Выбрана плата ESP32-S (BOARD_TYPE 4, ESP32-S3), а в Arduino IDE другая: Tools -> Board -> ESP32S3 Dev Module"
  #endif
  #define BOARD_NAME "ESP32-S3"
  #define SVT_SERIAL Serial0

  #define SVT_LOW_RAM        0
  #define SVT_MAX_METERS     40
  #define SVT_EVQ_SIZE       8
  #define SVT_SCAN_STACK     8192
  #define SVT_AP_MAX_CLIENTS 4
  #define SVT_HISTORY_DAYS   65

  // --- кнопки (INPUT_PULLUP, второй вывод на GND) ---
  #define BTN_UP_PIN    4
  #define BTN_DOWN_PIN  5

  // --- LCD5110: VCC и BL подключить к 3.3V платы (аппаратный SPI FSPI: SCK 12, MOSI 11) ---
  #define LCD_VCC -1
  #define LCD_CLK 12
  #define LCD_DIN 11
  #define LCD_DC  9
  #define LCD_CS  10                  // вывод CE дисплея
  #define LCD_RST 8
  #define LCD_BL  -1

  // --- TFT (ST7735 / ST7789): VCC на 3.3V ---
  #define TFT_SCK   12
  #define TFT_MOSI  11
  #define TFT_RST   8
  #define TFT_DC    9
  #define TFT_CS    10

#else
  #error "Неизвестный BOARD_TYPE: допустимы 1-4 (см. начало файла .ino)"
#endif

// ---- значения по умолчанию для параметров, которые задаёт не каждая плата ----
#ifndef TFT_BL
  #define TFT_BL -1                 // нет отдельной подсветки TFT
#endif
#ifndef BTN_UP_MODE
  #define BTN_UP_MODE INPUT_PULLUP
#endif
#ifndef BTN_DOWN_MODE
  #define BTN_DOWN_MODE INPUT_PULLUP
#endif
