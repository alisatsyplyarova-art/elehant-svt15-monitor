// Тревоги и напоминания.
// Тревоги (по каждому счётчику): LOW BAT, HIGH TEMP, NO DATA, NO FLOW, DAILY LIMIT, WEAK BLE.
// Состояние тревоги: активна, подтверждена; при нормализации сбрасывается, при повторе создаётся новая.
// Подавление (по типу тревоги, на заданное число часов): тревога не выводится на экран,
// но обнаружение продолжается и в WEB она остаётся активной.
// Напоминание: ежемесячное "передать показания" (по умолчанию 23 число, 12:00-23:00).
#pragma once
#include <Preferences.h>
#include <math.h>
#include "monitor_common.h"
#include "monitor_meters.h"
#include "monitor_time.h"

enum AlarmType : uint8_t {
  AL_LOW_BAT = 0, AL_HIGH_TEMP, AL_NO_DATA, AL_NO_FLOW, AL_DAILY_LIMIT, AL_WEAK_BLE, AL_COUNT
};

static const char* const SVT_ALARM_NAME[AL_COUNT] = {
  "Низкий заряд батареи", "Высокая температура", "Нет данных", "Нет расхода",
  "Суточный лимит", "Слабый сигнал BLE"
};

// Латинские названия для Serial0
static const char* const SVT_ALARM_NAME_LAT[AL_COUNT] = {
  "LOW BAT", "HIGH TEMP", "NO DATA", "NO FLOW", "DAILY LIMIT", "WEAK BLE"
};

struct AlarmSettings {
  uint16_t mask = 0x3F;        // включённые тревоги: бит на тип
  uint8_t  lowBat = 20;        // %
  int16_t  highTemp = 60;      // °C
  uint16_t noDataH = 72;       // часов без данных
  uint16_t noFlowH = 72;       // часов без расхода
  uint32_t limit1 = 0;         // суточный лимит тарифа 1, литров (0 = выключен)
  uint32_t limit2 = 0;         // суточный лимит тарифа 2, литров
  int16_t  weakDbm = -85;      // dBm
  bool     remEnabled = true;  // ежемесячное напоминание
  uint8_t  remDay = 23;        // число месяца
  uint8_t  remFrom = 12;       // час начала
  uint8_t  remTo = 23;         // час окончания
};

struct AlarmState {
  bool active = false;
  bool acked = false;
  uint32_t sinceMs = 0;
};

struct AlarmItem {
  uint8_t kind;    // 1 тревога, 2 напоминание
  uint8_t meter;
  uint8_t type;
};

#define SVT_ALERT_MAX 32

class AlertManager {
 public:
  AlarmSettings cfg;

  void begin(MeterManager* meters, TimeManager* clk) {
    meters_ = meters;
    clk_ = clk;
    for (uint8_t i = 0; i < MAX_METERS; i++) flow_[i].lastTotal = -1;
    load();
  }

  void load() {
    Preferences p;
    if (!p.begin("svt15alm", true)) return;
    cfg.mask = p.getUShort("mask", cfg.mask);
    cfg.lowBat = p.getUChar("lowbat", cfg.lowBat);
    cfg.highTemp = p.getShort("temp", cfg.highTemp);
    cfg.noDataH = p.getUShort("nodata", cfg.noDataH);
    cfg.noFlowH = p.getUShort("noflow", cfg.noFlowH);
    cfg.limit1 = p.getUInt("lim1", 0);
    cfg.limit2 = p.getUInt("lim2", 0);
    cfg.weakDbm = p.getShort("weak", cfg.weakDbm);
    cfg.remEnabled = p.getBool("rem_en", true);
    cfg.remDay = p.getUChar("rem_day", cfg.remDay);
    cfg.remFrom = p.getUChar("rem_from", cfg.remFrom);
    cfg.remTo = p.getUChar("rem_to", cfg.remTo);
    p.end();
  }

  void save() {
    Preferences p;
    if (!p.begin("svt15alm", false)) return;
    p.putUShort("mask", cfg.mask);
    p.putUChar("lowbat", cfg.lowBat);
    p.putShort("temp", cfg.highTemp);
    p.putUShort("nodata", cfg.noDataH);
    p.putUShort("noflow", cfg.noFlowH);
    p.putUInt("lim1", cfg.limit1);
    p.putUInt("lim2", cfg.limit2);
    p.putShort("weak", cfg.weakDbm);
    p.putBool("rem_en", cfg.remEnabled);
    p.putUChar("rem_day", cfg.remDay);
    p.putUChar("rem_from", cfg.remFrom);
    p.putUChar("rem_to", cfg.remTo);
    p.end();
  }

  // Из loop(): раз в секунду проверка условий и обновление списка для экрана
  void loop() {
    uint32_t now = millis();
    if (now - lastEval_ < 1000UL) return;
    lastEval_ = now;
    evaluate(now);
    rebuild(now);
  }

