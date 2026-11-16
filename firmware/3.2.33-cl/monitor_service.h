// Сервисное меню на экране: вход удержанием UP+DOWN 3 секунды.
// UP/DOWN: вверх/вниз по меню, UP+DOWN вместе (коротко): выбрать. Через 60 секунд без нажатий меню закрывается.
//   SERVICE: Wi-Fi (состояние домашней сети), AP (SSID, пароль, IP, сброс AP), Reset (сброс настроек, полный сброс), Exit
// Сброс настроек: удаляются настройки Wi-Fi, времени, тревог, Telegram, срока истории; счётчики и история сохраняются.
// Полный сброс: дополнительно удаляются список счётчиков и история. Прошивка и профиль платы не затрагиваются.
// Оба сброса с двойным подтверждением; после сброса плата перезагружается.
#pragma once
#include <Preferences.h>
#include <LittleFS.h>
#include "monitor_common.h"
#include "monitor_buttons.h"
#include "monitor_wifi.h"
#include "monitor_history.h"

class ServiceMenu {
 public:
  void begin(WifiManager* wifi) { wifi_ = wifi; }
  bool active() const { return active_; }
  void open() { active_ = true; go(S_MAIN, 0); }
  void close() { active_ = false; }

  void loop() {
    uint32_t now = millis();
    if (active_ && !restartAt_ && now - lastAct_ > 60000UL) close();
    if (restartAt_ && (int32_t)(now - restartAt_) >= 0) SVT_RESTART();
  }

  void onEvent(BtnEvent e) {
    lastAct_ = millis();
    if (restartAt_) return;
    switch (screen_) {
      case S_WIFI:
        if (e == BTN_BOTH) go(S_MAIN, 0);
        return;
      case S_TEXT:
        if (e == BTN_UP && scroll_ > 0) scroll_--;
        else if (e == BTN_DOWN) scroll_++;     // верхняя граница ограничивается в build()
        else if (e == BTN_BOTH) go(S_AP, textKind_);
        return;
      case S_MSG:
        go(S_MAIN, 0);
        return;
      default:
        break;
    }
    uint8_t n = itemCount();
    if (e == BTN_UP) sel_ = (sel_ + n - 1) % n;
    else if (e == BTN_DOWN) sel_ = (sel_ + 1) % n;
    else if (e == BTN_BOTH) select();
  }

  void build(MenuView& v) {
    v = MenuView();
    switch (screen_) {
      case S_MAIN:
        title(v, "SERVICE");
        add(v, "Wi-Fi"); add(v, "AP"); add(v, "Reset"); add(v, "Exit");
        v.sel = sel_;
        break;
      case S_WIFI: {
        WifiInfo w;
        wifi_->fillInfo(w);
        char b[28];
        title(v, "Wi-Fi");
        snprintf(b, sizeof(b), "Home: %s", w.enabled ? "ON" : "OFF");
        add(v, b);
        add(v, w.ssid[0] ? w.ssid : "-");
        if (w.connected) {
          snprintf(b, sizeof(b), "IP %s", w.ip);
          add(v, b);
          snprintf(b, sizeof(b), "RSSI %d", (int)w.rssi);
          add(v, b);
        } else {
          add(v, w.enabled ? w.state : "no link");
        }
        break;
      }
      case S_AP:
        title(v, "AP");
        add(v, "SSID"); add(v, "Password"); add(v, "IP"); add(v, "Reset AP"); add(v, "Back");
        v.sel = sel_;
        break;
      case S_TEXT: {
        static const char* const names[3] = {"SSID", "Password", "AP IP"};
        title(v, names[textKind_ < 3 ? textKind_ : 0]);
        String s = textKind_ == 0 ? wifi_->apSsid() : (textKind_ == 1 ? wifi_->apPass() : wifi_->apIp());
        int total = ((int)s.length() + 12) / 13;
        if (total < 1) total = 1;
        if (scroll_ >= total) scroll_ = total - 1;
        for (int k = scroll_; k < total && v.n < 8; k++) {
          char b[16];
          snprintf(b, sizeof(b), "%.13s", s.c_str() + k * 13);
          add(v, b);
        }
        break;
      }
      case S_RESETSEL:
        title(v, "RESET");
        add(v, "Settings reset"); add(v, "Full reset"); add(v, "Back");
        v.sel = sel_;
        break;
      case S_CONFIRM:
        title(v, "CONFIRM");
        if (action_ == A_RESET_AP) {
          add(v, "Reset AP to"); add(v, "factory?");
        } else if (action_ == A_SETTINGS) {
          if (step_ == 1) { add(v, "Reset"); add(v, "settings?"); }
          else { add(v, "Are you sure?"); add(v, "Meters kept"); }
        } else {
          if (step_ == 1) { add(v, "Full reset?"); }
          else { add(v, "DATA WILL BE"); add(v, "ERASED FOREVER"); }
        }
        {
          uint8_t m = v.n;
          add(v, "No");
          add(v, "Yes");
          v.sel = m + sel_;
        }
        break;
      case S_MSG:
        title(v, "SERVICE");
        add(v, msg1_);
        if (msg2_[0]) add(v, msg2_);
        break;
    }
  }

