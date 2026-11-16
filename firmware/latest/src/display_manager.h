#pragma once
#include <Arduino.h>
#include "meter_manager.h"

void displayBegin();
void displayRender(const MeterManager& meters, uint32_t nowMillis, uint8_t selectedMeter, bool configMode, uint8_t selectedFlag);
// Returns true when the event only wakes a sleeping display and should be consumed.
bool displayButtonActivity(uint32_t nowMillis);
void displaySetAlarm(bool active);
void displayService(uint32_t nowMillis);
void displayWake();
void displaySleep();
