#include <Arduino.h>
#include <NimBLEDevice.h>
#include "ble_scanner.h"
#include "protocol_parser.h"

static NimBLEScan* scanner=nullptr;

class RawCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    std::string mfg=d->getManufacturerData();
    if (mfg.size() < 2) return;
    // Manufacturer data is expected to contain the 0xFFFF company ID followed
    // by the 17-byte protocol payload. Reject unexpected lengths in the parser.
    const uint8_t* data=reinterpret_cast<const uint8_t*>(mfg.data());
    size_t len=mfg.size();
    if (len >= 19 && data[0] == 0xFF && data[1] == 0xFF) { data+=2; len-=2; }
    MeterPacket packet;
    const String mac=d->getAddress().toString().c_str();
    if (!parseElehantPacket(data,len,mac.c_str(),d->getRSSI(),packet)) return;
    Serial.printf("Elehant type=%u model=%u SN=%lu version=%u MAC=%s RSSI=%d\n",
      packet.type, packet.model, (unsigned long)packet.serial, packet.version,
      packet.mac.c_str(), packet.rssi);
    if (packet.hasReading) Serial.printf("Reading: %.5f %s\n", packet.reading, packet.unit.c_str());
    if (packet.hasReading2) Serial.printf("Reading 2: %.5f %s\n", packet.reading2, packet.unit.c_str());
    if (packet.hasBattery) Serial.printf("Battery: %u%%\n", packet.batteryPercent);
    if (packet.temperatureValid) Serial.printf("Temperature: %.2f C\n", packet.temperatureC);
    if (packet.type==7) Serial.println("WiFi box/gateway: excluded from meter readings.");
  }
};
static RawCallbacks scanCallbacks;

void bleScannerBegin() {
  NimBLEDevice::init("");
  scanner=NimBLEDevice::getScan();
  scanner->setScanCallbacks(&scanCallbacks, false);
  scanner->setActiveScan(false);
  scanner->setInterval(100);
  scanner->setWindow(80);
}
void bleScannerPoll() {
  if (scanner && !scanner->isScanning()) scanner->start(4, false, true);
}