  // ---------- для экрана ----------
  uint8_t visibleCount() const { return n_; }

  // Текущий показываемый элемент (меняется каждые 3 секунды)
  bool current(AlertInfo& a) {
    a = AlertInfo();
    if (n_ == 0) return false;
    uint32_t now = millis();
    shown_ = (uint8_t)((now / 3000UL) % n_);
    const AlarmItem& it = items_[shown_];
    MeterData m;
    if (!meters_->get(it.meter, m)) return false;
    a.show = true;
    a.kind = it.kind;
    a.phase = ((now / 500UL) % 2) == 1;
    a.pos = shown_ + 1;
    a.count = n_;
    snprintf(a.sn, sizeof(a.sn), "%lu", (unsigned long)m.id);
    describe(it, m, now, a);
    return true;
  }

  void ackCurrent() {
    if (n_ == 0) return;
    ack(items_[shown_ < n_ ? shown_ : 0]);
    rebuild(millis());
  }

  void ackAll() {
    for (uint8_t i = 0; i < n_; i++) ack(items_[i]);
    rebuild(millis());
  }

  // ---------- для WEB ----------
  const AlarmState& state(uint8_t meter, uint8_t type) const { return st_[meter][type]; }

  // Новая тревога (для уведомлений): true и элемент, пока очередь не пуста
  bool popNewAlarm(AlarmItem& out) {
    if (nqHead_ == nqTail_) return false;
    out = nq_[nqTail_];
    nqTail_ = (nqTail_ + 1) % 8;
    return true;
  }

  // Подавить напоминание «передать показания» на текущий месяц для всех счётчиков (кнопка в вебе)
  bool dismissReminderThisMonth() {
    if (!clk_->valid()) return false;
    time_t l = clk_->local();
    struct tm t;
    gmtime_r(&l, &t);
    uint32_t mk = (uint32_t)(t.tm_year + 1900) * 100UL + (uint32_t)(t.tm_mon + 1);
    for (uint8_t i = 0; i < MAX_METERS; i++) remDone_[i] = mk;
    rebuild(millis());
    SVT_SERIAL.println("REMINDER: suppressed for this month (web)");
    return true;
  }

  void ackAlarm(uint8_t meter, uint8_t type) {
    if (meter < MAX_METERS && type < AL_COUNT && st_[meter][type].active) {
      st_[meter][type].acked = true;
      SVT_SERIAL.printf("ALARM: acknowledged type=%u meter=%u\n", type, meter);
      rebuild(millis());
    }
  }

  void ackAllAlarms() {
    for (uint8_t i = 0; i < MAX_METERS; i++)
      for (uint8_t t = 0; t < AL_COUNT; t++)
        if (st_[i][t].active) st_[i][t].acked = true;
    rebuild(millis());
  }

  // hours = 0: снять подавление
  void suppress(uint8_t type, uint32_t hours) {
    if (type >= AL_COUNT) return;
    if (hours > 999) hours = 999;
    suppUntil_[type] = hours ? millis() + hours * 3600000UL : 0;
    SVT_SERIAL.printf("ALARM: suppress type=%u hours=%lu\n", type, (unsigned long)hours);
    rebuild(millis());
  }

  bool suppressed(uint8_t type, uint32_t now) const {
    return suppUntil_[type] != 0 && (int32_t)(suppUntil_[type] - now) > 0;
  }

  uint32_t suppressLeftMin(uint8_t type) const {
    uint32_t now = millis();
    return suppressed(type, now) ? (suppUntil_[type] - now) / 60000UL : 0;
  }

  // Текст причины для WEB: "15% (порог 20%)"
  void reasonText(uint8_t meter, uint8_t type, char* out, size_t n) {
    MeterData m;
    out[0] = 0;
    if (!meters_->get(meter, m)) return;
    AlarmItem it = {1, meter, type};
    AlertInfo a;
    describe(it, m, millis(), a);
    snprintf(out, n, "%s", a.reasonL);
  }

 private:
  struct FlowTrack { double lastTotal = -1; uint32_t lastChangeMs = 0; };
  struct DayTrack { uint32_t key = 0; double b1 = 0, b2 = 0; };

  void upd(uint8_t i, uint8_t t, bool trig, bool rel, uint32_t now) {
    AlarmState& s = st_[i][t];
    if (!(cfg.mask & (1u << t))) {
      s.active = false;
      s.acked = false;
      return;
    }
    if (!s.active && trig) {
      s.active = true;
      s.acked = false;
      s.sinceMs = now;
      SVT_SERIAL.printf("ALARM: %s meter=%u\n", SVT_ALARM_NAME_LAT[t], i);
      uint8_t nx = (nqHead_ + 1) % 8;
      if (nx != nqTail_) {            // очередь новых тревог для уведомлений
        nq_[nqHead_] = AlarmItem{1, i, t};
        nqHead_ = nx;
      }
    } else if (s.active && rel) {
      s.active = false;
      s.acked = false;
      SVT_SERIAL.printf("ALARM: cleared %s meter=%u\n", SVT_ALARM_NAME_LAT[t], i);
    }
  }

