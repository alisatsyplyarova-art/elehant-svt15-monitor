// Дисплей LCD5110 (PCD8544, 84x48, монохромный)
// Библиотеки: Adafruit GFX Library, Adafruit PCD8544 Nokia 5110 LCD library
//
// Раскладка экрана счётчика (84x48 точек):
//   y 0-8     заголовок: номер счётчика, дата справа (ДД.ММ)
//   y 9-27    слева две рамки тарифов "1:0.2613" и "2:0.3456", справа рамка 32x19: время или тревога
//   y 29      температура и батарея
//   y 38      статус, Wi-Fi: название сети и IP сменяют друг друга каждые 3 секунды
#pragma once
#include "monitor_common.h"
#include "monitor_profile.h"
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// Пины и контраст: monitor_boards.h, monitor_profile.h

class Svt15Lcd {
 public:
  Svt15Lcd() : lcd_(LCD_CLK, LCD_DIN, LCD_DC, LCD_CS, LCD_RST) {}

  void begin(uint8_t contrast = LCD_CONTRAST) {
    if (LCD_BL >= 0) {
      pinMode(LCD_BL, OUTPUT);
      digitalWrite(LCD_BL, HIGH);
    }
    if (LCD_VCC >= 0) {
      // Питание дисплея с пина: сброс отключением питания, затем включение
      pinMode(LCD_VCC, OUTPUT);
      digitalWrite(LCD_VCC, LOW);
      delay(100);
      digitalWrite(LCD_VCC, HIGH);
      delay(100);
    }
    lcd_.begin();
    lcd_.setContrast(contrast);
    lcd_.setTextSize(1);
    lcd_.setTextColor(BLACK);
    lcd_.clearDisplay();
    lcd_.display();
  }

  // Самотест: перебор контраста 30..90, чёрный экран и белая надпись C=xx.
  // Как только на экране что-то появилось, записать число C в LCD_CONTRAST.
  void selfTest() {
    char buf[12];
    for (int c = 30; c <= 90; c += 5) {
      lcd_.setContrast(c);
      lcd_.fillScreen(BLACK);
      lcd_.setTextColor(WHITE);
      lcd_.setCursor(20, 20);
      snprintf(buf, sizeof(buf), "C=%d", c);
      lcd_.print(buf);
      lcd_.display();
      SVT_SERIAL.printf("LCD selftest contrast=%d\n", c);
      delay(600);
    }
    lcd_.setTextColor(BLACK);
    lcd_.setContrast(LCD_CONTRAST);
  }

  void showSplash() {
    lcd_.clearDisplay();
    lcd_.setCursor(0, 4);  lcd_.print("Monitor");
    lcd_.setCursor(0, 14); lcd_.print("Elehant");
    lcd_.setCursor(0, 28); lcd_.print("v" SVT15_LCD_VERSION);
    lcd_.display();
  }

  void showSearching() {
    lcd_.clearDisplay();
    lcd_.setCursor(0, 8);  lcd_.print("Searching");
    lcd_.setCursor(0, 20); lcd_.print("for meters...");
    char wl[24];
    if (svt15WifiLine(wifi_, wl, sizeof(wl), 14, millis(), nullptr)) row(5, wl);
    lcd_.display();
  }

  // Сервисное меню: заголовок и до 5 строк, выбранная отмечена ">"
  void showMenu(const MenuView& v) {
    lcd_.clearDisplay();
    lcd_.fillRect(0, 0, 84, 9, BLACK);
    lcd_.setTextColor(WHITE);
    lcd_.setCursor(1, 1);
    lcd_.print(v.title);
    lcd_.setTextColor(BLACK);
    const int cap = 5;
    int first = v.sel >= cap ? v.sel - cap + 1 : 0;
    char b[16];
    for (int k = 0; k < cap && first + k < v.n; k++) {
      snprintf(b, sizeof(b), "%c%.13s", (first + k == v.sel) ? '>' : ' ', v.line[first + k]);
      row(k + 1, b);
    }
    lcd_.display();
  }

