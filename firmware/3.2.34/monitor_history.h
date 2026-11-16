// История показаний в LittleFS: одна запись на счётчик в сутки, срок хранения настраивается.
// Файл /history.csv, строки: дата,SN,тип,тариф1,тариф2,температура,батарея,RSSI
// Раздел LittleFS берётся из схемы разделов (Arduino IDE: Tools -> Partition Scheme, например "Default 4MB with spiffs").
#pragma once
#include <LittleFS.h>
#include <Preferences.h>
#include "monitor_common.h"
#include "monitor_meters.h"
#include "monitor_time.h"

#define SVT_HIST_FILE "/history.csv"
#define SVT_HIST_TMP  "/history.tmp"
#ifndef SVT_HIST_MINUTE
  #define SVT_HIST_MINUTE 1435          // запись 23:55 по местному времени
#endif
#define SVT_HIST_MIN_FREE 20000UL       // меньше свободного места: запись не делаем

class HistoryManager {
 public:
  void begin(TimeManager* clk, MeterManager* meters) {
    clk_ = clk;
    meters_ = meters;
    ok_ = LittleFS.begin(true);          // true: отформатировать при первом запуске
    Preferences p;
    if (p.begin("svt15hist", true)) {
      days_ = p.getUShort("days", SVT_HISTORY_DAYS);
      lastDate_ = p.getUInt("last", 0);
      periodic_ = p.getBool("per", true);
      ivlH_ = p.getUChar("ivl", 2);
      p.end();
    }
    if (days_ < 1 || days_ > 365) days_ = SVT_HISTORY_DAYS;
    if (ivlH_ < 1 || ivlH_ > 12) ivlH_ = 2;
    SVT_SERIAL.printf("History: LittleFS %s, keep %u days\n", ok_ ? "ok" : "FAILED", (unsigned)days_);
  }

  bool ok() const { return ok_; }
  bool periodicOn() const { return periodic_; }
  uint8_t interval() const { return ivlH_; }
  uint16_t days() const { return days_; }
  size_t total() { return ok_ ? LittleFS.totalBytes() : 0; }
  size_t used() { return ok_ ? LittleFS.usedBytes() : 0; }

  void setDays(uint16_t d) {
    if (d < 1 || d > 365) return;
    days_ = d;
    Preferences p;
    if (p.begin("svt15hist", false)) {
      p.putUShort("days", days_);
      p.end();
    }
    trimOld();
  }

  // Настройка периодических записей (через веб); сохраняется в памяти
  void setPeriodic(bool on, uint8_t hours) {
    if (hours < 1 || hours > 12) return;
    periodic_ = on;
    ivlH_ = hours;
    Preferences p;
    if (p.begin("svt15hist", false)) {
      p.putBool("per", periodic_);
      p.putUChar("ivl", ivlH_);
      p.end();
    }
  }

  // Из loop(): раз в сутки в SVT_HIST_MINUTE
  void loop() {
    if (!ok_ || !clk_->valid()) return;
    uint32_t now = millis();
    if (now - lastCheck_ < 10000UL) return;
    lastCheck_ = now;
    char today[11];
    TimeManager::fmtDate(clk_->local(), today);
    uint32_t key = dateKey(today);
    if (lastDate_ != 0 && key > lastDate_) catchUp(today);   // пропущенные дни (плата была выключена)
    // периодические записи: в начале каждого интервала (часы кратны интервалу), в первые 15 минут
    if (periodic_) {
      uint32_t hour = clk_->minuteOfDay() / 60, minute = clk_->minuteOfDay() % 60;
      if (hour % ivlH_ == 0 && minute < 15) {
        uint32_t slot = key * 100 + hour;
        if (slot != lastSlot_) {
          char hhmm[6];
          snprintf(hhmm, sizeof(hhmm), "%02u:00", (unsigned)hour);
          if (writeSnapshot(today, false, hhmm)) lastSlot_ = slot;
        }
      }
    }
    if (key == lastDate_ || clk_->minuteOfDay() < SVT_HIST_MINUTE) return;
    if (writeSnapshot(today)) {
      lastDate_ = key;
      Preferences p;
      if (p.begin("svt15hist", false)) {
        p.putUInt("last", lastDate_);
        p.end();
      }
      trimOld();
    }
  }

