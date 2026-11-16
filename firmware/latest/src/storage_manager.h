#pragma once
#include <Arduino.h>
#include <time.h>
#include "protocol_parser.h"

struct MeterConfig {
  uint32_t serial=0;
  uint8_t type=0, model=0;
  char mac[18]{};
  uint16_t flags=0x01FF; // active, collect, history, LCD, web, Telegram, totals, calculator, alarms
  int32_t lastHistoryDay=-1;
};

class StorageManager {
public:
  bool begin();
  bool loadMeters(MeterConfig* meters, uint8_t capacity, uint8_t& count);
  bool saveMeters(const MeterConfig* meters, uint8_t count);
  bool appendHistory(const MeterPacket& packet, time_t timestamp);
  void pruneHistory(time_t now, uint16_t retentionDays);
  uint64_t filesystemTotal() const;
  uint64_t filesystemUsed() const;
  bool isReady() const { return ready_; }
private:
  bool ready_=false;
};
