// Общие определения: данные счётчика, пороги, статус, вывод в Serial
#pragma once
#include <Arduino.h>
#include "monitor_boards.h"    // плата: пины, память, SVT_SERIAL
#include "monitor_profile.h"   // общие настройки

#define SVT15_LCD_VERSION "3.2.33-cl"

// Порт вывода SVT_SERIAL задаётся в monitor_boards.h

// 1 — печатать в Serial0 сырые пакеты (строки RAW ...) для отладки
#define SVT_DEBUG 1

#define LOW_BAT_PCT    20
#define WEAK_BLE_DBM   (-85)
#define NO_DATA_MS     (72UL * 3600UL * 1000UL)

struct MeterData {
  uint32_t id = 0;
  uint8_t  type = 2;           // 1 газ, 2 вода, 4 тепло
  bool     hasTemp = true;     // у газовых счётчиков температуры нет
  bool     valid = false;      // были ли принятые пакеты
  bool     twoTariff = false;  // двухтарифный: T1 = горячая, T2 = холодная
  double   t1 = 0;             // м3 (однотарифный: общий)
  double   t2 = 0;             // м3
  bool     hasT1 = false;      // значение T1 уже принято
  bool     hasT2 = false;      // значение T2 уже принято
  uint8_t  dec = 3;            // знаков после запятой (версия пакета 1: 4, версия 5: 3)
  float    temp = 0;           // °C
  uint8_t  battery = 0;        // %
  float    fw = 0;             // версия прошивки счётчика (data[16] / 10)
  int8_t   rssi = 0;           // dBm
  uint8_t  seq = 0;            // байт 3 пакета (без FFFF): счётчик пакетов, только версия 1
  uint8_t  seqStream = 0;      // поток: 0 — тариф 1 (модель 4), 1 — тариф 2 (модель 3)
  uint8_t  seqValid = 0;       // пакет несёт счётчик последовательности
  uint8_t  seqLast[2] = {0, 0};
  uint8_t  seqHave[2] = {0, 0};
  uint32_t rx = 0;             // принято пакетов
  uint32_t lost = 0;           // пропущено пакетов по счётчику последовательности
  uint32_t lastSeenMs = 0;     // millis() последнего пакета
  // для отладки
  uint8_t  model = 0, version = 0, flags = 0;
  uint32_t rawOwn = 0, rawPaired = 0;
};

enum Svt15Level { SVT_OK = 0, SVT_WARN = 1, SVT_BAD = 2 };

// Состояние домашнего Wi-Fi для экрана
struct WifiInfo {
  bool enabled = false;     // домашняя сеть включена и SSID задан
  bool connected = false;
  char ssid[33] = "";
  char ip[16] = "";
  int8_t rssi = 0;
  char state[10] = "";      // wait, no net, bad pass, refused, lost, fail
};

// Перезагрузка платы (в тестах подменяется)
#ifndef SVT_RESTART
  #define SVT_RESTART() ESP.restart()
#endif

// Страница сервисного меню для экрана
struct MenuView {
  char title[16];
  char line[8][28];
  uint8_t n;          // число строк
  int8_t sel;         // выбранная строка (-1: без выбора)
  MenuView() : n(0), sel(-1) { memset(title, 0, sizeof(title)); memset(line, 0, sizeof(line)); }
};

// Местное время и дата для экрана (valid = false: время не установлено)
struct ClockInfo {
  bool valid = false;
  uint16_t year = 0;
  uint8_t mon = 0, day = 0, hour = 0, min = 0, sec = 0;
};

// Тревога или напоминание для мигающей рамки в правом верхнем углу экрана
struct AlertInfo {
  bool show = false;
  uint8_t kind = 0;          // 1 тревога, 2 напоминание
  bool phase = false;        // фаза мигания (меняется каждые 0,5 с)
  uint8_t pos = 0, count = 0;
  char sn[12] = "";
  char titleS[8] = "";       // название для LCD (до 5 символов)
  char reasonS[8] = "";      // причина для LCD (до 5 символов)
  char titleL[16] = "";      // название для TFT (до 10 символов)
  char reasonL[16] = "";     // причина для TFT (до 10 символов)
};

// Строка Wi-Fi для экрана с ограниченной шириной: при подключении название сети и IP
// сменяют друг друга каждые 3 секунды, иначе "WiFi: <причина>".
// Возвращает false, если домашняя сеть выключена (строку выводить не нужно).
static inline bool svt15WifiLine(const WifiInfo& w, char* out, size_t n, uint8_t maxLen,
                                 uint32_t nowMs, Svt15Level* lvl) {
  if (!w.enabled) return false;
  if (w.connected) {
    const char* src = ((nowMs / 3000UL) % 2 == 0) ? w.ssid : w.ip;
    snprintf(out, n, "%.*s", (int)maxLen, src);
    if (lvl) *lvl = SVT_OK;
  } else {
    snprintf(out, n, "WiFi: %s", w.state[0] ? w.state : "wait");
    if (lvl) *lvl = SVT_WARN;
  }
  if (strlen(out) > maxLen) out[maxLen] = 0;
  return true;
}

