#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "hardware_profiles.h"
#include "button_manager.h"
#include "ble_scanner.h"
#include "meter_manager.h"
#include "display_manager.h"
#include "network_manager.h"

ButtonManager buttons;
MeterManager meterManager;
uint32_t lastStorageStatus=0;
uint32_t lastPruneAt=0;
uint8_t selectedMeter=0;
uint8_t selectedFlag=0;
bool configMode=false;
static const uint16_t editableFlags[]={METER_ACTIVE,METER_COLLECT,METER_HISTORY,METER_LCD,METER_WEB,METER_TELEGRAM,METER_TOTALS,METER_CALCULATOR,METER_ALARMS};
static const char* flagNames[]={"Active","Collect","History","LCD","Web","Telegram","Totals","Calculator","Alarms"};

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
  displayBegin();
  networkBegin(meterManager);
  bleScannerBegin();
}
void loop() {
  const ButtonEvent event=buttons.update(millis());
  if (event!=ButtonEvent::None) displayWake();
  switch (event) {
    case ButtonEvent::UpShort:
      if (configMode) selectedFlag=(selectedFlag+8)%9;
      else if (meterManager.count()) selectedMeter=(selectedMeter+meterManager.count()-1)%meterManager.count();
      break;
    case ButtonEvent::DownShort:
      if (configMode) selectedFlag=(selectedFlag+1)%9;
      else if (meterManager.count()) selectedMeter=(selectedMeter+1)%meterManager.count();
      break;
    case ButtonEvent::BothShort:
      if (!configMode) {
        configMode=true;
        selectedFlag=0;
        Serial.println(F("Meter settings: UP/DOWN select option; BOTH toggles; BOTH long exits."));
      } else if (meterManager.count()) {
        const MeterConfig* cfg=meterManager.configAt(selectedMeter);
        const bool enabled=cfg && (cfg->flags&editableFlags[selectedFlag]);
        meterManager.setFlagAt(selectedMeter,editableFlags[selectedFlag],!enabled);
        Serial.printf("Meter %u: %s=%s\n",selectedMeter+1,flagNames[selectedFlag],enabled?"OFF":"ON");
      }
      break;
    case ButtonEvent::BothLong:
      configMode=false;
      Serial.println(F("Meter settings closed."));
      break;
    case ButtonEvent::UpLong:
    case ButtonEvent::DownLong:
      if (configMode && meterManager.count()) {
        const MeterConfig* cfg=meterManager.configAt(selectedMeter);
        const bool enabled=cfg && (cfg->flags&editableFlags[selectedFlag]);
        meterManager.setFlagAt(selectedMeter,editableFlags[selectedFlag],!enabled);
      }
      break;
    case ButtonEvent::ApRecovery:
      Serial.println(F("AP recovery requested; restarting default access point."));
      networkRestartAp();
      break;
    default: break;
  }
  if (selectedMeter>=meterManager.count()) selectedMeter=0;
  networkLoop();
  bleScannerPoll();
  const time_t now=time(nullptr);
  meterManager.process(millis(),now);
  displayRender(meterManager,millis(),selectedMeter,configMode,selectedFlag);
  if (now>1760000000 && millis()-lastPruneAt>86400000UL) {
    meterManager.storage().pruneHistory(now,HISTORY_RETENTION_DAYS);
    lastPruneAt=millis();
  }
  if (millis()-lastStorageStatus>60000UL) {
    lastStorageStatus=millis();
    Serial.printf("Meters: %u; LittleFS: %llu/%llu bytes\n",meterManager.count(),
      (unsigned long long)meterManager.storage().filesystemUsed(),
      (unsigned long long)meterManager.storage().filesystemTotal());
  }
  delay(5);
}
