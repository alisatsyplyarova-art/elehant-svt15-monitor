// Уведомления Telegram: тревоги и передача показаний по расписанию, настройки по каждому счётчику
// (monitor_mcfg.h): отправлять ли счётчик, как часто, какие данные, какие тревоги и как часто их повторять.
// Отправка по HTTPS (api.telegram.org), только при подключённой домашней сети, сообщения по одному.
// Токен бота в журнал и диагностику не выводится, в WEB показываются только его последние 4 символа.
// Проверка сертификата сервера по умолчанию отключена (SVT_TG_INSECURE 1): токен можно перехватить в недоверенной сети.
#pragma once
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>
#include "monitor_common.h"
#include "monitor_meters.h"
#include "monitor_time.h"
#include "monitor_alarms.h"
#include "monitor_wifi.h"
#include "monitor_mcfg.h"

#ifndef SVT_TG_INSECURE
  #define SVT_TG_INSECURE 1
#endif
#define SVT_TG_Q 6                       // сообщений в очереди
#define SVT_TG_MSG 420                   // байт на сообщение
#define SVT_TG_MIN_BLOCK 45000UL         // нужный свободный блок памяти для TLS

struct TelegramSettings {
  bool enabled = false;                  // общее включение
  String token;
  String chat;
  bool alarms = true;                    // общее включение уведомлений о тревогах
  String botName;                        // имя бота (определяется по токену), для QR-кода
};

class TelegramManager {
 public:
  TelegramSettings cfg;

  void begin(MeterManager* m, AlertManager* a, TimeManager* c, WifiManager* w, MeterCfgStore* s) {
    meters_ = m;
    alerts_ = a;
    clk_ = c;
    wifi_ = w;
    store_ = s;
    load();
  }

  void load() {
    Preferences p;
    if (!p.begin("svt15tg", true)) return;
    cfg.enabled = p.getBool("en", false);
    cfg.token = p.getString("token", "");
    cfg.chat = p.getString("chat", "");
    cfg.alarms = p.getBool("alarms", true);
    cfg.botName = p.getString("bot", "");
    p.end();
  }

  void save() {
    Preferences p;
    if (!p.begin("svt15tg", false)) return;
    p.putBool("en", cfg.enabled);
    p.putString("token", cfg.token);
    p.putString("chat", cfg.chat);
    p.putBool("alarms", cfg.alarms);
    p.putString("bot", cfg.botName);
    p.end();
  }

  bool configured() const { return validToken(cfg.token) && validChat(cfg.chat); }

