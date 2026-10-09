#include "display_manager.h"
#include "config.h"
#include "hardware_profiles.h"

#if defined(DISPLAY_LCD5110)
  #include <Adafruit_GFX.h>
  #include <Adafruit_PCD8544.h>
  static Adafruit_PCD8544 screen(HW.displaySclk, HW.displayMosi, HW.displayDc, HW.displayCs, HW.displayRst);
#elif defined(DISPLAY_ST7789)
  #include <TFT_eSPI.h>
  static TFT_eSPI screen;
#endif

static uint32_t lastDrawAt=0;
static bool initialized=false;
static const uint16_t flagBits[]={METER_ACTIVE,METER_COLLECT,METER_HISTORY,METER_LCD,METER_WEB,METER_TELEGRAM,METER_TOTALS,METER_CALCULATOR,METER_ALARMS};
static const char* flagLabels[]={ "Active","Collect","History","LCD","Web","Telegram","Totals","Calculator","Alarms" };

void displayBegin() {
#if defined(DISPLAY_LCD5110)
  screen.begin();
  screen.setContrast(55);
  screen.clearDisplay();
  screen.setTextSize(1);
  screen.setTextColor(BLACK);
  if (HW.displayBacklight>=0) {
    pinMode(HW.displayBacklight,OUTPUT);
    digitalWrite(HW.displayBacklight,HIGH);
  }
#elif defined(DISPLAY_ST7789)
  screen.init();
  screen.setRotation(1);
  screen.fillScreen(TFT_BLACK);
  screen.setTextSize(1);
  screen.setTextColor(TFT_WHITE,TFT_BLACK);
  if (HW.displayPower>=0) {
    pinMode(HW.displayPower,OUTPUT);
    digitalWrite(HW.displayPower,HIGH);
  }
  if (HW.displayBacklight>=0) {
    pinMode(HW.displayBacklight,OUTPUT);
    digitalWrite(HW.displayBacklight,HIGH);
  }
#endif
  initialized=true;
  lastDrawAt=0;
}

void displayRender(const MeterManager& meters, uint32_t nowMillis, uint8_t selectedMeter, bool configMode, uint8_t selectedFlag) {
  if (!initialized || nowMillis-lastDrawAt<250) return;
  lastDrawAt=nowMillis;
  const uint8_t count=meters.count();
  if (count && selectedMeter>=count) selectedMeter=0;
  const MeterConfig* cfg=count?meters.configAt(selectedMeter):nullptr;
  const MeterPacket* packet=count?meters.latestAt(selectedMeter):nullptr;
  const uint8_t flag=(selectedFlag<9)?selectedFlag:0;
  const bool enabled=cfg && (cfg->flags&flagBits[flag]);
#if defined(DISPLAY_LCD5110)
  screen.clearDisplay();
  screen.setCursor(0,0);
  screen.println(F("SVT-15 Monitor"));
  if (!count) {
    screen.println(F("Searching BLE..."));
    screen.println(F("UP/DN: browse"));
  } else {
    screen.print(F("#")); screen.print(selectedMeter+1); screen.print('/'); screen.println(count);
    screen.print(F("SN:")); screen.println(cfg?cfg->serial:0);
    if (configMode) {
      screen.print(F(">")); screen.println(flagLabels[flag]);
      screen.print(F("Value: ")); screen.println(enabled?F("ON"):F("OFF"));
      screen.println(F("UP/DN choose"));
      screen.println(F("BOTH toggle"));
    } else {
      if (packet && packet->hasReading) {
        screen.print(packet->reading,3); screen.print(' '); screen.println(packet->unit);
      } else if (packet && packet->fieldCount) {
        screen.print(packet->fields[0].key); screen.print(':'); screen.println(packet->fields[0].value,2);
      } else screen.println(F("Waiting BLE..."));
      if (packet && packet->hasBattery) { screen.print(F("Batt ")); screen.print(packet->batteryPercent); screen.println('%'); }
      screen.println(F("BOTH: settings"));
    }
  }
  screen.display();
#elif defined(DISPLAY_ST7789)
  screen.fillScreen(TFT_BLACK);
  screen.setCursor(0,0);
  screen.setTextSize(2);
  screen.println(F("SVT-15"));
  screen.setTextSize(1);
  screen.print(F("Meters: ")); screen.println(count);
  if (!count) {
    screen.println(F("Searching for BLE meters..."));
  } else {
    screen.println();
    screen.print(F("Selected: ")); screen.print(selectedMeter+1); screen.print('/'); screen.println(count);
    screen.print(F("Serial: ")); screen.println(cfg?cfg->serial:0);
    if (configMode) {
      screen.println(F("METER SETTINGS"));
      screen.print(F("> ")); screen.println(flagLabels[flag]);
      screen.print(F("State: ")); screen.println(enabled?F("ON"):F("OFF"));
      screen.println(F("UP/DOWN: choose"));
      screen.println(F("BOTH: toggle"));
      screen.println(F("BOTH long: exit"));
    } else {
      if (packet && packet->hasReading) {
        screen.print(F("Reading: ")); screen.print(packet->reading,4); screen.print(' '); screen.println(packet->unit);
      }
      if (packet && packet->hasReading2) {
        screen.print(F("Reading 2: ")); screen.print(packet->reading2,4); screen.print(' '); screen.println(packet->unit);
      }
      if (packet) {
        for (uint8_t i=0; i<packet->fieldCount && i<5; ++i) {
          screen.print(packet->fields[i].key); screen.print(F(": "));
          screen.print(packet->fields[i].value,2); screen.print(' '); screen.println(packet->fields[i].unit);
        }
        if (packet->hasBattery) { screen.print(F("Battery: ")); screen.print(packet->batteryPercent); screen.println('%'); }
        screen.print(F("RSSI: ")); screen.print(packet->rssi); screen.println(F(" dBm"));
      } else screen.println(F("Waiting for BLE packet"));
      screen.println(F("UP/DOWN: meter"));
      screen.println(F("BOTH: settings"));
    }
  }
#elif defined(DISPLAY_NONE)
  (void)meters; (void)selectedMeter; (void)configMode; (void)selectedFlag;
#endif
}

void displayWake() {
#if defined(DISPLAY_LCD5110)
  if (HW.displayBacklight>=0) digitalWrite(HW.displayBacklight,HIGH);
#elif defined(DISPLAY_ST7789)
  if (HW.displayPower>=0) digitalWrite(HW.displayPower,HIGH);
  if (HW.displayBacklight>=0) digitalWrite(HW.displayBacklight,HIGH);
#endif
}

void displaySleep() {
#if defined(DISPLAY_LCD5110)
  if (HW.displayBacklight>=0) digitalWrite(HW.displayBacklight,LOW);
#elif defined(DISPLAY_ST7789)
  if (HW.displayBacklight>=0) digitalWrite(HW.displayBacklight,LOW);
#endif
}
