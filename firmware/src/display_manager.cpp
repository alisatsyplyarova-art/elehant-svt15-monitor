#include "display_manager.h"
#include "config.h"
#include "hardware_profiles.h"
#include <time.h>
#include <WiFi.h>

#if defined(DISPLAY_LCD5110)
  #include <Adafruit_GFX.h>
  #include <Adafruit_PCD8544.h>
  static Adafruit_PCD8544 screen(HW.displaySclk, HW.displayMosi, HW.displayDc, HW.displayCs, HW.displayRst);
#elif defined(DISPLAY_ST7735)
  #include <SPI.h>
  #include <Adafruit_ST7735.h>
  static Adafruit_ST7735 screen(&SPI, HW.displayCs, HW.displayDc, HW.displayRst);
  #define TFT_BLACK ST77XX_BLACK
  #define TFT_WHITE ST77XX_WHITE
  #define TFT_RED   ST77XX_RED
  #define TFT_YELLOW ST77XX_YELLOW
  #define TFT_BLUE ST77XX_BLUE
#elif defined(DISPLAY_ST7789)
  #include <TFT_eSPI.h>
  static TFT_eSPI screen;
#endif

static uint32_t lastDrawAt=millis()-1000UL;
static bool initialized=false;
static uint32_t backlightDeadline=0;
static bool displayAwake=false;
static bool alarmActive=false;

static void setBacklight(bool on) {
#if defined(DISPLAY_LCD5110) || defined(DISPLAY_ST7789) || defined(DISPLAY_ST7735)
  if (HW.displayBacklight>=0) digitalWrite(HW.displayBacklight,on?HIGH:LOW);
#else
  (void)on;
#endif
}

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
    digitalWrite(HW.displayBacklight,LOW);
  }
#elif defined(DISPLAY_ST7735)
  // Match the known-working Claude monitor_elehant v4.6-cl display setup.
  SPI.begin(HW.displaySclk, -1, HW.displayMosi, -1);
  screen.initR(INITR_BLACKTAB);
  screen.setRotation(1);
  screen.fillScreen(TFT_BLACK);
  screen.setTextSize(1);
  screen.setTextColor(TFT_WHITE, TFT_BLACK);
  if (HW.displayPower>=0) {
    pinMode(HW.displayPower,OUTPUT);
    digitalWrite(HW.displayPower,HIGH);
  }
  if (HW.displayBacklight>=0) {
    pinMode(HW.displayBacklight,OUTPUT);
    digitalWrite(HW.displayBacklight,LOW);
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
    digitalWrite(HW.displayBacklight,LOW);
  }
#endif
  initialized=true;
  lastDrawAt=0;
  backlightDeadline=0;
  displayAwake=false;
  alarmActive=false;
  setBacklight(false);
}

