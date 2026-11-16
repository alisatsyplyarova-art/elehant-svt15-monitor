// Таблица счётчиков (до 40) и хранение в Preferences (NVS)
// Ключ счётчика: тип + серийный номер. Обновляется из BLE-задачи, читается из loop().
#pragma once
#include <Preferences.h>
#include "monitor_common.h"

#define MAX_METERS SVT_MAX_METERS
#define SVT_SAVE_INTERVAL_MS   (15UL * 60UL * 1000UL)  // периодическое сохранение
#define SVT_SAVE_NEW_DELAY_MS  5000UL                  // пауза после появления нового счётчика
#define SVT_TBL_VERSION        1

static portMUX_TYPE svtMux = portMUX_INITIALIZER_UNLOCKED;

// Формат хранения (без String, не зависит от MeterData)
struct StoredMeter {
  uint32_t id;
  uint32_t ageSec;      // возраст последнего пакета на момент сохранения
  double   t1, t2;
  float    temp, fw;
  uint8_t  type, model, version, battery;
  int8_t   rssi;
  uint8_t  dec;
  uint8_t  bits;        // 1 twoTariff, 2 hasT1, 4 hasT2, 8 hasTemp
  uint8_t  pad;
};

class MeterManager {
 public:
  // Чтение сохранённых счётчиков при старте
  void begin() {
    Preferences p;
    if (!p.begin("svt15", true)) return;     // пространства ещё нет — первый запуск
    size_t len = p.getBytesLength("tbl");
    if (len > 0 && len % sizeof(StoredMeter) == 0 &&
        len / sizeof(StoredMeter) <= MAX_METERS &&
        p.getUShort("tver", 0) == SVT_TBL_VERSION) {
      StoredMeter buf[MAX_METERS];
      p.getBytes("tbl", buf, len);
      uint8_t n = len / sizeof(StoredMeter);
      for (uint8_t i = 0; i < n; i++) {
        MeterData m;
        const StoredMeter& s = buf[i];
        m.id = s.id; m.type = s.type; m.model = s.model; m.version = s.version;
        m.t1 = s.t1; m.t2 = s.t2; m.temp = s.temp; m.fw = s.fw;
        m.battery = s.battery; m.rssi = s.rssi; m.dec = s.dec;
        m.twoTariff = s.bits & 1; m.hasT1 = s.bits & 2; m.hasT2 = s.bits & 4; m.hasTemp = s.bits & 8;
        m.valid = true;
        m.lastSeenMs = millis() - s.ageSec * 1000UL;   // время выключения не учитывается
        meters_[i] = m;
      }
      count_ = n;
    }
    p.end();
  }

  // Из BLE-задачи: добавить или обновить счётчик, вернуть индекс (-1 — таблица полна)
  int update(const MeterData& in, uint8_t* total) {
    MeterData m = in;
    int idx = -1;
    portENTER_CRITICAL(&svtMux);
    for (uint8_t i = 0; i < count_; i++) {
      if (meters_[i].id == m.id && meters_[i].type == m.type) { idx = i; break; }
    }
    if (idx >= 0) {
      // пакет мог нести только один тариф: второй берём из таблицы
      if (!m.hasT1) { m.t1 = meters_[idx].t1; m.hasT1 = meters_[idx].hasT1; }
      if (!m.hasT2) { m.t2 = meters_[idx].t2; m.hasT2 = meters_[idx].hasT2; }
    } else if (count_ < MAX_METERS) {
      idx = count_++;
      newPending_ = true;
    }
    if (idx >= 0) {
      const MeterData& old = meters_[idx];
      m.rx = old.rx;
      m.lost = old.lost;
      m.seqLast[0] = old.seqLast[0];
      m.seqLast[1] = old.seqLast[1];
      m.seqHave[0] = old.seqHave[0];
      m.seqHave[1] = old.seqHave[1];
      if (m.seqValid) {
        uint8_t s = m.seqStream & 1;
        m.rx++;
        if (old.seqHave[s]) {
          uint8_t gap = (uint8_t)(m.seq - old.seqLast[s] - 1);
          if (gap < 32) m.lost += gap;         // большой скачок: счётчик перезапущен, потерь не считаем
        }
        m.seqLast[s] = m.seq;
        m.seqHave[s] = 1;
      }
      meters_[idx] = m;
      dirty_ = true;
    }
    if (total) *total = count_;
    portEXIT_CRITICAL(&svtMux);
    return idx;
  }

  uint8_t count() const { return count_; }

  // Копия записи (безопасно для loop())
  bool get(uint8_t i, MeterData& out) {
    bool ok = false;
    portENTER_CRITICAL(&svtMux);
    if (i < count_) { out = meters_[i]; ok = true; }
    portEXIT_CRITICAL(&svtMux);
    return ok;
  }

  // Вызывать из loop(): запись в NVS редко и только при изменениях
  void loop() {
    uint32_t now = millis();
    if (newPending_ && now - lastSave_ > SVT_SAVE_NEW_DELAY_MS) {
      save();
    } else if (dirty_ && now - lastSave_ > SVT_SAVE_INTERVAL_MS) {
      save();
    }
  }

  void save() {
    StoredMeter buf[MAX_METERS];
    uint8_t n;
    uint32_t now = millis();
    portENTER_CRITICAL(&svtMux);
    n = count_;
    for (uint8_t i = 0; i < n; i++) {
      const MeterData& m = meters_[i];
      StoredMeter& s = buf[i];
      memset(&s, 0, sizeof(s));
      s.id = m.id; s.type = m.type; s.model = m.model; s.version = m.version;
      s.ageSec = (now - m.lastSeenMs) / 1000UL;
      s.t1 = m.t1; s.t2 = m.t2; s.temp = m.temp; s.fw = m.fw;
      s.battery = m.battery; s.rssi = m.rssi; s.dec = m.dec;
      s.bits = (m.twoTariff ? 1 : 0) | (m.hasT1 ? 2 : 0) | (m.hasT2 ? 4 : 0) | (m.hasTemp ? 8 : 0);
    }
    dirty_ = false;
    newPending_ = false;
    portEXIT_CRITICAL(&svtMux);
    lastSave_ = now;
    if (n == 0) return;
    Preferences p;
    if (!p.begin("svt15", false)) return;
    p.putUShort("tver", SVT_TBL_VERSION);
    p.putBytes("tbl", buf, n * sizeof(StoredMeter));
    p.end();
  }

 private:
  MeterData meters_[MAX_METERS];
  volatile uint8_t count_ = 0;
  volatile bool dirty_ = false;
  volatile bool newPending_ = false;
  uint32_t lastSave_ = 0;
};