  void setBacklight(bool on) { if (LCD_BL >= 0) digitalWrite(LCD_BL, on ? HIGH : LOW); }

  // Состояние домашнего Wi-Fi
  void setWifiInfo(const WifiInfo& w) { wifi_ = w; }

  // Тревога или напоминание: мигающая рамка справа вверху, на месте времени
  void setAlert(const AlertInfo& a) { alert_ = a; }

  // Местное время и дата: время в рамке справа вверху (без секунд), дата в заголовке
  void setClock(const ClockInfo& c) { clock_ = c; }

  // Подписи тарифов: на этом экране всегда "1:" и "2:" (шрифт без кириллицы, место ограничено)
  void setTariffLabels(const char*, const char*) {}

  // index — номер счётчика на экране (с 0), count — всего счётчиков
  void showMeter(const MeterData& m, uint8_t index, uint8_t count) {
    char buf[24];
    lcd_.clearDisplay();

    // Заголовок: номер счётчика и дата справа. Если вместе не помещаются (14 символов),
    // номер и дата сменяют друг друга каждые 3 секунды. Пока время не установлено, вместо даты прочерки.
    lcd_.fillRect(0, 0, 84, 9, BLACK);
    lcd_.setTextColor(WHITE);
    char hs[20], ds[8], dl[12];
    if (count > 1) snprintf(hs, sizeof(hs), "%u/%u %c%lu", index + 1, count, svt15TypeChar(m.type), (unsigned long)m.id);
    else           snprintf(hs, sizeof(hs), "%c%lu", svt15TypeChar(m.type), (unsigned long)m.id);
    if (clock_.valid) {
      snprintf(ds, sizeof(ds), "%02u.%02u", clock_.day, clock_.mon);
      snprintf(dl, sizeof(dl), "%02u.%02u.%04u", clock_.day, clock_.mon, clock_.year);
    } else {
      snprintf(ds, sizeof(ds), "--.--");
      snprintf(dl, sizeof(dl), "--.--.----");
    }
    int hlen = strlen(hs);
    if (hlen + 6 <= 14) {
      lcd_.setCursor(1, 1);
      lcd_.print(hs);
      lcd_.setCursor(84 - 5 * 6 - 1, 1);
      lcd_.print(ds);
    } else {
      lcd_.setCursor(1, 1);
      lcd_.print(((millis() / 3000UL) % 2) == 1 ? dl : hs);
    }
    lcd_.setTextColor(BLACK);

    if (!m.valid) {
      lcd_.setCursor(0, 20);
      lcd_.print("NO DATA");
      lcd_.display();
      return;
    }

    // Рамки тарифов слева (51 точка), значения до 6 знаков
    char v[16];
    lcd_.drawRect(0, 9, 51, 10, BLACK);
    svt15FmtVol(v, sizeof(v), m.hasT1, m.t1, m.dec, 6);
    snprintf(buf, sizeof(buf), "1:%s", v);
    lcd_.setCursor(2, 10);
    lcd_.print(buf);
    if (m.twoTariff) {   // у однотарифного счётчика второго тарифа нет
      lcd_.drawRect(0, 18, 51, 10, BLACK);
      svt15FmtVol(v, sizeof(v), m.hasT2, m.t2, m.dec, 6);
      snprintf(buf, sizeof(buf), "2:%s", v);
      lcd_.setCursor(2, 19);
      lcd_.print(buf);
    }

    // Справа рамка: тревога или время
    if (alert_.show) drawAlert();
    else drawClock();

    // Общая сумма T1+T2 отдельной строкой (монохромный дисплей: без цветового выделения).
    if (m.twoTariff && m.hasT1 && m.hasT2) {
      snprintf(buf, sizeof(buf), "ОБ:%.*f", m.dec > 3 ? 3 : m.dec, m.t1 + m.t2);
      lcd_.setCursor(0, 29);
      lcd_.print(buf);
    }
    // Температура и батарея выводятся в последней строке; статус/Wi-Fi чередуется с ней.
    char ageStr[8];
    svt15AgeStr(m, ageStr, sizeof(ageStr));
    char st[24];
    snprintf(st, sizeof(st), "%s %s", svt15Status(m, nullptr), ageStr);
    const char* shown = st;
    char wl[36];
    uint8_t ph = (uint8_t)((millis() / 3000UL) % 3);
    if (wifi_.enabled && ph != 0) {
      if (wifi_.connected) snprintf(wl, sizeof(wl), "%.14s", ph == 1 ? wifi_.ssid : wifi_.ip);
      else snprintf(wl, sizeof(wl), "WiFi: %.8s", wifi_.state[0] ? wifi_.state : "wait");
      shown = wl;
    }
    // текст статуса не длиннее 12 символов: справа место под значок сигнала
    // значки сигнала слева (x 0..10), текст статуса от x=14: не больше 11 символов
    char cut[12];
    snprintf(cut, sizeof(cut), "%s", shown);
    drawSignalLcd(svt15SignalBars(m.rssi));
    lcd_.setCursor(14, 38);
    if ((millis() / 3000UL) % 2 == 0) {
      if (m.hasTemp) snprintf(cut, sizeof(cut), "T:%.1f B:%u", m.temp, m.battery);
      else snprintf(cut, sizeof(cut), "BAT:%u%%", m.battery);
    }
    lcd_.print(cut);

    lcd_.display();
  }

