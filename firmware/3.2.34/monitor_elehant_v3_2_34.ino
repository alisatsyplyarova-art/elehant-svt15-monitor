// Версия 3.2.34, изменения: Σ вместо ОБ; ячейка суммы равна T1/T2.
// Monitor Elehant (monitor_elehant): BLE-приём (пассивный) + таблица счётчиков + дисплей + Wi-Fi AP+STA + WEB + Serial0
// Плата и дисплей выбираются ниже (BOARD_TYPE, DISPLAY_TYPE). Пины: monitor_boards.h, общие настройки: monitor_profile.h

// ======================= ВЫБОР ПЛАТЫ =======================
//   1 = ESP32-C3 (режим экономии ОЗУ), в Arduino IDE: ESP32C3 Dev Module
//   2 = ESP32 классический (DevKit),   в Arduino IDE: ESP32 Dev Module
//   3 = ESP32-T (плата T-Display со встроенным дисплеем), в Arduino IDE: ESP32 Dev Module
//   4 = ESP32-S (ESP32-S3 Dual Type-C),   в Arduino IDE: ESP32S3 Dev Module
#define BOARD_ESP32C3 1
#define BOARD_ESP32   2
#define BOARD_ESP32T  3
#define BOARD_ESP32S  4
#define BOARD_TYPE    1   // <-- ВЫБОР ПЛАТЫ: 1, 2, 3 или 4
// ===========================================================

// ===================== ВЫБОР ДИСПЛЕЯ =====================
//   1 = Nokia 5110 (монохромный 84x48)
//   2 = цветной TFT 1.77" 160x128 (ST7735)
//   3 = цветной TFT на ST7789
#define DISPLAY_LCD5110 1
#define DISPLAY_ST7735  2
#define DISPLAY_ST7789  3
#define DISPLAY_TYPE    2   // <-- ВЫБОР ДИСПЛЕЯ: 1, 2 или 3
// У платы ESP32-T дисплей встроенный (ST7789): выбор игнорируется
#if BOARD_TYPE == BOARD_ESP32T
  #undef DISPLAY_TYPE
  #define DISPLAY_TYPE DISPLAY_ST7789
#endif
// =========================================================

// ===================== НАСТРОЙКИ ПЛАТЫ =====================
// Сторож Wi-Fi: если домашняя сеть настроена, но не подключается дольше этого времени (минут),
// плата перезапускается. Точка доступа продолжает работать до перезапуска.
// 0 — сторож выключен. Выставьте 0, если домашняя сеть у вас часто недоступна по другим причинам:
// иначе плата будет перезапускаться каждые 30 минут.
#define SVT_WDT_WIFI_MIN 30
// ===========================================================

#include "monitor_profile.h"
#include "monitor_common.h"
#if SVT_BLE_NIMBLE
  #include <NimBLEDevice.h>
#else
  #include <BLEDevice.h>
  #include <BLEScan.h>
  #include <BLEAdvertisedDevice.h>
#endif
#include "monitor_decoder.h"
#include "monitor_meters.h"
#include "monitor_time.h"
#include "monitor_history.h"
#include "monitor_alarms.h"
#include "monitor_wifi.h"
#include "monitor_web.h"
#include "monitor_captive.h"
#include "monitor_buttons.h"
#include "monitor_service.h"

#if DISPLAY_TYPE == DISPLAY_LCD5110
  #include "monitor_lcd5110.h"
  typedef Svt15Lcd Svt15Display;
#else
  #include "monitor_tft.h"
  typedef Svt15Tft Svt15Display;
#endif

Svt15Display display;
MeterManager meters;
TimeManager clk;
HistoryManager history;
AlertManager alerts;
WifiManager wifi;
Svt15Web web;
CaptiveDns captive;
MeterCfgStore store;
TelegramManager tg;
ServiceMenu svc;
Buttons buttons;
#if !SVT_BLE_NIMBLE
BLEScan* scan;
#endif
uint8_t current = 0;

// ---------- очередь событий: BLE-задача только кладёт данные, печать в loop() ----------
struct PacketEvent {
  MeterData m;        // как декодировано из пакета
  uint8_t raw[19];
  uint8_t rawLen;
  int8_t idx;         // индекс в таблице
  uint8_t total;      // счётчиков в таблице
};
PacketEvent evq[SVT_EVQ_SIZE];
volatile uint8_t evHead = 0, evTail = 0;