void displayRender(const MeterManager& meters, uint32_t nowMillis, uint8_t selectedMeter, bool configMode, uint8_t selectedFlag) {
#if defined(BOARD_TTGO_TDISPLAY) || defined(BOARD_TTGO_TDISPLAY_S3)
  if (!initialized || nowMillis-lastDrawAt<250) return;
#else
  if (!initialized || nowMillis-lastDrawAt<1000) return;
#endif
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
    if (!(cfg->flags&METER_LCD) && !configMode) {
      screen.println(F("Display disabled for this meter"));
      screen.println(F("BOTH: settings"));
    } else if (configMode) {
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
#elif defined(DISPLAY_ST7789) || defined(DISPLAY_ST7735)
  // Keep the screen calm and readable: no full-screen alarm flashing.
  const uint16_t background=TFT_BLACK;
  screen.fillScreen(background);
  screen.setTextSize(1);
  screen.setTextColor(TFT_WHITE,background);
  screen.setCursor(0,0);
  screen.println(F("SVT-15 Monitor"));
  screen.drawFastHLine(0,9,screen.width(),TFT_WHITE);

  if (!count) {
    screen.setCursor(0,14);
    screen.println(F("Нет данных"));
    screen.println(F("Поиск счетчика BLE..."));
  } else {
    screen.setCursor(0,13);
    screen.print(F("#")); screen.print(selectedMeter+1); screen.print('/'); screen.print(count);
    screen.print(F(" SN:")); screen.println(cfg?cfg->serial:0);

    if (configMode) {
      screen.setCursor(0,27);
      screen.println(F("НАСТРОЙКИ СЧЕТЧИКА"));
      screen.print(F("> ")); screen.println(flagLabels[flag]);
      screen.print(F("Состояние: ")); screen.println(enabled?F("ON"):F("OFF"));
      screen.println(F("UP/DOWN: выбор"));
      screen.println(F("BOTH: переключить"));
      screen.println(F("BOTH long: выход"));
    } else if (!(cfg->flags&METER_LCD)) {
      screen.setCursor(0,29);
      screen.println(F("Экран отключен"));
      screen.println(F("для этого счетчика"));
    } else {
      uint16_t ageMinutes=0;
      const uint32_t lastSeen=meters.lastSeenMillisAt(selectedMeter);
      if (packet) ageMinutes=(uint16_t)((nowMillis-lastSeen)/60000UL);

      // Tariff 1 = hot water (yellow); tariff 2 = cold water (blue).
      int y=28;
      if (packet && packet->hasReading) {
        screen.setCursor(0,y);
        screen.setTextColor(TFT_YELLOW,background);
        screen.print(F("ГОРЯЧАЯ: "));
        screen.print(packet->reading,3); screen.print(' '); screen.println(packet->unit);
        y+=13;
      }
      if (packet && packet->hasReading2) {
        screen.setCursor(0,y);
        screen.setTextColor(TFT_BLUE,background);
        screen.print(F("ХОЛОДНАЯ: "));
        screen.print(packet->reading2,3); screen.print(' '); screen.println(packet->unit);
        y+=13;
      }
      screen.setTextColor(TFT_WHITE,background);
      if (!packet) {
        screen.setCursor(0,y);
        screen.println(F("Авария: нет данных"));
        y+=12;
      } else {
        if (packet->hasBattery) {
          screen.setCursor(0,y);
          screen.print(F("Батарея: ")); screen.print(packet->batteryPercent); screen.println('%');
          y+=11;
        }
        if (packet->temperatureValid) {
          screen.setCursor(0,y);
          screen.print(F("Температура: ")); screen.print(packet->temperatureC,1); screen.println(F(" C"));
          y+=11;
        }
        screen.setCursor(0,y);
        screen.print(F("Данные: ")); screen.print(ageMinutes); screen.println(F(" мин назад"));
        y+=11;
      }
      if (alarmActive) {
        screen.setCursor(0,y);
        screen.setTextColor(TFT_RED,background);
        if (!packet) screen.println(F("АВАРИЯ: НЕТ ДАННЫХ"));
        else screen.println(F("АВАРИЯ"));
        y+=11;
        screen.setTextColor(TFT_WHITE,background);
      }
      // Home Wi-Fi status is shown only when connected to the router.
      if (WiFi.status()==WL_CONNECTED && y<=screen.height()-20) {
        screen.setCursor(0,screen.height()-19);
        screen.print(F("Wi-Fi: "));
        String ssid=WiFi.SSID();
        if (ssid.length()>18) ssid=ssid.substring(0,18);
        screen.println(ssid);
        screen.setCursor(0,screen.height()-9);
        screen.print(F("IP: "));
        screen.println(WiFi.localIP());
      } else if (y<=screen.height()-10) {
        screen.setCursor(0,screen.height()-9);
        screen.println(F("Wi-Fi: не подключен"));
      }
      // Keep the most useful navigation hint out of the data area.
    }
  }
#elif defined(DISPLAY_NONE)
  (void)meters; (void)selectedMeter; (void)configMode; (void)selectedFlag;
#endif
}

void displayWake() {
  displayAwake=true;
  backlightDeadline=millis()+BACKLIGHT_TIMEOUT_MS;
  setBacklight(true);
#if defined(DISPLAY_ST7789) || defined(DISPLAY_ST7735)
  if (HW.displayPower>=0) digitalWrite(HW.displayPower,HIGH);
#endif
}

void displaySleep() {
  displayAwake=false;
  backlightDeadline=0;
  if (!alarmActive) setBacklight(false);
}

bool displayButtonActivity(uint32_t nowMillis) {
  const bool wasAwake=displayAwake && (int32_t)(backlightDeadline-nowMillis)>0;
  displayAwake=true;
  backlightDeadline=nowMillis+BACKLIGHT_TIMEOUT_MS;
  if (!wasAwake) {
    setBacklight(true);
    return true; // first press wakes the screen without changing selection
  }
  return false; // subsequent press extends timeout and keeps its normal action
}

void displaySetAlarm(bool active) {
  if (alarmActive==active) return;
  alarmActive=active;
  if (active) setBacklight(true);
  else setBacklight(displayAwake && (int32_t)(backlightDeadline-millis())>0);
  lastDrawAt=0;
}

void displayService(uint32_t nowMillis) {
  if (alarmActive) {
    setBacklight(true);
  } else if (!displayAwake || (int32_t)(nowMillis-backlightDeadline)>=0) {
    displayAwake=false;
    setBacklight(false);
  } else {
    setBacklight(true);
  }
}
