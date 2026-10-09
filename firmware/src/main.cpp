#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "hardware_profiles.h"
#include "button_manager.h"
#include "ble_scanner.h"
#include "meter_manager.h"

ButtonManager buttons;
MeterManager meterManager;
uint32_t lastStorageStatus=0;

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println();
  Serial.println(F("Elehant SVT-15 Monitor — firmware foundation"));
  Serial.printf("Hardware: %s\nUP GPIO: %d\nDOWN GPIO: %d\n",
                HW.name, HW.buttonUp, HW.buttonDown);
  if (!meterManager.begin()) Serial.println(F("WARNING: meter settings load failed"));
  Serial.printf("Saved meters: %u\n",meterManager.count());
  buttons.begin(HW.buttonUp, HW.buttonDown, HW.upPullup, HW.downPullup);
  bleScannerBegin();
}
void loop() {
  switch (buttons.update(millis())) {
    case ButtonEvent::UpShort: Serial.println(F("BUTTON UP")); break;
    case ButtonEvent::DownShort: Serial.println(F("BUTTON DOWN")); break;
    case ButtonEvent::BothShort: Serial.println(F("BUTTON BOTH SHORT")); break;
    case ButtonEvent::UpLong: Serial.println(F("BUTTON UP LONG")); break;
    case ButtonEvent::DownLong: Serial.println(F("BUTTON DOWN LONG")); break;
    case ButtonEvent::BothLong: Serial.println(F("BUTTON BOTH LONG")); break;
    case ButtonEvent::ApRecovery:
      Serial.println(F("AP recovery requested; AP reset is not connected yet."));
      break;
    default: break;
  }
  bleScannerPoll();
  const time_t now=time(nullptr);
  meterManager.process(millis(),now);
  if (millis()-lastStorageStatus>60000UL) {
    lastStorageStatus=millis();
    Serial.printf("Meters: %u; LittleFS: %llu/%llu bytes\n",meterManager.count(),
      (unsigned long long)meterManager.storage().filesystemUsed(),
      (unsigned long long)meterManager.storage().filesystemTotal());
  }
  delay(5);
}