  // Проверка токена (цифры:символы) и идентификатора чата (число со знаком или @имя)
  static bool validToken(const String& t) {
    if (t.length() < 20 || t.length() > 90) return false;
    bool colon = false;
    for (size_t i = 0; i < t.length(); i++) {
      char c = t[i];
      if (c == ':') colon = true;
      else if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '-')) return false;
    }
    return colon;
  }
  static bool validChat(const String& c) {
    if (c.length() < 1 || c.length() > 40) return false;
    size_t i = 0;
    if (c[0] == '@') {
      for (i = 1; i < c.length(); i++) {
        char x = c[i];
        if (!((x >= '0' && x <= '9') || (x >= 'A' && x <= 'Z') || (x >= 'a' && x <= 'z') || x == '_')) return false;
      }
      return c.length() > 1;
    }
    if (c[0] == '-') i = 1;
    if (i >= c.length()) return false;
    for (; i < c.length(); i++) if (!(c[i] >= '0' && c[i] <= '9')) return false;
    return true;
  }

  // Последние 4 символа токена для показа
  String tokenTail() const {
    String t;
    size_t n = cfg.token.length();
    for (size_t i = (n > 4 ? n - 4 : 0); i < n; i++) t += cfg.token[i];
    return t;
  }

  // Ссылка на бота для QR-кода: https://t.me/<имя>
  bool botLink(String& out) const {
    if (cfg.botName.length() == 0) return false;
    out = "https://t.me/";
    out += cfg.botName;
    return true;
  }

  bool sendTest() {
    char b[96];
    snprintf(b, sizeof(b), "Тест: связь работает (Monitor Elehant %s)", SVT15_LCD_VERSION);
    return enqueue(b);
  }

  // Показания одного счётчика сейчас (кнопка на странице счётчика)
  bool sendMeterNow(uint8_t idx) { return enqueueReadings(idx, false); }

  // Показания всех счётчиков, у которых включена отправка (кнопка на странице Telegram)
  void sendAllNow() { nowIdx_ = 0; }

  // Определить имя бота по токену: запрос getMe; в result текст для страницы
  bool checkToken(String& result) {
    if (!validToken(cfg.token)) { result = "токен не задан или неверного вида"; return false; }
    if (!wifi_->staConnected()) { result = "нет подключения к домашней сети"; return false; }
    if (ESP.getMaxAllocHeap() < SVT_TG_MIN_BLOCK) { result = "мало памяти, повторите позже"; return false; }
    WiFiClientSecure client;
#if SVT_TG_INSECURE
    client.setInsecure();
#endif
    HTTPClient http;
    String url = "https://api.telegram.org/bot";
    url += cfg.token;
    url += "/getMe";
    http.setConnectTimeout(5000);
    http.setTimeout(8000);
    if (!http.begin(client, url)) { result = "ошибка соединения"; return false; }
    int code = http.GET();
    String body;
    if (code == 200) body = http.getString();
    http.end();
    lastCode_ = code;
    if (code != 200) {
      result = code == 401 ? "токен не принят Telegram (код 401)" : "ошибка Telegram, код ";
      if (code != 401) result += String(code);
      return false;
    }
    int p = body.indexOf("\"username\":\"");
    if (p < 0) { result = "в ответе нет имени бота"; return false; }
    p += 12;
    int e = body.indexOf('"', p);
    if (e < 0 || e - p < 3 || e - p > 32) { result = "в ответе неверное имя бота"; return false; }
    String u = body.substring(p, e);
    for (size_t i = 0; i < u.length(); i++) {
      char c = u[i];
      if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')) { result = "в ответе неверное имя бота"; return false; }
    }
    cfg.botName = u;
    save();
    result = "бот найден: @";
    result += u;
    return true;
  }

  // Определить ID чата по последнему сообщению боту (getUpdates).
  // Перед этим отправьте боту в Telegram любое сообщение. Берётся последний чат из ответа.
  bool detectChat(String& result) {
    if (!validToken(cfg.token)) { result = "сначала сохраните токен бота"; return false; }
    if (!wifi_->staConnected()) { result = "нет подключения к домашней сети"; return false; }
    if (ESP.getMaxAllocHeap() < SVT_TG_MIN_BLOCK) { result = "мало памяти, повторите позже"; return false; }
    WiFiClientSecure client;
#if SVT_TG_INSECURE
    client.setInsecure();
#endif
    HTTPClient http;
    String url = "https://api.telegram.org/bot";
    url += cfg.token;
    url += "/getUpdates?limit=100";
    http.setConnectTimeout(5000);
    http.setTimeout(8000);
    if (!http.begin(client, url)) { result = "ошибка соединения"; return false; }
    int code = http.GET();
    String body;
    if (code == 200) body = http.getString();
    http.end();
    lastCode_ = code;
    if (code == 401) { result = "токен не принят Telegram (код 401)"; return false; }
    if (code == 409) { result = "у бота включён webhook: ID чата введите вручную"; return false; }
    if (code != 200) { result = "ошибка Telegram, код "; result += String(code); return false; }

    const char* key = "\"chat\":{\"id\":";
    int last = -1;
    for (int p = body.indexOf(key); p >= 0; p = body.indexOf(key, p + 1)) last = p;
    if (last < 0) {
      result = "сообщений боту пока нет: откройте бота в Telegram, отправьте ему любое сообщение и повторите";
      return false;
    }
    int q = last + (int)strlen(key);
    String id;
    if (q < (int)body.length() && body[q] == '-') { id += '-'; q++; }
    while (q < (int)body.length() && body[q] >= '0' && body[q] <= '9' && id.length() < 20) { id += body[q]; q++; }
    if (!validChat(id)) { result = "не удалось разобрать ID чата"; return false; }
    cfg.chat = id;
    save();
    result = "ID чата найден: ";
    result += id;
    return true;
  }

  uint8_t queued() const { return qn_; }
  const char* status() const { return status_; }
  int lastCode() const { return lastCode_; }
  uint32_t lastOkAgoMin() const { return lastOkMs_ ? (millis() - lastOkMs_) / 60000UL : 0; }
  bool everOk() const { return lastOkMs_ != 0; }

  // Из loop(): формирование сообщений и отправка по одному
  void loop() {
    uint32_t now = millis();

    AlarmItem it;
    while (alerts_->popNewAlarm(it)) {   // события читаем всегда, чтобы не копились
      if (alarmAllowed(it.meter, it.type, now) && enqueueAlarm(it)) lastAlarm_[it.meter][it.type] = now ? now : 1;
    }
    if (!cfg.enabled) return;

    // повтор уведомления о незакрытой тревоге
    if (now - lastScan_ >= 10000UL) {
      lastScan_ = now;
      scheduleReadings();
      repeatAlarms(now);
    }

    // показания всех счётчиков по кнопке
    while (nowIdx_ != 255 && qn_ < SVT_TG_Q) {
      if (nowIdx_ >= meters_->count()) { nowIdx_ = 255; break; }
      MeterData m;
      if (meters_->get(nowIdx_, m) && store_->at(m.type, m.id).tgOn) enqueueReadings(nowIdx_, false);
      nowIdx_++;
    }

    if (qn_ > 0 && now >= nextTry_) trySend(now);
  }

 private:
  bool alarmAllowed(uint8_t meter, uint8_t type, uint32_t now) {
    if (!cfg.enabled || !cfg.alarms || alerts_->suppressed(type, now)) return false;
    MeterData m;
    if (!meters_->get(meter, m)) return false;
    const MeterCfg& c = store_->at(m.type, m.id);
    return c.tgOn && (c.tgAlarms & (1u << type));
  }

  // Передача показаний по расписанию счётчика: не чаще раза в сутки
  void scheduleReadings() {
    if (!clk_->valid()) return;
    time_t l = clk_->local();
    struct tm t;
    gmtime_r(&l, &t);
    uint32_t key = (uint32_t)(t.tm_year + 1900) * 10000UL + (uint32_t)(t.tm_mon + 1) * 100UL + (uint32_t)t.tm_mday;
    bool changed = false;
    uint8_t n = meters_->count();
    for (uint8_t i = 0; i < n; i++) {
      MeterData m;
      if (!meters_->get(i, m) || !m.valid) continue;
      MeterCfg& c = store_->at(m.type, m.id);
      if (!c.tgOn || c.tgMode == SVT_TGM_OFF || c.tgLast == key || t.tm_hour < c.tgHour) continue;
      bool day = c.tgMode == SVT_TGM_DAILY ||
                 (c.tgMode == SVT_TGM_WEEKLY && t.tm_wday == c.tgDay) ||
                 (c.tgMode == SVT_TGM_MONTHLY && t.tm_mday == c.tgDay);
      if (!day) continue;
      if (enqueueReadings(i, c.tgMode == SVT_TGM_MONTHLY)) {
        c.tgLast = key;
        changed = true;
      }
    }
    if (changed) store_->save();
  }

  void repeatAlarms(uint32_t now) {
    uint8_t n = meters_->count();
    for (uint8_t i = 0; i < n; i++) {
      MeterData m;
      if (!meters_->get(i, m)) continue;
      const MeterCfg& c = store_->at(m.type, m.id);
      for (uint8_t t = 0; t < AL_COUNT; t++) {
        const AlarmState& s = alerts_->state(i, t);
        if (!s.active) { lastAlarm_[i][t] = 0; continue; }
        if (s.acked || c.tgRepeatH == 0 || lastAlarm_[i][t] == 0) continue;
        if (now - lastAlarm_[i][t] >= (uint32_t)c.tgRepeatH * 3600000UL && alarmAllowed(i, t, now)) {
          if (enqueueAlarm(AlarmItem{1, i, t})) lastAlarm_[i][t] = now ? now : 1;
        }
      }
    }
  }

  static void jsonEsc(const char* s, String& out) {
    for (; *s; s++) {
      char c = *s;
      if (c == '"' || c == '\\') { out += '\\'; out += c; }
      else if (c == '\n') out += "\\n";
      else if ((uint8_t)c >= 0x20) out += c;   // байты UTF-8 передаются как есть
    }
  }

  bool enqueue(const char* text) {
    if (!cfg.enabled || !configured()) { snprintf(status_, sizeof(status_), "not configured"); return false; }
    if (qn_ >= SVT_TG_Q) return false;
    snprintf(q_[(qh_ + qn_) % SVT_TG_Q], SVT_TG_MSG, "%s", text);
    qn_++;
    return true;
  }

  static const char* unit(uint8_t type) { return type == 4 ? "ГДж" : "м³"; }
  static const char* typeName(uint8_t type) { return type == 1 ? "Газ" : (type == 4 ? "Тепло" : "Вода"); }

  bool enqueueAlarm(const AlarmItem& it) {
    MeterData m;
    if (!meters_->get(it.meter, m)) return false;
    char rs[24], b[SVT_TG_MSG];
    alerts_->reasonText(it.meter, it.type, rs, sizeof(rs));
    snprintf(b, sizeof(b), "ТРЕВОГА: %s\nСчётчик: %c%lu (%s)\nПричина: %s", SVT_ALARM_NAME[it.type],
             svt15TypeChar(m.type), (unsigned long)m.id, typeName(m.type), rs);
    return enqueue(b);
  }

  // Сообщение с показаниями: набор данных задан настройками счётчика
  bool enqueueReadings(uint8_t idx, bool monthly) {
    MeterData m;
    if (!meters_->get(idx, m) || !m.valid) return false;
    MeterCfg& c = store_->at(m.type, m.id);
    char b[SVT_TG_MSG];
    int n = snprintf(b, sizeof(b), "%s\nSN %lu (%s)", monthly ? "ПЕРЕДАТЬ ПОКАЗАНИЯ" : "ПОКАЗАНИЯ",
                     (unsigned long)m.id, typeName(m.type));
    // дата и время снятия показаний (местное время); без установленного времени — прочерк
    if (clk_->valid()) {
      char dt[24];
      TimeManager::fmtDateTime(clk_->local(), dt, sizeof(dt));
      n += snprintf(b + n, sizeof(b) - n, "\nДата: %s", dt);
    } else {
      n += snprintf(b + n, sizeof(b) - n, "\nДата: не установлена");
    }
    auto add = [&](const char* fmt, const char* a1, const char* a2) {
      if (n > 0 && n < (int)sizeof(b)) n += snprintf(b + n, sizeof(b) - n, fmt, a1, a2);
    };
    char v[24], x[24];
    if ((c.tgFields & SVT_TGF_T1) && m.hasT1) {
      svt15FmtVol(v, sizeof(v), true, m.t1, m.dec);
      add("\n%s: %s", store_->name(m.type, m.id, 1, m.twoTariff), v);
      add(" %s%s", unit(m.type), "");
    }
    if ((c.tgFields & SVT_TGF_T2) && m.twoTariff && m.hasT2) {
      svt15FmtVol(v, sizeof(v), true, m.t2, m.dec);
      add("\n%s: %s", store_->name(m.type, m.id, 2, true), v);
      add(" %s%s", unit(m.type), "");
    }
    if (m.twoTariff && m.hasT1 && m.hasT2) {
      snprintf(v, sizeof(v), "%.*f", m.dec, m.t1 + m.t2);
      add("\nОБ: %s", v, "");
      add(" %s%s", unit(m.type), "");
    }
    if ((c.tgFields & SVT_TGF_TEMP) && m.hasTemp) { snprintf(x, sizeof(x), "%.1f", m.temp); add("\nТемпература: %s °C%s", x, ""); }
    if (c.tgFields & SVT_TGF_BAT) { snprintf(x, sizeof(x), "%u", m.battery); add("\nБатарея: %s %%%s", x, ""); }
    if (c.tgFields & SVT_TGF_RSSI) { snprintf(x, sizeof(x), "%d", m.rssi); add("\nRSSI: %s dBm%s", x, ""); }
    if (c.tgFields & SVT_TGF_FW) { snprintf(x, sizeof(x), "%.1f", m.fw); add("\nПрошивка счётчика: %s%s", x, ""); }
    return enqueue(b);
  }

  void trySend(uint32_t now) {
    if (!wifi_->staConnected()) {
      snprintf(status_, sizeof(status_), "no home Wi-Fi");
      nextTry_ = now + 15000UL;
      return;
    }
    if (ESP.getMaxAllocHeap() < SVT_TG_MIN_BLOCK) {
      snprintf(status_, sizeof(status_), "low memory");
      nextTry_ = now + 30000UL;
      return;
    }
    if (post(q_[qh_])) {
      qh_ = (qh_ + 1) % SVT_TG_Q;
      qn_--;
      fails_ = 0;
      lastOkMs_ = now ? now : 1;
      snprintf(status_, sizeof(status_), "ok");
      nextTry_ = now + 1500UL;               // пауза между сообщениями
      SVT_SERIAL.println("TG: sent");
    } else {
      snprintf(status_, sizeof(status_), "error %d", lastCode_);
      SVT_SERIAL.printf("TG: send failed, code %d\n", lastCode_);
      if (++fails_ >= 3) {                   // сообщение отбрасывается после трёх неудач
        qh_ = (qh_ + 1) % SVT_TG_Q;
        qn_--;
        fails_ = 0;
        SVT_SERIAL.println("TG: message dropped");
      }
      nextTry_ = now + 60000UL;
    }
  }

  bool post(const char* text) {
    WiFiClientSecure client;
#if SVT_TG_INSECURE
    client.setInsecure();
#endif
    HTTPClient http;
    String url = "https://api.telegram.org/bot";
    url += cfg.token;
    url += "/sendMessage";
    http.setConnectTimeout(5000);
    http.setTimeout(8000);
    if (!http.begin(client, url)) {
      lastCode_ = -1;
      return false;
    }
    http.addHeader("Content-Type", "application/json");
    String body = "{\"chat_id\":\"";
    jsonEsc(cfg.chat.c_str(), body);
    body += "\",\"text\":\"";
    jsonEsc(text, body);
    body += "\"}";
    int code = http.POST(body);
    http.end();
    lastCode_ = code;
    return code == 200;
  }

  MeterManager* meters_ = nullptr;
  AlertManager* alerts_ = nullptr;
  TimeManager* clk_ = nullptr;
  WifiManager* wifi_ = nullptr;
  MeterCfgStore* store_ = nullptr;
  char q_[SVT_TG_Q][SVT_TG_MSG];
  uint8_t qh_ = 0, qn_ = 0;
  uint8_t nowIdx_ = 255;                 // 255: передача по кнопке не идёт
  uint32_t lastAlarm_[MAX_METERS][AL_COUNT] = {};   // когда отправлено уведомление о тревоге (0: не отправлялось)
  uint32_t lastScan_ = 0;
  uint32_t nextTry_ = 0;
  uint8_t fails_ = 0;
  int lastCode_ = 0;
  uint32_t lastOkMs_ = 0;
  char status_[24] = "idle";
};