// ---------- BLE ----------
// Обработка одного BLE-объявления (общая для обеих BLE-библиотек)
static void handleAdvert(const uint8_t* mfg, size_t len, int rssi) {
  if (len == 0) return;

  MeterData m;
  if (!decodeSVT15(mfg, len, m)) return;
  m.rssi = rssi;
  m.lastSeenMs = millis();

  uint8_t total = 0;
  int idx = meters.update(m, &total);
  if (idx < 0) return;   // таблица полна

  uint8_t next = (evHead + 1) % SVT_EVQ_SIZE;
  if (next == evTail) return;   // очередь занята: печать пропускаем
  PacketEvent& e = evq[evHead];
  e.m = m;
  e.rawLen = len > 19 ? 19 : len;
  memcpy(e.raw, mfg, e.rawLen);
  e.idx = idx;
  e.total = total;
  evHead = next;
}

#if SVT_BLE_NIMBLE
// NimBLE-Arduino 2.x: сканирование идёт непрерывно в собственной задаче библиотеки
class NimCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    std::string mfg = dev->getManufacturerData();
    handleAdvert((const uint8_t*)mfg.data(), mfg.size(), dev->getRSSI());
  }
};
static NimCallbacks nimCallbacks;
#else
class MyCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice device) {
    String mfg = device.getManufacturerData();
    handleAdvert((const uint8_t*)mfg.c_str(), mfg.length(), device.getRSSI());
  }
};

