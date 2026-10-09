#pragma once
#include <Arduino.h>
#include "meter_manager.h"

void displayBegin();
void displayRender(const MeterManager& meters, uint32_t nowMillis, uint8_t selectedMeter, bool configMode, uint8_t selectedFlag);
void displayWake();
void displaySleep();
