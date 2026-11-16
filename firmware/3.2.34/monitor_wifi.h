// Wi-Fi: точка доступа (AP) и домашний Wi-Fi (STA) одновременно.
// Параметры хранятся в Preferences. Заводской AP: SSID Monitor-Elehant, пароль 88118811, IP 192.168.4.1
#pragma once
#include <WiFi.h>
#include <Preferences.h>
#include "monitor_profile.h"
#ifndef SVT_WDT_WIFI_MIN
#define SVT_WDT_WIFI_MIN 30   // запасное значение, если модуль собирают без главного файла
#endif
#include "monitor_common.h"

#define SVT_AP_DEF_SSID "Monitor-Elehant"
#define SVT_AP_DEF_PASS "88118811"
#define SVT_STA_ATTEMPT_MS 25000UL   // сколько ждать подключения к домашней сети в одной попытке

// Аварийное восстановление AP: UP+DOWN удерживаются при включении 5 секунд.
// Вызывать в начале setup() после pinMode кнопок.
static inline bool svtApRecoveryRequested() {
  if (digitalRead(BTN_UP_PIN) != LOW || digitalRead(BTN_DOWN_PIN) != LOW) return false;
  uint32_t t0 = millis();
  while (digitalRead(BTN_UP_PIN) == LOW && digitalRead(BTN_DOWN_PIN) == LOW) {
    if (millis() - t0 >= 5000) return true;
    delay(50);
  }
  return false;
}

