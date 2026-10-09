#pragma once
#include <Arduino.h>
#include "meter_manager.h"

void displayBegin();
void displayRender(const MeterManager& meters, uint32_t nowMillis);
void displayWake();
void displaySleep();
