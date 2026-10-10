#pragma once
#include <Arduino.h>
#include "config.h"
#include "protocol_parser.h"
#include "storage_manager.h"

enum MeterFlag : uint16_t {
  METER_ACTIVE=1<<0, METER_COLLECT=1<<1, METER_HISTORY=1<<2, METER_LCD=1<<3,
  METER_WEB=1<<4, METER_TELEGRAM=1<<5, METER_TOTALS=1<<6, METER_CALCULATOR=1<<7, METER_ALARMS=1<<8
};

class MeterManager {
public:
  bool begin();
  void observe(const MeterPacket& packet);
  void process(uint32_t nowMillis, time_t nowEpoch);
  uint8_t count() const { return count_; }
  const MeterConfig* configAt(uint8_t i) const;
  const MeterPacket* latestAt(uint8_t i) const;
  bool setFlagAt(uint8_t i, uint16_t flag, bool enabled);
  StorageManager& storage() { return storage_; }
private:
  struct Slot { MeterConfig config; MeterPacket latest; bool seen=false; };
  Slot slots_[MAX_METERS];
  uint8_t count_=0;
  bool dirty_=false;
  uint32_t lastSaveAt_=0;
  StorageManager storage_;
  int findBySerial(uint32_t serial) const;
};
