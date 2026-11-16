// Цветной TFT по SPI: ST7735 (1.77" 160x128) или ST7789
// Библиотеки: Adafruit GFX Library, Adafruit ST7735 and ST7789 Library
// Нужен DISPLAY_TYPE 2 (ST7735) или 3 (ST7789), задаётся в файле .ino
#pragma once
#include "monitor_common.h"
#include "monitor_profile.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#if DISPLAY_TYPE == DISPLAY_ST7735
  #include <Adafruit_ST7735.h>
  typedef Adafruit_ST7735 TftDriver;
#else
  #include <Adafruit_ST7789.h>
  typedef Adafruit_ST7789 TftDriver;
#endif

// Пины и настройки: monitor_profile.h, monitor_boards.h

#define C_BG    0x0000
#define C_FG    0xFFFF
#define C_HDR   0x041F   // фон заголовка
#define C_HOT   0xFD20   // тариф 1
#define C_COLD  0x07FF   // тариф 2
#define C_TOTAL 0xD6BF   // сиреневый: T1 + T2
#define C_OK    0x07E0
#define C_WARN  0xFFE0
#define C_BAD   0xF800

class Svt15Tft {
 public:
  Svt15Tft() : tft_(&SPI, TFT_CS, TFT_DC, TFT_RST) {}

  void begin() {
    if (TFT_BL >= 0) {                // отдельная подсветка (встроенный дисплей)
      pinMode(TFT_BL, OUTPUT);
      digitalWrite(TFT_BL, HIGH);
    }
    SPI.begin(TFT_SCK, -1, TFT_MOSI, -1);
#if DISPLAY_TYPE == DISPLAY_ST7735
    tft_.initR(ST7735_INIT);
#else
    tft_.init(ST7789_W, ST7789_H);
#endif
    tft_.setRotation(TFT_ROTATION);
    tft_.setTextWrap(false);
    tft_.fillScreen(C_BG); frameDrawn_ = false;
    w_ = tft_.width();
    h_ = tft_.height();
    // Компактная служебная типографика; показания выводятся отдельно крупным шрифтом 2x.
    ss_ = 1;
    vs_ = 1;
    blockH_ = 24;
    boxW_ = w_ * 5 / 12;             // правая зона часов/оповещения
    textW_ = w_;
    SVT_SERIAL.printf("TFT %dx%d, text %d/%d\n", w_, h_, ss_, vs_);
  }

  // Самотест: красный, зелёный, синий
  void selfTest() {
    const uint16_t cols[3] = {0xF800, 0x07E0, 0x001F};
    for (int i = 0; i < 3; i++) {
      tft_.fillScreen(cols[i]);
      delay(400);
    }
    tft_.fillScreen(C_BG); frameDrawn_ = false;
    mode_ = 0;
  }

  void showSplash() {
    tft_.fillScreen(C_BG); frameDrawn_ = false;
    text(8 * ss_, ss_, C_FG, C_BG, "Monitor Elehant");
    text(8 * ss_ * 3, ss_, C_HOT, C_BG, "v" SVT15_LCD_VERSION);
    mode_ = 0;
  }

  void showSearching() {
    if (mode_ != 2) { tft_.fillScreen(C_BG); frameDrawn_ = false; mode_ = 2; }
    text(8 * ss_, ss_, C_FG, C_BG, "Searching");
    text(8 * ss_ * 3, ss_, C_FG, C_BG, "for meters...");
    wifiLine(8 * ss_ * 5);
  }

  // Сервисное меню
  void showMenu(const MenuView& v) {
    uint32_t sig = (uint32_t)v.n * 131u;
    for (const char* p = v.title; *p; p++) sig = sig * 31u + (uint8_t)*p;
    if (mode_ != 3 || menuSig_ != sig) { tft_.fillScreen(C_BG); frameDrawn_ = false; mode_ = 3; menuSig_ = sig; }
    textW_ = w_;
    text(0, ss_, C_FG, C_HDR, v.title, w_);
    int16_t lh = 8 * ss_ + 4;
    int16_t y0 = 8 * ss_ + 8;
    int cap = (h_ - y0) / lh;
    int first = v.sel >= cap ? v.sel - cap + 1 : 0;
    for (int k = 0; k < cap; k++) {
      int idx = first + k;
      char b[40];
      if (idx < v.n) snprintf(b, sizeof(b), "%c %.30s", idx == v.sel ? '>' : ' ', v.line[idx]);
      else b[0] = 0;
      text(y0 + k * lh, ss_, C_FG, idx == v.sel ? C_HDR : C_BG, b, w_);
    }
  }

