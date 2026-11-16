// Кнопки UP и DOWN: одиночное нажатие, обе вместе (короткое), обе вместе удержание 3 секунды.
// Одиночное нажатие выдаётся через 80 мс (ожидание второй кнопки), "обе" выдаются при отпускании.
#pragma once
#include <Arduino.h>
#include "monitor_boards.h"

enum BtnEvent : uint8_t { BTN_NONE = 0, BTN_UP, BTN_DOWN, BTN_BOTH, BTN_BOTH_LONG };

#define SVT_BTN_LONG_MS 3000UL

class Buttons {
 public:
  BtnEvent poll() {
    uint32_t now = millis();
    bool up = digitalRead(BTN_UP_PIN) == LOW;
    bool dn = digitalRead(BTN_DOWN_PIN) == LOW;
    switch (st_) {
      case IDLE:
        if (now - tRel_ >= 40 && (up || dn)) { st_ = PENDING; t0_ = now; }
        break;
      case PENDING:
        if (up && dn) { st_ = BOTH; t0_ = now; }
        else if (!up && !dn) { st_ = IDLE; tRel_ = now; }          // дребезг
        else if (now - t0_ >= 80) { st_ = WAITREL; return up ? BTN_UP : BTN_DOWN; }
        break;
      case BOTH:
        if (!up && !dn) { st_ = IDLE; tRel_ = now; return BTN_BOTH; }
        if (now - t0_ >= SVT_BTN_LONG_MS) { st_ = WAITREL; return BTN_BOTH_LONG; }
        break;
      case WAITREL:
        if (!up && !dn) { st_ = IDLE; tRel_ = now; }
        break;
    }
    return BTN_NONE;
  }

 private:
  enum St : uint8_t { IDLE, PENDING, BOTH, WAITREL } st_ = IDLE;
  uint32_t t0_ = 0, tRel_ = 0;
};
