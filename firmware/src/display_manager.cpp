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
  #include <U8g2_for_Adafruit_GFX.h>
  static Adafruit_ST7735 screen(&SPI, HW.displayCs, HW.displayDc, HW.displayRst);
  static U8G2_FOR_ADAFRUIT_GFX cyrillic;
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
static const char* flagLabels[]={ "Активен","Сбор данных","История","Экран","Веб","Телеграм","Итоги","Калькулятор","Аварии" };

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
  cyrillic.begin(screen);
  cyrillic.setFont(u8g2_font_5x8_t_cyrillic);
  cyrillic.setFontMode(1);
  cyrillic.setForegroundColor(TFT_WHITE);
  cyrillic.setBackgroundColor(TFT_BLACK);
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
#elif defined(DISPLAY_ST7735)
  const uint16_t background=TFT_BLACK;
  screen.fillScreen(background);
  cyrillic.setFont(u8g2_font_5x8_t_cyrillic);
  cyrillic.setForegroundColor(TFT_WHITE);
  cyrillic.setBackgroundColor(background);
  cyrillic.drawUTF8(0,7,"СВТ-15 Монитор");
  screen.drawFastHLine(0,10,screen.width(),TFT_WHITE);

  if (!count) {
    cyrillic.drawUTF8(0,22,"Нет данных");
    cyrillic.drawUTF8(0,33,"Поиск счётчика BLE...");
  } else {
    char line[64];
    snprintf(line,sizeof(line),"Счётчик %u из %u  №%lu",selectedMeter+1,count,(unsigned long)(cfg?cfg->serial:0));
    cyrillic.drawUTF8(0,20,line);

    if (configMode) {
      cyrillic.drawUTF8(0,32,"Настройки счётчика");
      cyrillic.drawUTF8(0,43,flagLabels[flag]);
      cyrillic.drawUTF8(0,54,enabled?"Включено":"Выключено");
      cyrillic.drawUTF8(0,65,"Вверх/Вниз: выбор");
      cyrillic.drawUTF8(0,76,"Обе: переключить");
      cyrillic.drawUTF8(0,87,"Обе долго: выход");
    } else if (!(cfg->flags&METER_LCD)) {
      cyrillic.drawUTF8(0,36,"Экран выключен");
      cyrillic.drawUTF8(0,47,"для этого счётчика");
    } else {
      int y=31;
      uint32_t ageMinutes=0;
      if (packet) ageMinutes=(nowMillis-meters.lastSeenMillisAt(selectedMeter))/60000UL;

      if (packet && packet->hasReading) {
        snprintf(line,sizeof(line),"Горячая: %.3f %s",packet->reading,packet->unit.c_str());
        cyrillic.setForegroundColor(TFT_YELLOW);
        cyrillic.drawUTF8(0,y,line);
        cyrillic.setForegroundColor(TFT_WHITE);
        y+=11;
      }
      if (packet && packet->hasReading2) {
        snprintf(line,sizeof(line),"Холодная: %.3f %s",packet->reading2,packet->unit.c_str());
        cyrillic.setForegroundColor(TFT_BLUE);
        cyrillic.drawUTF8(0,y,line);
        cyrillic.setForegroundColor(TFT_WHITE);
        y+=11;
      }
      if (!packet) {
        cyrillic.setForegroundColor(TFT_RED);
        cyrillic.drawUTF8(0,y,"Авария: нет данных");
        cyrillic.setForegroundColor(TFT_WHITE);
        y+=11;
      } else {
        if (packet->hasBattery) {
          snprintf(line,sizeof(line),"Батарея: %u%%",packet->batteryPercent);
          cyrillic.drawUTF8(0,y,line);
          y+=10;
        }
        if (packet->temperatureValid) {
          snprintf(line,sizeof(line),"Температура: %.1f C",packet->temperatureC);
          cyrillic.drawUTF8(0,y,line);
          y+=10;
        }
        snprintf(line,sizeof(line),"Данные %lu мин назад",(unsigned long)ageMinutes);
        cyrillic.drawUTF8(0,y,line);
        y+=10;
      }

      if (alarmActive && packet) {
        cyrillic.setForegroundColor(TFT_RED);
        cyrillic.drawUTF8(0,y,"Авария");
        cyrillic.setForegroundColor(TFT_WHITE);
      }

      if (WiFi.status()==WL_CONNECTED) {
        String ssid=WiFi.SSID();
        if (ssid.length()>19) ssid=ssid.substring(0,19);
        String wifiLine="Сеть: "+ssid;
        cyrillic.drawUTF8(0,screen.height()-17,wifiLine.c_str());
        String ipLine="IP: "+WiFi.localIP().toString();
        cyrillic.drawUTF8(0,screen.height()-7,ipLine.c_str());
      } else {
        cyrillic.drawUTF8(0,screen.height()-7,"Домашняя сеть: нет");
      }
    }
  }
#elif defined(DISPLAY_ST7789)
  // Keep the legacy TFT_eSPI rendering path for TTGO boards.
  screen.fillScreen(TFT_BLACK);
  screen.setTextColor(TFT_WHITE, TFT_BLACK);
  screen.setTextSize(1);
  screen.setCursor(0, 0);
  screen.println(F("SVT-15 Monitor"));
  if (!count) {
    screen.println(F("No meter data"));
  } else {
    screen.print(F("Meter "));
    screen.print(selectedMeter + 1);
    screen.print('/');
    screen.println(count);
    screen.print(F("SN: "));
    screen.println(cfg ? cfg->serial : 0);
    if (configMode) {
      screen.println(F("Meter settings"));
      screen.println(flagLabels[flag]);
      screen.println(enabled ? F("Enabled") : F("Disabled"));
    } else if (!(cfg->flags & METER_LCD)) {
      screen.println(F("Display disabled"));
    } else if (packet && packet->hasReading) {
      screen.print(F("Reading: "));
      screen.print(packet->reading, 3);
      screen.print(' ');
      screen.println(packet->unit.c_str());
      if (packet->hasReading2) {
        screen.print(F("Reading 2: "));
        screen.print(packet->reading2, 3);
        screen.print(' ');
        screen.println(packet->unit.c_str());
      }
      if (packet->hasBattery) {
        screen.print(F("Battery: "));
        screen.print(packet->batteryPercent);
        screen.println('%');
      }
    } else {
      screen.println(F("No data"));
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
