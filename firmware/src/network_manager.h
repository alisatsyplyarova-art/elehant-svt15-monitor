#pragma once
#include <Arduino.h>
#include "meter_manager.h"

void networkBegin(MeterManager& meters);
void networkLoop();
void networkRestartAp();
bool networkAlarmActive(uint32_t nowMillis);
