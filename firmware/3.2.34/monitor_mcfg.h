// Настройки счётчика (по типу и серийному номеру): названия тарифов и параметры отправки в Telegram.
// Хранятся в Preferences блоком (структура без String), редактируются в WEB на странице счётчика.
#pragma once
#include <Preferences.h>
#include "monitor_common.h"
#include "monitor_meters.h"

// Данные в сообщении Telegram: биты tgFields
#define SVT_TGF_T1    1     // тариф 1 (или показания однотарифного)
#define SVT_TGF_T2    2     // тариф 2
#define SVT_TGF_TEMP  4     // температура
#define SVT_TGF_BAT   8     // батарея
#define SVT_TGF_RSSI  16    // уровень сигнала
#define SVT_TGF_FW    32    // версия прошивки счётчика

// Расписание передачи показаний: tgMode
#define SVT_TGM_OFF     0   // показания не отправляются (тревоги отправляются)
#define SVT_TGM_DAILY   1   // каждый день
#define SVT_TGM_WEEKLY  2   // раз в неделю (tgDay: 0 воскресенье ... 6 суббота)
#define SVT_TGM_MONTHLY 3   // раз в месяц (tgDay: число 1-28)

struct MeterCfg {
  uint32_t id;
  uint32_t tgLast;          // ключ последней отправки показаний ГГГГММДД
  uint8_t type;
  uint8_t used;
  uint8_t tgOn;             // счётчик участвует в отправке в Telegram
  uint8_t tgMode;
  uint8_t tgDay;
  uint8_t tgHour;           // час отправки 0-23
  uint8_t tgFields;
  uint8_t tgAlarms;         // биты типов тревог, которые отправляются (AL_LOW_BAT = бит 0 ...)
  uint8_t tgRepeatH;        // повтор уведомления о незакрытой тревоге, часов (0: один раз)
  uint8_t pad[3];
  char name1[24];           // название тарифа 1 (пусто: по умолчанию)
  char name2[24];           // название тарифа 2
};

#define SVT_MCFG_VERSION 1

class MeterCfgStore {
 public:
  void begin() {
    Preferences p;
    if (!p.begin("svt15mcfg", true)) return;
    size_t len = p.getBytesLength("tbl");
    if (len > 0 && len % sizeof(MeterCfg) == 0 && len / sizeof(MeterCfg) <= MAX_METERS &&
        p.getUShort("ver", 0) == SVT_MCFG_VERSION) {
      p.getBytes("tbl", e_, len);
      n_ = len / sizeof(MeterCfg);
    }
    p.end();
  }

  void save() {
    Preferences p;
    if (!p.begin("svt15mcfg", false)) return;
    p.putUShort("ver", SVT_MCFG_VERSION);
    p.putBytes("tbl", e_, n_ * sizeof(MeterCfg));
    p.end();
  }

  // Найти настройки счётчика или создать с значениями по умолчанию
  MeterCfg& at(uint8_t type, uint32_t id) {
    for (uint8_t i = 0; i < n_; i++)
      if (e_[i].used && e_[i].id == id && e_[i].type == type) return e_[i];
    if (n_ >= MAX_METERS) return overflow_ = defaults(type, id);
    e_[n_] = defaults(type, id);
    return e_[n_++];
  }

  // Название тарифа: заданное или по умолчанию
  const char* name(uint8_t type, uint32_t id, uint8_t which, bool twoTariff) {
    MeterCfg& c = at(type, id);
    const char* n = which == 1 ? c.name1 : c.name2;
    if (n[0]) return n;
    if (!twoTariff) return "Показания";
    return which == 1 ? "Тариф 1" : "Тариф 2";
  }

  // Подпись для экрана (латиница): название тарифа, если оно из символов ASCII, иначе T1 или T2
  void screenLabel(uint8_t type, uint32_t id, uint8_t which, char* out, size_t n) {
    MeterCfg& c = at(type, id);
    const char* s = which == 1 ? c.name1 : c.name2;
    bool ok = s[0] != 0;
    for (const char* p = s; *p; p++) if ((uint8_t)*p < 0x20 || (uint8_t)*p > 0x7E) { ok = false; break; }
    snprintf(out, n, "%.*s", (int)n - 1, ok ? s : (which == 1 ? "T1" : "T2"));
  }

  // Допустимое название: до 23 байт UTF-8, без управляющих символов и кавычек
  static bool validName(const char* s) {
    size_t n = strlen(s);
    if (n > 23) return false;
    for (size_t i = 0; i < n; i++) if ((uint8_t)s[i] < 0x20 || s[i] == '"' || s[i] == '\\' || s[i] == '<' || s[i] == '>') return false;
    return true;
  }

  uint8_t count() const { return n_; }

 private:
  static MeterCfg defaults(uint8_t type, uint32_t id) {
    MeterCfg c;
    memset(&c, 0, sizeof(c));
    c.id = id;
    c.type = type;
    c.used = 1;
    c.tgOn = 1;
    c.tgMode = SVT_TGM_MONTHLY;
    c.tgDay = 23;
    c.tgHour = 12;
    c.tgFields = SVT_TGF_T1 | SVT_TGF_T2;
    c.tgAlarms = 0x3F;
    return c;
  }

  MeterCfg e_[MAX_METERS];
  MeterCfg overflow_;
  uint8_t n_ = 0;
};