  // Подписи тарифов (латиница): название тарифа или T1 и T2
  void setTariffLabels(const char* a, const char* b) {
    snprintf(lab1_, sizeof(lab1_), "%s", a);
    snprintf(lab2_, sizeof(lab2_), "%s", b);
  }

  void showMeter(const MeterData& m, uint8_t index, uint8_t count) {
    if (mode_ != 1 || idx_ != index) { tft_.fillScreen(C_BG); frameDrawn_ = false; mode_ = 1; idx_ = index; }
    char buf[40];
    int16_t y = 0;
    textW_ = w_;

    // Заголовок: номер счётчика слева, дата справа (ДД.ММ.ГГГГ, если не помещается: ДД.ММ);
    // пока время не установлено, вместо даты прочерки
    char left[24], line[64];
    snprintf(left, sizeof(left), "%u/%u  %c%lu", index + 1, count, svt15TypeChar(m.type), (unsigned long)m.id);
    int hcols = (w_ - 2) / (6 * ss_);
    if (hcols > 62) hcols = 62;
    memset(line, ' ', hcols);
    line[hcols] = 0;
    int ll = strlen(left);
    if (ll > hcols) ll = hcols;
    memcpy(line, left, ll);
    {
      char d[12];
      int dl = 10;
      if (clock_.valid) snprintf(d, sizeof(d), "%02u.%02u.%04u", clock_.day, clock_.mon, clock_.year);
      else snprintf(d, sizeof(d), "--.--.----");
      if (ll + 1 + dl > hcols) {
        dl = 5;
        if (clock_.valid) snprintf(d, sizeof(d), "%02u.%02u", clock_.day, clock_.mon);
        else snprintf(d, sizeof(d), "--.--");
      }
      if (ll + 1 + dl <= hcols) memcpy(line + hcols - dl, d, dl);
    }
    text(y, ss_, C_FG, C_HDR, line, w_);
    y += 8 * ss_ + 8;

    if (!m.valid) {
      text(y, ss_, C_BAD, C_BG, "NO DATA");
      return;
    }

    // Тарифы: три одинаковые по размеру ячейки на всю ширину экрана.
    // Подписи и показания имеют один и тот же крупный размер шрифта.
    char v1[24], v2[24], vt[24];
    svt15FmtVol(v1, sizeof(v1), m.hasT1, m.t1, m.dec, 5);
    if (m.twoTariff) {
      svt15FmtVol(v2, sizeof(v2), m.hasT2, m.t2, m.dec, 5);
      if (m.hasT1 && m.hasT2) svt15FmtVol(vt, sizeof(vt), true, m.t1 + m.t2, m.dec, 5);
      else snprintf(vt, sizeof(vt), "--");
      // Первые два тарифа занимают левую колонку: справа остаётся отдельное окно часов/оповещения.
      // Подпись занимает строго две позиции, значение печатается увеличенным шрифтом.
      tariffCell(y, "T1", v1, C_HOT, w_ - boxW_ - 2); y += 24;
      tariffCell(y, "T2", v2, C_COLD, w_ - boxW_ - 2); y += 24;
      tariffCell(y, "ОБ", vt, C_TOTAL, w_); y += 24;
    } else {
      tariffCell(y, lab1_, v1, C_HOT, w_); y += 18;
    }
    textW_ = w_;

    if (m.hasTemp) snprintf(buf, sizeof(buf), "T:%.1fC BAT:%u%%", m.temp, m.battery);
    else           snprintf(buf, sizeof(buf), "BAT:%u%%", m.battery);
    text(y, ss_, C_FG, C_BG, buf);
    drawSignal(y, svt15SignalBars(m.rssi));    // значок сигнала справа в строке батареи
    y += 12;

    Svt15Level lvl;
    const char* st = svt15Status(m, &lvl);
    char ageStr[8];
    svt15AgeStr(m, ageStr, sizeof(ageStr));
    snprintf(buf, sizeof(buf), "%s %s", st, ageStr);
    text(y, ss_, lvl == SVT_OK ? C_OK : (lvl == SVT_WARN ? C_WARN : C_BAD), C_BG, buf);
    y += 12;

    wifiLine(y);   // домашний Wi-Fi: IP и название сети

    if (alert_.show) drawAlert();
    else drawClock();
  }

  // Состояние домашнего Wi-Fi
  void setWifiInfo(const WifiInfo& w) { wifi_ = w; }

  // Тревога или напоминание: мигающая рамка справа вверху, на месте времени
  void setAlert(const AlertInfo& a) { alert_ = a; }

