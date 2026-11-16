// Время: источники (браузер, NTP, вручную), часовой пояс (смещение без перехода на летнее время)
// Часов реального времени нет: после перезагрузки время не установлено, пока его не передаст
// браузер, NTP или пользователь. История пишется только при установленном времени.
#pragma once
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include "monitor_common.h"

#ifndef SVT_TIME
  #define SVT_TIME() time(nullptr)
#endif
#ifndef SVT_SETTIME
  static inline void svtSetTime(time_t e) {
    struct timeval tv;
    tv.tv_sec = e;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
  }
  #define SVT_SETTIME(e) svtSetTime(e)
#endif

#define SVT_TIME_MIN_VALID 1700000000UL   // всё, что раньше (ноябрь 2023), = "время не установлено"

// Дата и время по гражданскому календарю в секунды Unix (без часовых поясов)
static inline int64_t svtCivilToEpoch(int y, int m, int d, int hh, int mm) {
  y -= m <= 2;
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  int64_t days = (int64_t)era * 146097 + (int64_t)doe - 719468;
  return days * 86400 + hh * 3600 + mm * 60;
}

class TimeManager {
 public:
  void begin() {
    Preferences p;
    if (p.begin("svt15time", true)) {
      tzMin_ = p.getShort("tz", 0);
      tzSet_ = p.getBool("tzset", false);
      ntpOn_ = p.getBool("ntp", true);   // по умолчанию время берётся по NTP, когда подключена домашняя сеть
      ntpServer_ = p.getString("ntpsrv", "pool.ntp.org");
      p.end();
    }
    if (tzMin_ < -720 || tzMin_ > 840) tzMin_ = 0;
    if (ntpServer_.length() == 0) ntpServer_ = "pool.ntp.org";
  }

  bool valid() const { return (unsigned long)SVT_TIME() > SVT_TIME_MIN_VALID; }
  time_t utc() const { return SVT_TIME(); }
  time_t local() const { return utc() + (time_t)tzMin_ * 60; }
  int tzMinutes() const { return tzMin_; }
  bool tzConfigured() const { return tzSet_; }
  bool ntpEnabled() const { return ntpOn_; }
  const String& ntpServer() const { return ntpServer_; }
  // Заполнить местное время и дату для экрана
  void fillInfo(ClockInfo& c) const {
    c = ClockInfo();
    if (!valid()) return;
    time_t l = local();
    struct tm t;
    gmtime_r(&l, &t);
    c.valid = true;
    c.year = (uint16_t)(t.tm_year + 1900);
    c.mon = (uint8_t)(t.tm_mon + 1);
    c.day = (uint8_t)t.tm_mday;
    c.hour = (uint8_t)t.tm_hour;
    c.min = (uint8_t)t.tm_min;
    c.sec = (uint8_t)t.tm_sec;
  }

  uint16_t minuteOfDay() const { return (uint16_t)((local() % 86400) / 60); }

  static void fmtDate(time_t localEpoch, char* out) {   // out: 11 байт
    struct tm t;
    gmtime_r(&localEpoch, &t);
    snprintf(out, 11, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  }
  static void fmtDateTime(time_t localEpoch, char* out, size_t n) {
    struct tm t;
    gmtime_r(&localEpoch, &t);
    snprintf(out, n, "%04d-%02d-%02d %02d:%02d:%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);
  }
  static void fmtInput(time_t localEpoch, char* out, size_t n) {   // для datetime-local
    struct tm t;
    gmtime_r(&localEpoch, &t);
    snprintf(out, n, "%04d-%02d-%02dT%02d:%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min);
  }

  void setUtc(time_t e) {
    SVT_SETTIME(e);
    char b[24];
    fmtDateTime(local(), b, sizeof(b));
    SVT_SERIAL.printf("Time set: %s (UTC%+d min)\n", b, tzMin_);
  }

  // markSet = true: пояс задан пользователем (автосинхронизация браузера его не меняет)
  void setTz(int minutes, bool markSet) {
    if (minutes < -720 || minutes > 840) return;
    tzMin_ = minutes;
    if (markSet) tzSet_ = true;
    Preferences p;
    if (p.begin("svt15time", false)) {
      p.putShort("tz", (int16_t)tzMin_);
      p.putBool("tzset", tzSet_);
      p.end();
    }
  }

  void setNtp(bool on, const String& server) {
    ntpOn_ = on;
    if (server.length() > 0) ntpServer_ = server;
    ntpStarted_ = false;
    Preferences p;
    if (p.begin("svt15time", false)) {
      p.putBool("ntp", ntpOn_);
      p.putString("ntpsrv", ntpServer_);
      p.end();
    }
  }

  // Время из браузера: обновляется, если время не установлено или расходится больше чем на 60 минут
  // (force: кнопка "Синхронизировать с браузером", пояс тоже берётся из браузера)
  bool fromBrowser(time_t epoch, int tzMin, bool force) {
    if (!force && valid() && llabs((long long)epoch - (long long)utc()) <= 3600) return false;
    setUtc(epoch);
    if (force) setTz(tzMin, true);
    else if (!tzSet_) setTz(tzMin, false);
    return true;
  }

  // Из loop(): NTP запускается, когда есть домашняя сеть
  void loop(bool staConnected) {
    if (ntpOn_ && staConnected && !ntpStarted_) startNtp();
    bool v = valid();
    if (v && !wasValid_) {
      wasValid_ = true;
      char b[24];
      fmtDateTime(local(), b, sizeof(b));
      SVT_SERIAL.printf("Time valid: %s\n", b);
    } else if (!v) {
      wasValid_ = false;
    }
  }

  // Синхронизация по NTP сейчас; false, если домашней сети нет
  bool syncNtpNow(bool staConnected) {
    if (!staConnected) return false;
    startNtp();
    return true;
  }

 private:
  void startNtp() {
    configTime(0, 0, ntpServer_.c_str());   // пояс применяем сами
    ntpStarted_ = true;
    SVT_SERIAL.printf("NTP: started, server %s\n", ntpServer_.c_str());
  }

  int tzMin_ = 0;
  bool tzSet_ = false;
  bool ntpOn_ = true;
  String ntpServer_ = "pool.ntp.org";
  bool ntpStarted_ = false;
  bool wasValid_ = false;
};
