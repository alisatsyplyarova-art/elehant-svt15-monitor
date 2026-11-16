// Декодер BLE-пакета Элехант (газ 1, вода 2, тепло 4) строго по protocol.md
#pragma once
#include <Arduino.h>
#include "monitor_common.h"

static inline uint32_t svtU32LE(const uint8_t* d, int p) {
  return ((uint32_t)d[p]) | ((uint32_t)d[p + 1] << 8) |
         ((uint32_t)d[p + 2] << 16) | ((uint32_t)d[p + 3] << 24);
}

static inline uint16_t svtU16LE(const uint8_t* d, int p) {
  return ((uint16_t)d[p]) | ((uint16_t)d[p + 1] << 8);
}

// Тариф модели воды: 0 — однотарифная, 1 или 2 — тариф двухтарифного счётчика,
// -1 — модель не из таблицы
static inline int svtWaterTariff(uint8_t model) {
  switch (model) {
    case 1: case 2: case 7: case 8: case 13: case 14:  return 0;  // СВД, СВДУ, SWTU
    case 4: case 6: case 10: case 12: case 16: case 18: return 1;  // СВТ, СВТУ, TWM, тариф 1
    case 3: case 5: case 9: case 11: case 15: case 17:  return 2;  // то же, тариф 2
    default: return -1;
  }
}

// Газовые счётчики: модели из таблицы protocol.md
static inline bool svtGasModel(uint8_t m) {
  return (m >= 1 && m <= 5) || (m >= 16 && m <= 20) || (m >= 32 && m <= 36) ||
         (m >= 48 && m <= 52) || (m >= 64 && m <= 68) || (m >= 80 && m <= 84);
}

// Тариф модели по типу: 0 — одиночный счётчик, 1/2 — тариф двухтарифной воды, -1 — не из таблицы
static inline int svtTariff(uint8_t type, uint8_t model) {
  if (type == 2) return svtWaterTariff(model);
  if (type == 1) return svtGasModel(model) ? 0 : -1;
  if (type == 4) return model == 1 ? 0 : -1;   // СТЭ
  return -1;
}

// Заряд: raw < 1 или raw > limit → 1 %, raw > 100 → 100 %, иначе raw
// limit: 171 для версии 1, 127 для версии 5
static inline uint8_t svtBattery(uint8_t raw, uint8_t limit) {
  if (raw < 1 || raw > limit) return 1;
  if (raw > 100) return 100;
  return raw;
}

// true — пакет воды версии 1 или 5, out заполнен
// (rssi и lastSeenMs заполняет вызывающий код)
static inline bool decodeSVT15(const uint8_t* data, int len, MeterData& out) {
  // Manufacturer Data: ID производителя FFFF + 17 байт данных
  if (len == 19 && data[0] == 0xFF && data[1] == 0xFF) {
    data += 2;
    len -= 2;
  }
  if (len != 17) return false;

  uint8_t flags     = data[0];
  uint8_t version   = data[3];
  uint8_t meterType = data[4];
  uint8_t model     = data[5];

  if (!(flags & 0x80)) return false;   // недействительный пакет
  if (meterType != 1 && meterType != 2 && meterType != 4) return false;  // газ, вода, тепло
  int tariff = svtTariff(meterType, model);
  if (tariff < 0) return false;

  uint32_t serial = ((uint32_t)data[6]) | ((uint32_t)data[7] << 8) |
                    ((uint32_t)data[8] << 16);
  if (serial == 0) return false;

  if (version == 1) {
    // Показания: u32 (байты 9-12) в десятитысячных, флаг 0x04 добавляет разряд из байта 2
    double raw = svtU32LE(data, 9);
    bool hiRes = flags & 0x04;
    if (hiRes) raw += (data[2] & 0x0F) / 10.0;
    double vol = raw / 10000.0;

    out.id = serial;
    out.type = meterType;
    out.hasTemp = (meterType != 1);   // у газовых счётчиков в поле температуры мусор
    out.model = model; out.version = version; out.flags = flags;
    out.rawOwn = svtU32LE(data, 9); out.rawPaired = 0;
    out.seq = data[1];                         // счётчик пакетов (протокол: байт 1 версии 1)
    out.seqValid = 1;
    out.seqStream = (tariff == 2) ? 1 : 0;
    out.dec = hiRes ? 5 : 4;
    out.twoTariff = (tariff != 0);
    // Двухтарифный счётчик: каждый тариф вещается отдельным пакетом (своя модель)
    if (tariff == 2) { out.t2 = vol; out.hasT2 = true; }
    else             { out.t1 = vol; out.hasT1 = true; }
    out.battery = svtBattery(data[13], 171);
    out.temp = (int16_t)svtU16LE(data, 14) / 100.0f;
    out.fw = data[16] / 10.0f;
    out.valid = true;
    return true;
  }

  if (version == 5) {
    if (meterType != 2) return false;  // версия 5 — только двухтарифная вода
    uint32_t first = ((uint32_t)(data[9] >> 4) << 24) | ((uint32_t)data[10] << 16) |
                     ((uint32_t)data[11] << 8) | ((uint32_t)data[12]);
    uint32_t second = ((uint32_t)(data[9] & 0x0F) << 24) | ((uint32_t)data[13] << 16) |
                      ((uint32_t)data[14] << 8) | ((uint32_t)data[15]);
    double firstVol = first / 1000.0;
    double secondVol = second / 1000.0;

    out.id = serial;
    out.type = meterType;
    out.hasTemp = true;
    out.model = model; out.version = version; out.flags = flags;
    out.rawOwn = first; out.rawPaired = second;
    out.dec = 3;
    // Первое показание принадлежит модели из заголовка, второе — парной
    if (tariff == 1) {
      out.twoTariff = true;
      out.t1 = firstVol;  out.hasT1 = true;
      out.t2 = secondVol; out.hasT2 = true;
    } else if (tariff == 2) {
      out.twoTariff = true;
      out.t2 = firstVol;  out.hasT2 = true;
      out.t1 = secondVol; out.hasT1 = true;
    } else {
      out.twoTariff = false;
      out.t1 = firstVol;  out.hasT1 = true;
    }
    out.battery = svtBattery(data[1], 127);
    out.temp = (float)data[2];
    out.fw = data[16] / 10.0f;
    out.valid = true;
    return true;
  }

  return false;  // версии 8, 9 и прочие не являются текущими показаниями
}