  // Местное время и дата: время в рамке справа вверху (без секунд), дата в заголовке
  void setClock(const ClockInfo& c) { clock_ = c; }

 private:
  // Значок сигнала: 4 столбика справа в строке y, высота ступеней растёт; цвет по уровню
  void drawSignal(int16_t y, int bars) {
    const int16_t bw = 3 * ss_, gap = ss_;
    const int16_t total = 4 * bw + 3 * gap;
    const int16_t x0 = w_ - total - 2;
    const int16_t bottom = y + 8 * ss_ - 1;
    uint16_t col = bars >= 3 ? C_OK : (bars == 2 ? C_WARN : C_BAD);
    for (int i = 0; i < 4; i++) {
      int16_t bh = (i + 1) * 2 * ss_;
      int16_t x = x0 + i * (bw + gap);
      if (i < bars) tft_.fillRect(x, bottom - bh, bw, bh, col);
      else tft_.drawRect(x, bottom - bh, bw, bh, C_HDR);
    }
  }

  // Положение рамки справа: по высоте как два блока тарифов
  void boxRect(int16_t& x, int16_t& y, int16_t& w, int16_t& h) {
    x = w_ - boxW_ - 1;
    y = 8 * ss_ + 8;
    w = boxW_;
    h = 2 * 24 - 2;
  }

  // Время ЧЧ:ММ в рамке справа. Рамка рисуется один раз. Цифры перерисовываются при смене минуты.
  // Двоеточие мигает: перерисовывается только его ячейка, рамка и цифры не трогаются.
  // Пока время не установлено, "--:--" без мигания.
  void drawClock() {
    int16_t x0, y0, bw, bh;
    boxRect(x0, y0, bw, bh);
    char hh[4] = "--", mm[4] = "--";
    if (clock_.valid) {
      snprintf(hh, sizeof(hh), "%02u", (unsigned)clock_.hour);
      snprintf(mm, sizeof(mm), "%02u", (unsigned)clock_.min);
    }
    int sc = (bw - 6) / 30;                    // "ЧЧ:ММ" = 5 знаков по 6 точек на единицу масштаба
    if (sc > 3) sc = 3;
    if (sc < 1) sc = 1;
    int cw = 6 * sc, ch = 8 * sc;
    int16_t tx = x0 + (bw - 5 * cw) / 2, ty = y0 + (bh - ch) / 2;
    char key[6];
    snprintf(key, sizeof(key), "%s%s", hh, mm);
    if (!frameDrawn_) {
      tft_.fillRect(x0, y0, bw, bh, C_BG);
      tft_.drawRect(x0, y0, bw, bh, C_HDR);
      frameDrawn_ = true;
      lastClock_[0] = 0;
      lastColon_ = 255;
    }
    uint16_t fg = clock_.valid ? C_FG : C_WARN;
    if (strcmp(key, lastClock_) != 0) {         // смена минуты: цифры заново, только внутри рамки
      tft_.fillRect(tx - 1, ty - 1, 5 * cw + 2, ch + 2, C_BG);
      tft_.setTextSize(sc);
      tft_.setTextColor(fg, C_BG);
      tft_.setCursor(tx, ty);
      tft_.print(hh);
      tft_.setCursor(tx + 3 * cw, ty);
      tft_.print(mm);
      snprintf(lastClock_, sizeof(lastClock_), "%s", key);
      lastColon_ = 255;
    }
    bool colonOn = !clock_.valid || ((millis() / 500UL) % 2) == 0;
    if (colonOn != (lastColon_ == 1)) {         // мигание: только ячейка двоеточия
      tft_.fillRect(tx + 2 * cw, ty, cw, ch, C_BG);
      if (colonOn) {
        tft_.setTextSize(sc);
        tft_.setTextColor(fg, C_BG);
        tft_.setCursor(tx + 2 * cw, ty);
        tft_.print(":");
      }
      lastColon_ = colonOn ? 1 : 0;
    }
  }

