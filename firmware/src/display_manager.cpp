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
static uint8_t page=0;
static bool initialized=false;

#if defined(DISPLAY_LCD5110) || defined(DISPLAY_ST7789)
static void printFieldLine(const MeterPacket* packet, uint8_t row) {
  if (!packet || !packet->fieldCount) return;
  const uint8_t index=(uint8_t)(row % packet->fieldCount);
  const MeasurementField& f=packet->fields[index];
  String line=f.key;
  line += ":";
  line += String(f.value,2);
  line += f.unit;
  screen.println(line);
}
#endif

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

void displayRender(const MeterManager& meters, uint32_t nowMillis) {
  if (!initialized || nowMillis-lastDrawAt<1000) return;
  lastDrawAt=nowMillis;
  const uint8_t count=meters.count();
  if (count) page=(uint8_t)((nowMillis/5000UL)%count);
#if defined(DISPLAY_LCD5110)
  screen.clearDisplay();
  screen.setCursor(0,0);
  screen.println(F("SVT-15 Monitor"));
  screen.print(F("Meters: ")); screen.println(count);
  if (count) {
    const MeterConfig* cfg=meters.configAt(page);
    const MeterPacket* packet=meters.latestAt(page);
    screen.print(F("#")); screen.print(page+1); screen.print(F(" SN:")); screen.println(cfg?cfg->serial:0);
    if (packet && packet->hasReading) {
      screen.print(packet->reading,3); screen.print(' '); screen.println(packet->unit);
    } else if (packet && packet->fieldCount) {
      printFieldLine(packet,0);
    } else screen.println(F("Waiting BLE..."));
    if (packet && packet->hasBattery) { screen.print(F("Battery ")); screen.print(packet->batteryPercent); screen.println('%'); }
  } else screen.println(F("Searching BLE..."));
  screen.display();
#elif defined(DISPLAY_ST7789)
  screen.fillScreen(TFT_BLACK);
  screen.setCursor(0,0);
  screen.setTextSize(2);
  screen.println(F("SVT-15"));
  screen.setTextSize(1);
  screen.print(F("Meters discovered: ")); screen.println(count);
  if (count) {
    const MeterConfig* cfg=meters.configAt(page);
    const MeterPacket* packet=meters.latestAt(page);
    screen.println();
    screen.print(F("Meter ")); screen.print(page+1); screen.print('/'); screen.println(count);
    screen.print(F("Serial: ")); screen.println(cfg?cfg->serial:0);
    if (packet && packet->hasReading) {
      screen.print(F("Reading: ")); screen.print(packet->reading,4); screen.print(' '); screen.println(packet->unit);
    }
    if (packet && packet->hasReading2) {
      screen.print(F("Reading 2: ")); screen.print(packet->reading2,4); screen.print(' '); screen.println(packet->unit);
    }
    if (packet) {
      for (uint8_t i=0; i<packet->fieldCount && i<8; ++i) {
        screen.print(packet->fields[i].key); screen.print(F(": "));
        screen.print(packet->fields[i].value,2); screen.print(' '); screen.println(packet->fields[i].unit);
      }
      if (packet->hasBattery) { screen.print(F("Battery: ")); screen.print(packet->batteryPercent); screen.println('%'); }
      screen.print(F("RSSI: ")); screen.print(packet->rssi); screen.println(F(" dBm"));
    } else screen.println(F("Waiting for BLE packet"));
  } else screen.println(F("Searching for meters..."));
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