 private:
  enum Screen : uint8_t { S_MAIN, S_WIFI, S_AP, S_TEXT, S_RESETSEL, S_CONFIRM, S_MSG };
  enum Action : uint8_t { A_RESET_AP, A_SETTINGS, A_FULL };

  static void title(MenuView& v, const char* t) { snprintf(v.title, sizeof(v.title), "%s", t); }
  static void add(MenuView& v, const char* s) {
    if (v.n < 8) snprintf(v.line[v.n++], sizeof(v.line[0]), "%s", s);
  }

  void go(Screen s, uint8_t sel) {
    screen_ = s;
    sel_ = sel;
    scroll_ = 0;
    lastAct_ = millis();
  }

  uint8_t itemCount() const {
    switch (screen_) {
      case S_MAIN: return 4;
      case S_AP: return 5;
      case S_RESETSEL: return 3;
      case S_CONFIRM: return 2;
      default: return 1;
    }
  }

  void select() {
    switch (screen_) {
      case S_MAIN:
        if (sel_ == 0) go(S_WIFI, 0);
        else if (sel_ == 1) go(S_AP, 0);
        else if (sel_ == 2) go(S_RESETSEL, 0);
        else close();
        break;
      case S_AP:
        if (sel_ <= 2) { textKind_ = sel_; go(S_TEXT, 0); }
        else if (sel_ == 3) confirm(A_RESET_AP, S_AP, 3);
        else go(S_MAIN, 1);
        break;
      case S_RESETSEL:
        if (sel_ == 0) confirm(A_SETTINGS, S_RESETSEL, 0);
        else if (sel_ == 1) confirm(A_FULL, S_RESETSEL, 1);
        else go(S_MAIN, 2);
        break;
      case S_CONFIRM:
        if (sel_ == 0) { go(backScreen_, backSel_); break; }          // No
        if (action_ != A_RESET_AP && step_ == 1) { step_ = 2; sel_ = 0; break; }   // второй шаг
        perform();
        break;
      default:
        break;
    }
  }

  void confirm(Action a, Screen back, uint8_t backSel) {
    action_ = a;
    step_ = 1;
    backScreen_ = back;
    backSel_ = backSel;
    go(S_CONFIRM, 0);
  }

  static void clearNs(const char* ns) {
    Preferences p;
    if (p.begin(ns, false)) {
      p.clear();
      p.end();
    }
  }

  void perform() {
    if (action_ == A_RESET_AP) {
      wifi_->factoryApReset();
      snprintf(msg1_, sizeof(msg1_), "AP reset");
      msg2_[0] = 0;
      go(S_MSG, 0);
      return;
    }
    static const char* const settingsNs[] = {"svt15wifi", "svt15time", "svt15alm", "svt15hist", "svt15tg"};
    for (const char* ns : settingsNs) clearNs(ns);
    if (action_ == A_FULL) {
      clearNs("svt15");                                   // список счётчиков
      if (LittleFS.exists(SVT_HIST_FILE)) LittleFS.remove(SVT_HIST_FILE);   // история
    }
    SVT_SERIAL.println(action_ == A_FULL ? "SERVICE: full reset" : "SERVICE: settings reset");
    snprintf(msg1_, sizeof(msg1_), "Reset done");
    snprintf(msg2_, sizeof(msg2_), "Restarting...");
    go(S_MSG, 0);
    restartAt_ = millis() + 2000UL;
  }

  WifiManager* wifi_ = nullptr;
  bool active_ = false;
  Screen screen_ = S_MAIN;
  Screen backScreen_ = S_MAIN;
  uint8_t backSel_ = 0;
  uint8_t sel_ = 0;
  uint8_t scroll_ = 0;
  uint8_t textKind_ = 0;
  Action action_ = A_RESET_AP;
  uint8_t step_ = 1;
  char msg1_[28] = "";
  char msg2_[28] = "";
  uint32_t lastAct_ = 0;
  uint32_t restartAt_ = 0;
};