  uint32_t dayKeyNow() {
    if (!clk_->valid()) return 0;
    time_t l = clk_->local();
    struct tm t;
    gmtime_r(&l, &t);
    return (uint32_t)(t.tm_year + 1900) * 10000UL + (uint32_t)(t.tm_mon + 1) * 100UL + (uint32_t)t.tm_mday;
  }

  void evaluate(uint32_t now) {
    uint8_t n = meters_->count();
    uint32_t dayKey = dayKeyNow();
    for (uint8_t i = 0; i < n; i++) {
      MeterData m;
      if (!meters_->get(i, m) || !m.valid) continue;
      uint32_t ageMs = now - m.lastSeenMs;

      upd(i, AL_LOW_BAT, m.battery <= cfg.lowBat, m.battery > cfg.lowBat + 3, now);
      upd(i, AL_HIGH_TEMP, m.hasTemp && m.temp >= cfg.highTemp, !m.hasTemp || m.temp < cfg.highTemp - 2, now);

      uint32_t ndMs = (uint32_t)cfg.noDataH * 3600000UL;
      upd(i, AL_NO_DATA, ageMs > ndMs, ageMs <= ndMs, now);

      // нет расхода: суммарные показания не менялись заданное время (отсчёт от запуска платы)
      if (m.hasT1 && (!m.twoTariff || m.hasT2)) {
        double tot = m.t1 + (m.twoTariff ? m.t2 : 0.0);
        FlowTrack& f = flow_[i];
        if (f.lastTotal < 0 || fabs(tot - f.lastTotal) > 1e-6) {
          f.lastTotal = tot;
          f.lastChangeMs = now;
        }
        uint32_t nfMs = (uint32_t)cfg.noFlowH * 3600000UL;
        upd(i, AL_NO_FLOW, now - f.lastChangeMs > nfMs, now - f.lastChangeMs <= nfMs, now);
      }

      // суточный лимит: расход от первых показаний за сутки (нужно установленное время), только вода и газ
      if (dayKey && (cfg.limit1 || cfg.limit2) && m.type != 4 && m.hasT1 && (!m.twoTariff || m.hasT2)) {
        DayTrack& d = day_[i];
        if (d.key != dayKey) {
          d.key = dayKey;
          d.b1 = m.t1;
          d.b2 = m.t2;
        }
        double c1 = (m.t1 - d.b1) * 1000.0;
        double c2 = m.twoTariff ? (m.t2 - d.b2) * 1000.0 : 0.0;
        bool over = (cfg.limit1 && c1 > cfg.limit1) || (cfg.limit2 && c2 > cfg.limit2);
        upd(i, AL_DAILY_LIMIT, over, !over, now);
      } else {
        upd(i, AL_DAILY_LIMIT, false, true, now);
      }

      upd(i, AL_WEAK_BLE, m.rssi <= cfg.weakDbm, m.rssi > cfg.weakDbm + 3, now);
    }
  }

  // Окно ежемесячного напоминания; monthKey = ГГГГММ
  bool reminderWindow(uint32_t* monthKey) {
    if (!cfg.remEnabled || !clk_->valid()) return false;
    time_t l = clk_->local();
    struct tm t;
    gmtime_r(&l, &t);
    *monthKey = (uint32_t)(t.tm_year + 1900) * 100UL + (uint32_t)(t.tm_mon + 1);
    return t.tm_mday == cfg.remDay && t.tm_hour >= cfg.remFrom && t.tm_hour < cfg.remTo;
  }

  void rebuild(uint32_t now) {
    uint8_t n = 0;
    uint8_t mc = meters_->count();
    for (uint8_t i = 0; i < mc && n < SVT_ALERT_MAX; i++) {
      for (uint8_t t = 0; t < AL_COUNT && n < SVT_ALERT_MAX; t++) {
        const AlarmState& s = st_[i][t];
        if (s.active && !s.acked && !suppressed(t, now)) items_[n++] = AlarmItem{1, i, t};
      }
    }
    uint32_t mk = 0;
    if (n == 0 && reminderWindow(&mk)) {
      monthKey_ = mk;
      for (uint8_t i = 0; i < mc && n < SVT_ALERT_MAX; i++) {
        if (remDone_[i] != mk) items_[n++] = AlarmItem{2, i, 0};
      }
    }
    n_ = n;
  }

