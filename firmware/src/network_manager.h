#pragma once
#include <Arduino.h>
#include "meter_manager.h"

void networkBegin(MeterManager& meters);
void networkLoop();
void networkRestartAp();