 private:
  // Значок сигнала: 4 столбика слева в строке статуса (x 0..10, y 38..46); заполненные — уровень сигнала
  void drawSignalLcd(int bars) {
    for (int i = 0; i < 4; i++) {
      int hgt = 2 * (i + 1);
      int x = i * 3;
      int y0 = 46 - hgt;
      if (i < bars) lcd_.fillRect(x, y0, 2, hgt, BLACK);
      else lcd_.drawRect(x, y0, 2, hgt, BLACK);
    }
  }

  // Рамка 32x19 справа от тарифов: название и причина (по 5 символов), мигает
  void drawAlert() {
    const int x0 = 52, y0 = 9, w = 32, h = 19;
    if (alert_.phase) {
      lcd_.fillRect(x0, y0, w, h, BLACK);
      lcd_.setTextColor(WHITE);
    } else {
      lcd_.fillRect(x0, y0, w, h, WHITE);
      lcd_.drawRect(x0, y0, w, h, BLACK);
      lcd_.setTextColor(BLACK);
    }
    char b[8];
    snprintf(b, sizeof(b), "%.5s", alert_.titleS);
    lcd_.setCursor(x0 + 1, y0 + 1);
    lcd_.print(b);
    snprintf(b, sizeof(b), "%.5s", alert_.reasonS);
    lcd_.setCursor(x0 + 1, y0 + 9);
    lcd_.print(b);
    lcd_.setTextColor(BLACK);
  }

  // Время ЧЧ:ММ в рамке на месте тревоги; двоеточие мигает; пока время не установлено "--:--"
  void drawClock() {
    char b[8];
    if (clock_.valid) snprintf(b, sizeof(b), "%02u%c%02u", clock_.hour, ((millis() / 500UL) % 2) ? ':' : ' ', clock_.min);
    else snprintf(b, sizeof(b), "--:--");
    lcd_.drawRect(52, 9, 32, 19, BLACK);
    lcd_.setCursor(53, 14);
    lcd_.print(b);
  }

  void row(uint8_t r, const char* s) {
    lcd_.setCursor(0, r * 8 + 1);
    lcd_.print(s);
  }

  WifiInfo wifi_;
  AlertInfo alert_;
  ClockInfo clock_;
  Adafruit_PCD8544 lcd_;
};