  // Рамка тревоги: название, причина, SN, номер и подсказка; мигает (две цветовые фазы)
  void drawAlert() {
    int16_t x0, y0, bw, bh;
    boxRect(x0, y0, bw, bh);
    frameDrawn_ = false;                       // после тревоги рамка часов рисуется заново
    lastClock_[0] = 0;
    uint16_t accent = alert_.kind == 1 ? C_BAD : C_COLD;
    uint16_t bg = alert_.phase ? (alert_.kind == 1 ? C_BAD : C_HDR) : C_BG;
    uint16_t fg = alert_.phase ? C_FG : accent;
    tft_.fillRect(x0, y0, bw, bh, bg);
    tft_.drawRect(x0, y0, bw, bh, alert_.phase ? C_FG : accent);
    int cols = (bw - 4) / 6;
    if (cols > 20) cols = 20;
    char line[24];
    int16_t ly = y0 + 4;
    tft_.setTextSize(1);
    tft_.setTextColor(fg, bg);
    for (int k = 0; k < 4; k++) {
      switch (k) {
        case 0: snprintf(line, sizeof(line), "%s", alert_.titleL); break;
        case 1: snprintf(line, sizeof(line), "%s", alert_.reasonL); break;
        case 2: snprintf(line, sizeof(line), "SN %s", alert_.sn); break;
        default:
          if (alert_.count > 1) snprintf(line, sizeof(line), "%u/%u", alert_.pos, alert_.count);
          else snprintf(line, sizeof(line), "key: ack");
          break;
      }
      if ((int)strlen(line) > cols) line[cols] = 0;
      if (ly + 8 > y0 + bh) break;
      tft_.setCursor(x0 + 3, ly);
      tft_.print(line);
      ly += 10;
    }
  }

  // Строка домашнего Wi-Fi: "IP название" при подключении, "Wi-Fi: причина" при отказе
  void wifiLine(int16_t y) {
    char buf[64];
    uint16_t col = C_OK;
    if (!wifi_.enabled) {
      buf[0] = 0;
    } else if (wifi_.connected) {
      snprintf(buf, sizeof(buf), "%s %s", wifi_.ip, wifi_.ssid);
    } else {
      snprintf(buf, sizeof(buf), "Wi-Fi: %s", wifi_.state[0] ? wifi_.state : "wait");
      col = C_WARN;
    }
    text(y, ss_, col, C_BG, buf);
  }

  // Строка текста, дополненная пробелами до края: затирает старое без мерцания
  // width: ширина области (0 = текущая textW_, она уже, пока рисуются тарифы)
  void text(int16_t y, uint8_t scale, uint16_t fg, uint16_t bg, const char* s, int16_t width = 0) {
    const int16_t x0 = 2;
    int cols = ((width > 0 ? width : textW_) - x0) / (6 * scale);
    if (cols > 62) cols = 62;
    char buf[64];
    int n = strlen(s);
    if (n > cols) n = cols;
    memcpy(buf, s, n);
    while (n < cols) buf[n++] = ' ';
    buf[n] = 0;
    tft_.setTextSize(scale);
    tft_.setTextColor(fg, bg);
    tft_.setCursor(x0, y);
    tft_.print(buf);
  }

  // Ячейка показаний: короткая подпись (2 символа) и крупное значение.
  // width ограничивает первые две строки слева от часов/окна оповещения.
  void tariffCell(int16_t y, const char* label, const char* value, uint16_t col, int16_t width) {
    const int16_t cellH = 24;
    if (width > w_) width = w_;
    tft_.drawRect(0, y, width, cellH, col);
    tft_.setTextSize(1);
    tft_.setTextColor(col, C_BG);
    tft_.setCursor(3, y + 8);
    tft_.print(label);
    const uint8_t valueScale = 2;
    int maxChars = (width - 20) / (6 * valueScale);
    char shown[24];
    snprintf(shown, sizeof(shown), "%s", value);
    if ((int)strlen(shown) > maxChars && maxChars > 0) shown[maxChars] = 0;
    int16_t x = width - 4 - (int16_t)strlen(shown) * 6 * valueScale;
    if (x < 18) x = 18;
    tft_.setTextSize(valueScale);
    tft_.setTextColor(col, C_BG);
    tft_.setCursor(x, y + 4);
    tft_.print(shown);
  }

  WifiInfo wifi_;
  AlertInfo alert_;
  ClockInfo clock_;
  int16_t w_ = 0, h_ = 0;
  int16_t boxW_ = 0;     // ширина правой рамки
  int16_t textW_ = 0;    // ширина области текста (уже, пока рисуются тарифы)
  int16_t blockH_ = 0;   // высота блока тарифа с промежутком
  uint8_t ss_ = 1, vs_ = 1;
  uint8_t mode_ = 0;     // 0 — ничего, 1 — счётчик, 2 — поиск, 3 — меню
  uint8_t idx_ = 255;
  uint32_t menuSig_ = 0;
  char lab1_[10] = "T1";
  char lab2_[10] = "T2";
  bool frameDrawn_ = false;          // рамка часов уже нарисована
  char lastClock_[6] = "";           // последние показанные ЧЧММ
  uint8_t lastColon_ = 255;          // состояние двоеточия: 1 видно, 0 скрыто, 255 не рисовано
  TftDriver tft_;
};
