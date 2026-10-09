#include <Arduino.h>
#include <NimBLEDevice.h>
#include "ble_scanner.h"
#include "protocol_parser.h"

static NimBLEScan* scanner=nullptr;

class RawCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(const NimBLEAdvertisedDevice* d) override {
    std::string mfg=d->getManufacturerData();
    if (mfg.size() < 2) return;
    // BLE libraries can expose manufacturer data with or without company ID.
    // This project expects 17 protocol bytes after the 0xFFFF company ID.
    const uint8_t* data=reinterpret_cast<const uint8_t*>(mfg.data());
    size_t len=mfg.size();
    if (len >= 19 && data[0] == 0xFF && data[1] == 0xFF) { data+=2; len-=2; }
    MeterPacket packet;
    const bool parsed=parseElehantPacket(data,len,d->getAddress().toString().c_str(),d->getRSSI(),packet);
    if (!parsed) return;
    Serial.printf("Elehant type=%u model=%u SN=%lu version=%u MAC=%s RSSI=%d\n",
      packet.type, packet.model, (unsigned long)packet.serial, packet.version,
      packet.mac.c_str(), packet.rssi);
    if (packet.hasReading) Serial.printf("Reading: %.5f %s\n", packet.reading, packet.unit.c_str());
    if (packet.hasBattery) Serial.printf("Battery: %u%%\n", packet.batteryPercent);
    if (packet.temperatureValid) Serial.printf("Temperature: %.2f C\n", packet.temperatureC);
    if (packet.type==7) Serial.println("WiFi box/gateway: excluded from meter readings.");
  }
};

void bleScannerBegin() {
  NimBLEDevice::init("");
  scanner=NimBLEDevice::getScan();
  scanner->setScanCallbacks(new RawCallbacks(), false);
  scanner->setActiveScan(false);
  scanner->setInterval(100);
  scanner->setWindow(80);
}
void bleScannerPoll() {
  if (scanner && !scanner->isScanning()) scanner->start(4, false, true);
}