  void ack(const AlarmItem& it) {
    if (it.kind == 1) {
      st_[it.meter][it.type].acked = true;
      SVT_SERIAL.printf("ALARM: acknowledged %s meter=%u\n", SVT_ALARM_NAME_LAT[it.type], it.meter);
    } else {
      remDone_[it.meter] = monthKey_;
      SVT_SERIAL.printf("REMINDER: dismissed meter=%u\n", it.meter);
    }
  }

  // Названия и причина: короткие для LCD (до 5 символов) и длинные для TFT (до 10 символов)
  void describe(const AlarmItem& it, const MeterData& m, uint32_t now, AlertInfo& a) {
    if (it.kind == 2) {
      snprintf(a.titleS, sizeof(a.titleS), "SEND");
      snprintf(a.reasonS, sizeof(a.reasonS), "READ");
      snprintf(a.titleL, sizeof(a.titleL), "SEND READ");
      snprintf(a.reasonL, sizeof(a.reasonL), "day %u", cfg.remDay);
      return;
    }
    switch (it.type) {
      case AL_LOW_BAT:
        snprintf(a.titleS, sizeof(a.titleS), "BAT");
        snprintf(a.reasonS, sizeof(a.reasonS), "%u%%", m.battery);
        snprintf(a.titleL, sizeof(a.titleL), "LOW BAT");
        snprintf(a.reasonL, sizeof(a.reasonL), "%u%%<=%u%%", m.battery, cfg.lowBat);
        break;
      case AL_HIGH_TEMP:
        snprintf(a.titleS, sizeof(a.titleS), "TEMP");
        snprintf(a.reasonS, sizeof(a.reasonS), "%dC", (int)lroundf(m.temp));
        snprintf(a.titleL, sizeof(a.titleL), "HIGH TEMP");
        snprintf(a.reasonL, sizeof(a.reasonL), "%.1fC>=%dC", m.temp, cfg.highTemp);
        break;
      case AL_NO_DATA: {
        uint32_t h = (now - m.lastSeenMs) / 3600000UL;
        snprintf(a.titleS, sizeof(a.titleS), "DATA");
        snprintf(a.reasonS, sizeof(a.reasonS), "%luh", (unsigned long)h);
        snprintf(a.titleL, sizeof(a.titleL), "NO DATA");
        snprintf(a.reasonL, sizeof(a.reasonL), "%luh>%uh", (unsigned long)h, cfg.noDataH);
        break;
      }
      case AL_NO_FLOW: {
        uint32_t h = (now - flow_[it.meter].lastChangeMs) / 3600000UL;
        snprintf(a.titleS, sizeof(a.titleS), "FLOW");
        snprintf(a.reasonS, sizeof(a.reasonS), "%luh", (unsigned long)h);
        snprintf(a.titleL, sizeof(a.titleL), "NO FLOW");
        snprintf(a.reasonL, sizeof(a.reasonL), "%luh>%uh", (unsigned long)h, cfg.noFlowH);
        break;
      }
      case AL_DAILY_LIMIT: {
        const DayTrack& d = day_[it.meter];
        bool first = cfg.limit1 && (m.t1 - d.b1) * 1000.0 > cfg.limit1;
        long used = lround((first ? (m.t1 - d.b1) : (m.t2 - d.b2)) * 1000.0);
        long lim = first ? (long)cfg.limit1 : (long)cfg.limit2;
        snprintf(a.titleS, sizeof(a.titleS), "LIM");
        snprintf(a.reasonS, sizeof(a.reasonS), "%ldL", used);
        snprintf(a.titleL, sizeof(a.titleL), "DAILY LIM");
        snprintf(a.reasonL, sizeof(a.reasonL), "%ldL>%ldL", used, lim);
        break;
      }
      default:  // AL_WEAK_BLE
        snprintf(a.titleS, sizeof(a.titleS), "RSSI");
        snprintf(a.reasonS, sizeof(a.reasonS), "%d", m.rssi);
        snprintf(a.titleL, sizeof(a.titleL), "WEAK BLE");
        snprintf(a.reasonL, sizeof(a.reasonL), "%ddBm", m.rssi);
        break;
    }
  }

  MeterManager* meters_ = nullptr;
  TimeManager* clk_ = nullptr;
  AlarmState st_[MAX_METERS][AL_COUNT];
  FlowTrack flow_[MAX_METERS];
  DayTrack day_[MAX_METERS];
  uint32_t suppUntil_[AL_COUNT] = {0, 0, 0, 0, 0, 0};
  uint32_t remDone_[MAX_METERS] = {0};   // месяц (ГГГГММ), в котором напоминание закрыто
  uint32_t monthKey_ = 0;
  AlarmItem items_[SVT_ALERT_MAX];
  AlarmItem nq_[8];
  uint8_t nqHead_ = 0, nqTail_ = 0;
  uint8_t n_ = 0;
  uint8_t shown_ = 0;
  uint32_t lastEval_ = 0;
};