class WifiManager {
 public:
  // resetAp = true: вернуть заводские SSID и пароль AP (домашняя сеть не трогается)
  void begin(bool resetAp) {
    Preferences p;
    if (p.begin("svt15wifi", false)) {
      if (resetAp) {
        p.remove("ap_ssid");
        p.remove("ap_pass");
      }
      apSsid_ = p.getString("ap_ssid", SVT_AP_DEF_SSID);
      apPass_ = p.getString("ap_pass", SVT_AP_DEF_PASS);
      staSsid_ = p.getString("sta_ssid", "");
      staPass_ = p.getString("sta_pass", "");
      staEnabled_ = p.getBool("sta_en", false);
      p.end();
    } else {
      apSsid_ = SVT_AP_DEF_SSID;
      apPass_ = SVT_AP_DEF_PASS;
    }
    // Безопасный режим: только точка доступа (UP+DOWN при включении или SVT_FORCE_STA_OFF)
    if (SVT_FORCE_STA_OFF || resetAp) {
      if (staEnabled_) SVT_SERIAL.println("STA: disabled for this run (safe mode)");
      staEnabled_ = false;
    }
    // События Wi-Fi только запоминают причину, печать в loop()
    WiFi.onEvent([this](arduino_event_id_t event, arduino_event_info_t info) {
      if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        uint16_t r = info.wifi_sta_disconnected.reason;
        if (r != 8) { reason_ = r; evFlag_ = 1; }   // 8 — отключение, вызванное нами
      } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        reason_ = 0;
        evFlag_ = 2;
      } else if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
        apEv_ = 1;
      } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
        apEv_ = 2;
      }
    });
    WiFi.mode(WIFI_AP_STA);   // AP и STA одновременно
    startAp();
    connectSta();
  }

  // Сброс AP к заводским и безопасный режим на этот запуск (вызывается из loop())
  void factoryApReset() {
    Preferences p;
    if (p.begin("svt15wifi", false)) {
      p.remove("ap_ssid");
      p.remove("ap_pass");
      p.end();
    }
    apSsid_ = SVT_AP_DEF_SSID;
    apPass_ = SVT_AP_DEF_PASS;
    if (staEnabled_) {
      staEnabled_ = false;       // значение в памяти не меняется
      trying_ = false;
      WiFi.disconnect(false);
    }
    startAp();
    SVT_SERIAL.println("AP parameters reset to factory (home Wi-Fi off for this run)");
  }

  // Применить сохранённые параметры AP
  void startAp() {
    WiFi.softAP(apSsid_.c_str(), apPass_.c_str(), 1, 0, SVT_AP_MAX_CLIENTS);
  }

  // Сохранить новые параметры AP (применяются отдельно через startAp())
  void saveAp(const String& ssid, const String& pass) {
    apSsid_ = ssid;
    apPass_ = pass;
    Preferences p;
    if (p.begin("svt15wifi", false)) {
      p.putString("ap_ssid", ssid);
      p.putString("ap_pass", pass);
      p.end();
    }
  }

  // Сохранить параметры домашней сети (применяются через connectSta())
  void saveSta(const String& ssid, const String& pass, bool enabled) {
    staSsid_ = ssid;
    staPass_ = pass;
    staEnabled_ = enabled;
    Preferences p;
    if (p.begin("svt15wifi", false)) {
      p.putString("sta_ssid", ssid);
      p.putString("sta_pass", pass);
      p.putBool("sta_en", enabled);
      p.end();
    }
  }

  // Подключиться к домашней сети (или отключиться, если выключено).
  // Вызывается при старте и после сохранения настроек: счётчик неудач сбрасывается.
  void connectSta() {
    fails_ = 0;
    lastStaOkMs_ = millis();      // сторож отсчитывает от момента новой попытки
    startAttempt();
  }

  // Из loop(): печать событий в Serial0 и повтор подключения, если сеть пропала
  void loop() {
    // Сторож Wi-Fi: домашняя сеть настроена, но не подключается дольше SVT_WDT_WIFI_MIN — перезапуск
    uint32_t nowW = millis();
    if (staEnabled_ && staSsid_.length() > 0 && staConnected()) lastStaOkMs_ = nowW;
    if (SVT_WDT_WIFI_MIN > 0 && staEnabled_ && staSsid_.length() > 0 && !staConnected() &&
        nowW - lastStaOkMs_ > (uint32_t)SVT_WDT_WIFI_MIN * 60000UL) {
      SVT_SERIAL.printf("WDT: home Wi-Fi not connected for %u min, restarting\n", (unsigned)SVT_WDT_WIFI_MIN);
      SVT_RESTART();
    }
    if (evFlag_) {
      uint8_t f = evFlag_;
      evFlag_ = 0;
      if (f == 2) {
        SVT_SERIAL.printf("STA: connected to %s, IP %s, RSSI %d, channel %d (AP uses the same channel)\n",
                          staSsid_.c_str(), staIp().c_str(), staRssi(), (int)WiFi.channel());
      } else {
        SVT_SERIAL.printf("STA: not connected, reason %u (%s)\n", (unsigned)reason_, reasonShort(reason_));
      }
    }
    if (apEv_) {
      uint8_t a = apEv_;
      apEv_ = 0;
      SVT_SERIAL.printf("AP: client %s, clients now %d\n", a == 1 ? "connected" : "disconnected", apClients());
    }

    // Повторы с паузами: пока STA ищет недоступную сеть, она перебирает каналы и
    // точка доступа (и WEB) работает нестабильно, поэтому после неудачи поиск
    // останавливается и следующая попытка откладывается (30 с, 1, 2, затем 5 минут).
    if (!(staEnabled_ && staSsid_.length() > 0)) {
      trying_ = false;
      return;
    }
    uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED) {
      if (trying_) { trying_ = false; fails_ = 0; }
      return;
    }
    if (trying_) {
      if (now - attemptStart_ > SVT_STA_ATTEMPT_MS) {
        WiFi.disconnect(false);   // остановить поиск сети
        trying_ = false;
        if (fails_ < 250) fails_++;
        uint32_t wait = backoffMs(fails_);
        nextTry_ = now + wait;
        SVT_SERIAL.printf("STA: attempt %u failed, next try in %lu s\n", (unsigned)fails_, (unsigned long)(wait / 1000UL));
      }
    } else if ((int32_t)(now - nextTry_) >= 0) {
      // Поиск сети нарушает работу точки доступа: пока к ней кто-то подключён, автоповтор ждёт
      if (apClients() > 0) {
        if (!waitNoted_) {
          waitNoted_ = true;
          SVT_SERIAL.println("STA: retry postponed while AP has clients");
        }
      } else {
        waitNoted_ = false;
        startAttempt();
      }
    }
  }

  const String& apSsid() const { return apSsid_; }
  const String& apPass() const { return apPass_; }
  String apIp() { return WiFi.softAPIP().toString(); }

  const String& staSsid() const { return staSsid_; }
  const String& staPass() const { return staPass_; }
  bool staEnabled() const { return staEnabled_; }
  bool staConnected() { return WiFi.status() == WL_CONNECTED; }
  String staIp() { return WiFi.localIP().toString(); }
  int staRssi() { return WiFi.RSSI(); }
  int apClients() { return WiFi.softAPgetStationNum(); }   // подключено к точке доступа

  // Причина последнего отказа: короткая (для дисплея) и русская (для WEB)
  static const char* reasonShort(uint16_t r) {
    switch (r) {
      case 0:   return "wait";
      case 201: return "no net";
      case 2: case 15: case 202: case 204: return "bad pass";
      case 203: case 205: return "refused";
      case 200: return "lost";
      default:  return "fail";
    }
  }
  String staReasonText() const {
    uint16_t r = reason_;
    String s;
    switch (r) {
      case 0:   return String("идёт подключение");
      case 201: s = "сеть не найдена (проверьте название; ESP32 видит только 2.4 ГГц)"; break;
      case 2: case 15: case 202: case 204: s = "ошибка пароля или защиты сети"; break;
      case 203: case 205: s = "точка доступа отклонила подключение"; break;
      case 200: s = "потеряна связь с точкой доступа"; break;
      default:  s = "не удалось подключиться"; break;
    }
    s += ", код ";
    s += String((int)r);
    return s;
  }

  // Заполнить состояние для дисплея
  void fillInfo(WifiInfo& w) {
    w.enabled = staEnabled_ && staSsid_.length() > 0;
    w.connected = w.enabled && staConnected();
    strncpy(w.ssid, staSsid_.c_str(), sizeof(w.ssid) - 1);
    w.ssid[sizeof(w.ssid) - 1] = 0;
    w.ip[0] = 0;
    w.rssi = 0;
    if (w.connected) {
      strncpy(w.ip, staIp().c_str(), sizeof(w.ip) - 1);
      w.ip[sizeof(w.ip) - 1] = 0;
      w.rssi = (int8_t)staRssi();
    }
    strncpy(w.state, reasonShort(reason_), sizeof(w.state) - 1);
    w.state[sizeof(w.state) - 1] = 0;
  }

 private:
  String apSsid_, apPass_;
  String staSsid_, staPass_;
  bool staEnabled_ = false;
  uint32_t lastStaOkMs_ = 0;      // последний момент, когда домашняя сеть была подключена (для сторожа)
  bool trying_ = false;          // идёт попытка подключения к домашней сети
  uint8_t fails_ = 0;            // подряд неудачных попыток
  uint32_t attemptStart_ = 0;
  uint32_t nextTry_ = 0;

  static uint32_t backoffMs(uint8_t fails) {
    return fails <= 1 ? 30000UL : (fails == 2 ? 60000UL : (fails == 3 ? 120000UL : 300000UL));
  }

  void startAttempt() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.setAutoReconnect(false);   // повторами управляем сами
    WiFi.disconnect(false);
    trying_ = false;
    if (staEnabled_ && staSsid_.length() > 0) {
      WiFi.begin(staSsid_.c_str(), staPass_.length() ? staPass_.c_str() : nullptr);
      trying_ = true;
      attemptStart_ = millis();
    }
  }
  volatile uint16_t reason_ = 0;   // последняя причина отказа STA (0 — нет)
  volatile uint8_t evFlag_ = 0;    // 1 — отключение, 2 — получен IP
  volatile uint8_t apEv_ = 0;      // 1 — клиент подключился к AP, 2 — отключился
  bool waitNoted_ = false;
};
