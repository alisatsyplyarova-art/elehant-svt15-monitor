#pragma once
#include <Arduino.h>
#include "meter_manager.h"

struct AlarmSettings {
  uint8_t batteryLow=20;
  uint16_t staleMinutes=15;
  bool lowEnabled=false;
  bool highEnabled=false;
  float lowValue=0.0f;
  float highValue=0.0f;
};

void alarmSettingsLoad();
AlarmSettings alarmSettingsGet();
bool alarmSettingsSave(const AlarmSettings& settings);
bool alarmAnyActive(const MeterManager& meters, uint32_t nowMillis, const AlarmSettings& settings);