// Объём для экрана: число или "---", если этот тариф ещё не принят
// maxLen > 0: при слишком длинной записи уменьшается число знаков после запятой
static inline void svt15FmtVol(char* out, size_t n, bool has, double v, uint8_t dec, uint8_t maxLen = 0) {
  if (!has) { snprintf(out, n, "---"); return; }
  snprintf(out, n, "%.*f", dec, v);
  while (maxLen && strlen(out) > maxLen && dec > 0) {
    dec--;
    snprintf(out, n, "%.*f", dec, v);
  }
}

// Уровень сигнала для экрана: 0..4 по порогам Wi-Fi (-67, -70, -80, -90 dBm)
static inline int svt15SignalBars(int rssi) {
  if (rssi > -67) return 4;
  if (rssi > -70) return 3;
  if (rssi > -80) return 2;
  if (rssi > -90) return 1;
  return 0;
}

// Качество приёма в процентах по счётчику последовательности (-1: данных ещё нет)
static inline int svt15Quality(const MeterData& m) {
  uint32_t tot = m.rx + m.lost;
  if (tot == 0) return -1;
  return (int)((uint64_t)m.rx * 100 / tot);
}

// Статус: NO DATA (72 ч) / LOW BAT (<=20%) / WEAK BLE (<=-85 dBm) / OK
static inline const char* svt15Status(const MeterData& m, Svt15Level* lvl) {
  uint32_t age = millis() - m.lastSeenMs;
  Svt15Level l = SVT_OK;
  const char* s = "OK";
  if (age > NO_DATA_MS)                { l = SVT_BAD;  s = "NO DATA"; }
  else if (m.battery <= LOW_BAT_PCT)   { l = SVT_BAD;  s = "LOW BAT"; }
  else if (m.rssi <= WEAK_BLE_DBM)     { l = SVT_WARN; s = "WEAK BLE"; }
  if (lvl) *lvl = l;
  return s;
}

// Возраст последнего пакета: 5m / 3h / 4d
static inline void svt15AgeStr(const MeterData& m, char* out, size_t n) {
  uint32_t mins = (millis() - m.lastSeenMs) / 60000UL;
  if (mins < 60)        snprintf(out, n, "%lum", (unsigned long)mins);
  else if (mins < 2880) snprintf(out, n, "%luh", (unsigned long)(mins / 60));
  else                  snprintf(out, n, "%lud", (unsigned long)(mins / 1440));
}

// Тип счётчика: буква для экрана и единица измерения
static inline char svt15TypeChar(uint8_t t) {
  return t == 1 ? 'G' : (t == 2 ? 'W' : (t == 4 ? 'H' : '?'));
}
static inline const char* svt15Unit(uint8_t t) { return t == 4 ? "GJ" : "m3"; }

// Вывод тех же данных в Serial0 (115200)
static inline void svt15PrintSerial(const MeterData& m, uint8_t index, uint8_t count) {
  SVT_SERIAL.printf("[%u/%u] %c%lu", index + 1, count, svt15TypeChar(m.type), (unsigned long)m.id);
  if (!m.valid) { SVT_SERIAL.println(" NO DATA"); return; }
  char v1[24], v2[24];
  svt15FmtVol(v1, sizeof(v1), m.hasT1, m.t1, m.dec);
  svt15FmtVol(v2, sizeof(v2), m.hasT2, m.t2, m.dec);
  if (m.twoTariff) { SVT_SERIAL.printf(" T1=%s T2=%s", v1, v2); if (m.hasT1 && m.hasT2) SVT_SERIAL.printf(" TOTAL=%.*f", m.dec, m.t1 + m.t2); SVT_SERIAL.printf(" %s", svt15Unit(m.type)); }
  else             SVT_SERIAL.printf(" T1=%s %s", v1, svt15Unit(m.type));
  if (m.hasTemp)   SVT_SERIAL.printf(" T=%.1fC", m.temp);
  SVT_SERIAL.printf(" BAT=%u%% FW=%.1f RSSI=%d age=%lus\n",
                    m.battery, m.fw, m.rssi,
                    (unsigned long)((millis() - m.lastSeenMs) / 1000UL));
}

// Сырой пакет: тип модели, версия, флаги, два счётчика из пакета и HEX производителя
static inline void svt15PrintRaw(const MeterData& m, const uint8_t* mfg, int len) {
  SVT_SERIAL.printf("RAW id=%lu model=%u ver=%u flags=0x%02X own=%lu paired=%lu rssi=%d len=%d hex=",
                    (unsigned long)m.id, m.model, m.version, m.flags,
                    (unsigned long)m.rawOwn, (unsigned long)m.rawPaired, m.rssi, len);
  for (int i = 0; i < len; i++) SVT_SERIAL.printf("%02X ", mfg[i]);
  SVT_SERIAL.println();
}
