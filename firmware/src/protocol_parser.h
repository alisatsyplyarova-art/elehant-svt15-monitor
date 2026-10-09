#pragma once
#include <Arduino.h>

struct MeasurementField {
  char key[28]{};
  char unit[12]{};
  float value=0;
};

struct MeterPacket {
  uint8_t type=0, model=0, version=0;
  uint32_t serial=0;
  String mac;
  int rssi=0;
  bool hasReading=false, hasReading2=false, hasBattery=false, temperatureValid=false;
  bool hasHeatCarrier=false, heatTemperaturesValid=false;
  float heatCarrierVolume=0, inletTemperatureC=0, outletTemperatureC=0;
  float reading=0, reading2=0, temperatureC=0;
  uint8_t batteryPercent=0;
  String unit;
  MeasurementField fields[12]{};
  uint8_t fieldCount=0;
};

bool parseElehantPacket(const uint8_t* data, size_t length, const char* mac, int rssi, MeterPacket& out);
