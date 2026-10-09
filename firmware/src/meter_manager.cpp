#include "meter_manager.h"
#include <cstring>
#include <time.h>

bool MeterManager::begin() {
  storage_.begin();
  MeterConfig saved[MAX_METERS];
  uint8_t savedCount=0;
  if (!storage_.loadMeters(saved,MAX_METERS,savedCount)) return false;
  count_=savedCount;
  for (uint8_t i=0;i<count_;++i) slots_[i].config=saved[i];
  return true;
}
int MeterManager::findBySerial(uint32_t serial) const {
  for (uint8_t i=0;i<count_;++i) if (slots_[i].config.serial==serial) return i;
  return -1;
}
void MeterManager::observe(const MeterPacket& packet) {
  if (packet.type==7 || !packet.serial) return;
  int idx=findBySerial(packet.serial);
  if (idx<0) {
    if (count_>=MAX_METERS) return;
    idx=count_++;
    MeterConfig& cfg=slots_[idx].config;
    cfg=MeterConfig{};
    cfg.serial=packet.serial; cfg.type=packet.type; cfg.model=packet.model;
    snprintf(cfg.mac,sizeof(cfg.mac),"%s",packet.mac.c_str());
    cfg.flags=0x01FF;
    dirty_=true;
    Serial.printf("New meter discovered: SN=%lu type=%u model=%u (%u/%u)\n",
      (unsigned long)cfg.serial,cfg.type,cfg.model,count_,MAX_METERS);
  }
  Slot& slot=slots_[idx];
  slot.latest=packet;
  slot.seen=true;
  if (strncmp(slot.config.mac,packet.mac.c_str(),sizeof(slot.config.mac))!=0) {
    snprintf(slot.config.mac,sizeof(slot.config.mac),"%s",packet.mac.c_str());
    dirty_=true;
  }
}
void MeterManager::process(uint32_t nowMillis, time_t nowEpoch) {
  if (dirty_ && nowMillis-lastSaveAt_>=2000) {
    MeterConfig configs[MAX_METERS];
    for (uint8_t i=0;i<count_;++i) configs[i]=slots_[i].config;
    if (storage_.saveMeters(configs,count_)) dirty_=false;
    lastSaveAt_=nowMillis;
  }
  if (nowEpoch<1760000000) return; // No valid clock yet: do not write misleading timestamps.
  struct tm local{};
  localtime_r(&nowEpoch,&local);
  const int32_t day=(int32_t)(local.tm_year*400+local.tm_yday);
  for (uint8_t i=0;i<count_;++i) {
    Slot& slot=slots_[i];
    if (!slot.seen || !(slot.config.flags&METER_ACTIVE) || !(slot.config.flags&METER_COLLECT) ||
        !(slot.config.flags&METER_HISTORY) || slot.latest.type==7 || slot.config.lastHistoryDay==day) continue;
    if (storage_.appendHistory(slot.latest,nowEpoch)) {
      slot.config.lastHistoryDay=day;
      dirty_=true;
    }
  }
}
const MeterConfig* MeterManager::configAt(uint8_t i) const { return i<count_?&slots_[i].config:nullptr; }
const MeterPacket* MeterManager::latestAt(uint8_t i) const { return i<count_ && slots_[i].seen?&slots_[i].latest:nullptr; }