  // Записать текущие показания всех счётчиков с датой date (YYYY-MM-DD)
  // Дни между последней записью и сегодня: пишем последние известные показания (возраст 999)
  void catchUp(const char* today) {
    uint32_t tk = dateKey(today);
    time_t todayStart = svtCivilToEpoch(tk / 10000, (tk / 100) % 100, tk % 100, 0, 0);
    time_t t = svtCivilToEpoch(lastDate_ / 10000, (lastDate_ / 100) % 100, lastDate_ % 100, 0, 0) + 86400;
    time_t floorT = todayStart - (time_t)days_ * 86400;
    if (t < floorT) t = floorT;                 // старше срока хранения не пишем
    bool any = false;
    for (int guard = 0; t < todayStart && guard < 400; guard++, t += 86400) {
      struct tm tmv;
      gmtime_r(&t, &tmv);
      char ds[11];
      snprintf(ds, sizeof(ds), "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
      if (!writeSnapshot(ds, true)) break;
      lastDate_ = dateKey(ds);
      any = true;
    }
    if (any) {
      Preferences p;
      if (p.begin("svt15hist", false)) {
        p.putUInt("last", lastDate_);
        p.end();
      }
    }
  }

  bool writeSnapshot(const char* date, bool catchup = false, const char* hhmm = "23:55") {
    if (!ok_) return false;
    if (LittleFS.totalBytes() - LittleFS.usedBytes() < SVT_HIST_MIN_FREE) {
      if (!warned_) {
        warned_ = true;
        SVT_SERIAL.println("History: not enough free space, record skipped");
      }
      return false;
    }
    File f = LittleFS.open(SVT_HIST_FILE, FILE_APPEND);
    if (!f) return false;
    uint8_t n = meters_->count();
    int written = 0;
    for (uint8_t i = 0; i < n; i++) {
      MeterData m;
      if (!meters_->get(i, m) || !m.valid) continue;
      char t1[24] = "", t2[24] = "", tp[12] = "", line[128];
      if (m.hasT1) snprintf(t1, sizeof(t1), "%.*f", m.dec, m.t1);
      if (m.twoTariff && m.hasT2) snprintf(t2, sizeof(t2), "%.*f", m.dec, m.t2);
      if (m.hasTemp) snprintf(tp, sizeof(tp), "%.1f", m.temp);
      // возраст последнего пакета, часов (999: записано из последних известных показаний)
      uint32_t ageH = catchup ? 999UL : (uint32_t)((millis() - m.lastSeenMs) / 3600000UL);
      if (ageH > 999) ageH = 999;
      snprintf(line, sizeof(line), "%s,%lu,%u,%s,%s,%s,%u,%d,%lu,%s\n", date, (unsigned long)m.id, m.type,
               t1, t2, tp, m.battery, m.rssi, (unsigned long)ageH, hhmm);
      f.print(line);
      written++;
    }
    f.close();
    warned_ = false;
    SVT_SERIAL.printf("History: %s, %d records\n", date, written);
    return true;
  }

  // Удалить записи с датой раньше cutoff (YYYY-MM-DD); потоково, без загрузки файла в память
  bool trimBefore(const char* cutoff) {
    if (!ok_ || !LittleFS.exists(SVT_HIST_FILE)) return true;
    File in = LittleFS.open(SVT_HIST_FILE, FILE_READ);
    if (!in) return false;
    File out = LittleFS.open(SVT_HIST_TMP, FILE_WRITE);
    if (!out) { in.close(); return false; }
    char line[128];
    while (in.available()) {
      size_t n = in.readBytesUntil('\n', line, sizeof(line) - 2);
      if (n < 12) continue;
      line[n] = 0;
      if (strncmp(line, cutoff, 10) >= 0) {
        out.print(line);
        out.print("\n");
      }
    }
    in.close();
    out.close();
    LittleFS.remove(SVT_HIST_FILE);
    LittleFS.rename(SVT_HIST_TMP, SVT_HIST_FILE);
    return true;
  }

  // Удалить всё старше срока хранения
  void trimOld() {
    if (!ok_ || !clk_->valid()) return;
    char cutoff[11];
    TimeManager::fmtDate(clk_->local() - (time_t)days_ * 86400, cutoff);
    trimBefore(cutoff);
  }

  // Строки файла в диапазоне дат [from, to] и (если sn >= 0) только этого счётчика
  void csvFilter(const char* from, const char* to, long sn,
                 void (*out)(void*, const char*, size_t), void* ctx) {
    if (!ok_) return;
    File f = LittleFS.open(SVT_HIST_FILE, FILE_READ);
    if (!f) return;
    char line[128];
    char chunk[640];
    size_t cl = 0;
    while (f.available()) {
      size_t n = f.readBytesUntil('\n', line, sizeof(line) - 2);
      if (n < 12) continue;
      line[n] = 0;
      if (strncmp(line, from, 10) < 0 || strncmp(line, to, 10) > 0) continue;
      if (sn >= 0 && strtol(line + 11, nullptr, 10) != sn) continue;
      line[n++] = '\n';
      if (cl + n > sizeof(chunk)) { out(ctx, chunk, cl); cl = 0; }
      memcpy(chunk + cl, line, n);
      cl += n;
    }
    if (cl) out(ctx, chunk, cl);
    f.close();
  }

  static uint32_t dateKey(const char* d) {   // "2026-10-11" -> 20261011
    return (uint32_t)atoi(d) * 10000UL + (uint32_t)atoi(d + 5) * 100UL + (uint32_t)atoi(d + 8);
  }

 private:
  TimeManager* clk_ = nullptr;
  MeterManager* meters_ = nullptr;
  bool ok_ = false;
  bool warned_ = false;
  uint16_t days_ = 30;
  uint32_t lastDate_ = 0;
  bool periodic_ = true;          // периодические записи вкл/выкл
  uint8_t ivlH_ = 2;              // интервал, часов
  uint32_t lastSlot_ = 0;         // последний записанный интервал (дата*100 + час)
  uint32_t lastCheck_ = 0;
};