void scanTask(void*) {
  for (;;) {
    scan->start(5, false);
    scan->clearResults();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
#endif

// ---------- память ----------
// Свободно сейчас, минимум с запуска и самый большой свободный блок
static void heapNote(const char* where) {
  SVT_SERIAL.printf("Heap %-8s free=%u min=%u maxBlock=%u\n", where, (unsigned)ESP.getFreeHeap(),
                    (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
}

// ---------- кнопки ----------
bool pressed(uint8_t pin) {
  static uint32_t last[2] = {0, 0};
  uint8_t i = (pin == BTN_UP_PIN) ? 0 : 1;
  if (digitalRead(pin) == LOW && millis() - last[i] > 200) {
    last[i] = millis();
    return true;
  }
  return false;
}

void setup() {
  SVT_SERIAL.begin(115200);
  pinMode(BTN_UP_PIN, BTN_UP_MODE);
  pinMode(BTN_DOWN_PIN, BTN_DOWN_MODE);

  SVT_SERIAL.println();
  SVT_SERIAL.println("monitor_elehant " SVT15_LCD_VERSION " start");
  SVT_SERIAL.println("Board: " BOARD_NAME);
#if SVT_BLE_NIMBLE
  SVT_SERIAL.println("BLE: NimBLE");
#else
  SVT_SERIAL.println("BLE: built-in (Bluedroid)");
#endif

  // UP+DOWN при включении 5 сек: заводские SSID и пароль AP
  bool resetAp = svtApRecoveryRequested();
  if (resetAp) SVT_SERIAL.println("AP parameters reset to factory");

  heapNote("boot");
  display.begin();
#if SVT_SELFTEST
  display.selfTest();
#endif
  display.showSplash();
  clk.begin();                          // часовой пояс и NTP из памяти
  meters.begin();                       // восстановить счётчики из памяти
  SVT_SERIAL.printf("Restored meters: %u\n", meters.count());
  history.begin(&clk, &meters);         // LittleFS и история
  alerts.begin(&meters, &clk);          // тревоги и напоминания
  store.begin();                        // настройки счётчиков
  tg.begin(&meters, &alerts, &clk, &wifi, &store);
  heapNote("meters");
  delay(1500);

#if SVT_BLE_NIMBLE
  NimBLEDevice::init("");
  NimBLEScan* nimScan = NimBLEDevice::getScan();
  nimScan->setScanCallbacks(&nimCallbacks, false);  // повторы фильтруются, фильтр сбрасывается перезапуском скана
  nimScan->setActiveScan(false);                    // пассивное сканирование
  nimScan->setInterval(SVT_BLE_SCAN_INTERVAL_MS);
  nimScan->setWindow(SVT_BLE_SCAN_WINDOW_MS);
  nimScan->setMaxResults(0);                        // результаты в памяти не копить
  nimScan->start(0, false, true);                   // 0: сканировать до остановки (перезапуск раз в 5 с в loop())
#else
  BLEDevice::init("");
  scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new MyCallbacks());
  scan->setActiveScan(false);  // пассивное сканирование
  xTaskCreate(scanTask, "scan", SVT_SCAN_STACK, NULL, 1, NULL);
#endif

  heapNote("ble");
  wifi.begin(resetAp);                  // точка доступа
  web.begin(&meters, &wifi, &clk, &history, &alerts, &store, &tg);
  svc.begin(&wifi);            // WEB-интерфейс
#if SVT_FAKE_INTERNET
  captive.begin();                      // DNS точки доступа: любое имя -> адрес платы
#endif
  SVT_SERIAL.printf("AP: SSID=%s IP=%s\n", wifi.apSsid().c_str(), wifi.apIp().c_str());
  if (wifi.staEnabled()) SVT_SERIAL.printf("STA: connecting to %s\n", wifi.staSsid().c_str());
  heapNote("web");
}

void loop() {
  web.loop();
#if SVT_FAKE_INTERNET
  captive.loop();
#endif

  // печать принятых пакетов
  while (evTail != evHead) {
    PacketEvent& e = evq[evTail];
#if SVT_DEBUG
    svt15PrintRaw(e.m, e.raw, e.rawLen);
#endif
    MeterData merged;
    if (meters.get(e.idx, merged)) svt15PrintSerial(merged, e.idx, e.total);
    evTail = (evTail + 1) % SVT_EVQ_SIZE;
  }

  // кнопки: одиночные (переключение или подтверждение тревоги), обе коротко (подтвердить все),
  // обе удерживать 3 с (сервисное меню). В сервисном меню кнопки управляют меню.
  uint8_t cnt = meters.count();
  BtnEvent ev = buttons.poll();
  if (svc.active()) {
    svc.onEvent(ev);
  } else if (ev == BTN_BOTH_LONG) {
    svc.open();
  } else if (ev == BTN_BOTH) {
    if (alerts.visibleCount() > 0) alerts.ackAll();
  } else if (ev == BTN_UP || ev == BTN_DOWN) {
    if (alerts.visibleCount() > 0 && SVT_ACK_SINGLE_BUTTON) alerts.ackCurrent();
    else if (cnt) current = (ev == BTN_UP) ? (current + 1) % cnt : (current + cnt - 1) % cnt;
  }
  if (cnt && current >= cnt) current = 0;
  svc.loop();
  tg.loop();

  // экран
  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 500) {
    lastDraw = millis();
    WifiInfo wi;
    wifi.fillInfo(wi);
    display.setWifiInfo(wi);
    AlertInfo ai;
    alerts.current(ai);
    display.setAlert(ai);
    ClockInfo ci;
    clk.fillInfo(ci);
    display.setClock(ci);
    MeterData m;
    if (svc.active()) {
      MenuView v;
      svc.build(v);
      display.showMenu(v);
    } else if (cnt && meters.get(current, m)) {
      char l1[12], l2[12];
      store.screenLabel(m.type, m.id, 1, l1, sizeof(l1));
      store.screenLabel(m.type, m.id, 2, l2, sizeof(l2));
      display.setTariffLabels(l1, l2);
      display.showMeter(m, current, cnt);
    } else {
      display.showSearching();
    }
  }

  meters.loop();   // сохранение в память
  clk.loop(wifi.staConnected());   // NTP, когда есть домашняя сеть
  history.loop();                  // запись истории раз в сутки
  alerts.loop();                   // тревоги и напоминания

#if SVT_BLE_NIMBLE
  // Перезапуск сканирования раз в 5 секунд: сбрасывает фильтр повторов (обновляются RSSI и возраст данных)
  // и очищает результаты, поэтому память под них не растёт
  static uint32_t lastScan = 0;
  if (millis() - lastScan > 5000UL) {
    lastScan = millis();
    NimBLEScan* s = NimBLEDevice::getScan();
    s->stop();
    s->clearResults();
    s->start(0, false, true);
  }
#endif

  // Аварийное восстановление AP и после запуска: в первые 30 секунд удерживать UP+DOWN 5 секунд.
  // Нужно для платы, где кнопка занята загрузчиком (BOOT на ESP32-T).
  static uint32_t bothSince = 0;
  static bool recovered = false;
  if (!recovered && millis() < 30000UL) {
    if (digitalRead(BTN_UP_PIN) == LOW && digitalRead(BTN_DOWN_PIN) == LOW) {
      if (bothSince == 0) bothSince = millis();
      else if (millis() - bothSince >= 5000UL) {
        recovered = true;
        wifi.factoryApReset();
      }
    } else {
      bothSince = 0;
    }
  }

  // Память раз в минуту: свободно сейчас и минимум с момента запуска
  static uint32_t lastHeap = 0;
  if (millis() - lastHeap > (millis() < 60000UL ? 10000UL : 60000UL)) {
    lastHeap = millis();
    SVT_SERIAL.printf("Heap: free=%u min=%u maxBlock=%u apClients=%d sta=%s\n", (unsigned)ESP.getFreeHeap(),
                      (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getMaxAllocHeap(), wifi.apClients(),
                      wifi.staConnected() ? "up" : "down");
  }
}
