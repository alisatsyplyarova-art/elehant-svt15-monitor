// WEB-интерфейс (встроенный WebServer, обработка в loop()):
//   /            таблица счётчиков (обновляется раз в 5 сек)
//   /api/meters  те же данные в JSON
//   /wifi        точка доступа и домашний Wi-Fi
//   /api/scan    поиск Wi-Fi сетей (асинхронно)
#pragma once
#include <WebServer.h>
#include "monitor_common.h"
#include "monitor_meters.h"
#include "monitor_wifi.h"
#include "monitor_time.h"
#include "monitor_history.h"
#include "monitor_alarms.h"
#include "monitor_mcfg.h"
#include "monitor_telegram.h"
#include "monitor_qrjs.h"

static const char SVT_PAGE_STYLE[] = R"rawliteral(
<style>
:root{color-scheme:light dark;--bg:#fff;--fg:#1a1a1a;--mut:#666;--line:#ddd;--ok:#1a7f37;--warn:#9a6700;--bad:#cf222e;--acc:#1a3a6b;--chip:#eef2f8}
@media(prefers-color-scheme:dark){:root{--bg:#121212;--fg:#eaeaea;--mut:#9a9a9a;--line:#333;--ok:#3fb950;--warn:#d29922;--bad:#f85149;--acc:#8ab4f8;--chip:#1e2530}}
*{box-sizing:border-box}
body{margin:0;padding:10px 12px 28px;font:16px/1.45 system-ui,-apple-system,"Segoe UI",sans-serif;background:var(--bg);color:var(--fg);-webkit-text-size-adjust:100%}
h1{font-size:20px;margin:6px 0 8px}
h2{font-size:17px;margin:18px 0 8px;color:var(--acc)}
nav{display:flex;gap:8px;overflow-x:auto;-webkit-overflow-scrolling:touch;padding:6px 0 10px;margin:0 -12px 6px;padding-left:12px;padding-right:12px;position:sticky;top:0;background:var(--bg);z-index:5;border-bottom:1px solid var(--line)}
nav a{flex:0 0 auto;padding:9px 14px;border-radius:18px;background:var(--chip);color:var(--fg);text-decoration:none;font-size:14px;white-space:nowrap}
.mut{color:var(--mut);font-size:13px;margin:8px 0}
.wrap{overflow-x:auto;-webkit-overflow-scrolling:touch;margin:0 -4px}
table{border-collapse:collapse;width:100%;font-size:15px}
th,td{padding:9px 7px;border-bottom:1px solid var(--line);text-align:left;white-space:nowrap}
th{font-size:13px;color:var(--mut);font-weight:600}
/* Три тарифные ячейки равной ширины и увеличенного размера */
#t th:nth-child(3),#t th:nth-child(4),#t th:nth-child(5),#t td.tariff{width:12%;min-width:112px;text-align:center;font-size:16px;font-weight:700}
#t td.tariff1{color:#e58a32}#t td.tariff2{color:#18a9c5}#t td.tariff3{color:#b56bd6}
.l0{color:var(--ok);font-weight:600}.l1{color:var(--warn);font-weight:600}.l2{color:var(--bad);font-weight:600}
label{display:block;margin:10px 0 4px;font-size:15px}
input:not([type=checkbox]):not([type=radio]),select{font:inherit;width:100%;padding:12px;border:1px solid var(--line);border-radius:10px;background:var(--bg);color:var(--fg);margin:0 0 10px;min-height:44px}
input[type=checkbox],input[type=radio]{-webkit-appearance:checkbox;appearance:checkbox;width:22px;height:22px;margin:0 10px 0 0;vertical-align:middle;accent-color:var(--acc);cursor:pointer;flex:0 0 auto}
input[type=radio]{-webkit-appearance:radio;appearance:radio}
label input[type=checkbox]{display:inline-block}
button{font:inherit;font-size:16px;width:100%;min-height:48px;padding:12px 16px;margin:8px 0;border:0;border-radius:12px;background:var(--acc);color:#fff;font-weight:600}
button:active{opacity:.8}
button[disabled]{opacity:.5}
form{margin:0}
.box{max-width:560px;width:100%;margin:0 auto}
p{margin:8px 0}
a{color:var(--acc)}
@media(min-width:700px){button{width:auto;padding:12px 22px}input:not([type=checkbox]):not([type=radio]),select{max-width:420px}}
@media(max-width:420px){body{font-size:15px}h1{font-size:18px}nav a{padding:8px 11px;font-size:13px}th,td{padding:7px 5px;font-size:14px}}
</style>
)rawliteral";

// Меню страниц и скрипт автосинхронизации времени с браузером
#define SVT_NAV_HTML \
  "<nav><a href=\"/\">Счётчики</a><a href=\"/alarms\">Тревоги</a><a href=\"/history\">История</a><a href=\"/time\">Время</a>" \
  "<a href=\"/memory\">Память</a><a href=\"/meters\">Настройки</a><a href=\"/telegram\">Telegram</a><a href=\"/wifi\">Wi-Fi</a></nav>"

// Если время на плате не установлено или расходится больше чем на 60 минут, браузер передаёт своё
#define SVT_SYNC_JS \
  "(async()=>{try{const r=await(await fetch('/api/time')).json();const n=Math.floor(Date.now()/1000);" \
  "if(!r.valid||Math.abs(r.epoch-n)>3600){await fetch('/api/time',{method:'POST'," \
  "headers:{'Content-Type':'application/x-www-form-urlencoded'}," \
  "body:'epoch='+n+'&tz='+(-new Date().getTimezoneOffset())});}}catch(e){}})();"

static const char SVT_PAGE_MAIN[] = R"rawliteral(<!doctype html>
<html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Monitor Elehant</title>
)rawliteral";

static const char SVT_PAGE_MAIN_BODY[] = R"rawliteral(</head><body>
<h1>Monitor Elehant</h1>
<nav><a href="/">Счётчики</a><a href="/alarms">Тревоги</a><a href="/history">История</a><a href="/time">Время</a><a href="/memory">Память</a><a href="/meters">Настройки</a><a href="/telegram">Telegram</a><a href="/wifi">Wi-Fi</a></nav>
<div class="mut" id="info">Загрузка...</div>
<div class="wrap"><table id="t"><thead><tr>
<th>Тип</th><th>SN</th><th>Тариф 1</th><th>Тариф 2</th><th>ОБ</th><th>Батарея</th><th>Темп.</th><th>RSSI</th><th>Качество</th><th>Получено</th><th>Статус</th>
</tr></thead><tbody></tbody></table></div>
<script>
const TN={1:'Газ',2:'Вода',4:'Тепло'};
const ST={'OK':'Норма','LOW BAT':'Батарея','NO DATA':'Нет данных','WEAK BLE':'Слабый сигнал'};
function age(s){if(s<60)return s+' с';if(s<3600)return Math.floor(s/60)+' мин';if(s<86400)return Math.floor(s/3600)+' ч';return Math.floor(s/86400)+' д';}
function vol(x,u){return x===null?'—':x+' '+u;}
async function load(){
 try{
  const d=await (await fetch('/api/meters')).json();
  document.getElementById('info').textContent='Версия '+d.ver+' · свободно ОЗУ '+Math.round(d.heap/1024)+' КБ (минимум '+Math.round(d.minheap/1024)+' КБ) · работает '+Math.floor(d.up/60)+' мин'+(d.sta?' · Wi-Fi: '+d.sta:'');
  const tb=document.querySelector('#t tbody');tb.innerHTML='';
  if(!d.meters.length){const tr=tb.insertRow();const td=tr.insertCell();td.colSpan=11;td.textContent='Счётчики не найдены, идёт поиск...';}
  d.meters.forEach(m=>{
   const tr=tb.insertRow();
   const c=[TN[m.type]||m.type,m.sn,vol(m.t1,m.unit),m.two?vol(m.t2,m.unit):'',m.total===null?'':vol(m.total,m.unit),m.bat+' %',m.temp===null?'—':m.temp+' °C',m.rssi,m.q<0?'—':m.q+' %',age(m.age),ST[m.st]||m.st];
   c.forEach((x,i)=>{const td=tr.insertCell();td.textContent=x;if(i>=2&&i<=4)td.className='tariff tariff'+(i-1);if(i==10)td.className='l'+m.lv;});
  });
 }catch(e){document.getElementById('info').textContent='Нет связи с устройством';}
}
load();setInterval(load,5000);
</script>
)rawliteral";

class Svt15Web {
 public:
  Svt15Web() : server_(80) {}

  void begin(MeterManager* meters, WifiManager* wifi, TimeManager* clk, HistoryManager* hist, AlertManager* al, MeterCfgStore* store, TelegramManager* tg) {
    store_ = store;
    tg_ = tg;
    meters_ = meters;
    wifi_ = wifi;
    clk_ = clk;
    hist_ = hist;
    al_ = al;
    route("/", HTTP_ANY, [this]() { handleRoot(); });
    route("/api/meters", HTTP_ANY, [this]() { handleMeters(); });
    route("/api/ha", HTTP_ANY, [this]() { handleApiHa(); });
    route("/ha.yaml", HTTP_ANY, [this]() { handleHaYaml(); });
    route("/wifi", HTTP_ANY, [this]() { handleWifi(""); });
    route("/wifi/save", HTTP_POST, [this]() { handleWifiSave(); });
    route("/wifi/sta", HTTP_POST, [this]() { handleWifiSta(); });
    route("/api/scan", HTTP_ANY, [this]() { handleScan(); });
    route("/ping", HTTP_ANY, [this]() { server_.send(200, "text/plain; charset=utf-8", "OK"); });
    route("/alarms", HTTP_ANY, [this]() { handleAlarmsPage(""); });
    route("/meters", HTTP_ANY, [this]() { handleMetersPage(); });
    route("/reset", HTTP_ANY, [this]() { handleResetPage(""); });
    route("/reset/do", HTTP_POST, [this]() { handleResetDo(); });
    route("/meter", HTTP_ANY, [this]() { handleMeterPage(""); });
    route("/meter/save", HTTP_POST, [this]() { handleMeterSave(); });
    route("/meter/now", HTTP_POST, [this]() { handleMeterNow(); });
    route("/telegram", HTTP_ANY, [this]() { handleTelegramPage(""); });
    route("/telegram/save", HTTP_POST, [this]() { handleTelegramSave(); });
    route("/telegram/check", HTTP_POST, [this]() { handleTelegramCheck(); });
    route("/telegram/test", HTTP_POST, [this]() { handleTelegramTest(); });
    route("/telegram/now", HTTP_POST, [this]() { handleTelegramNow(); });
    route("/telegram/detect", HTTP_POST, [this]() { handleTelegramDetect(); });
    route("/qrcode.js", HTTP_ANY, [this]() { handleQrJs(); });
    route("/alarms/ack", HTTP_POST, [this]() { handleAlarmAck(); });
    route("/alarms/remind", HTTP_POST, [this]() { handleRemindDismiss(); });
    route("/alarms/ackall", HTTP_POST, [this]() { al_->ackAllAlarms(); handleAlarmsPage("Все тревоги подтверждены."); });
    route("/alarms/suppress", HTTP_POST, [this]() { handleAlarmSuppress(); });
    route("/alarms/cfg", HTTP_POST, [this]() { handleAlarmCfg(); });
    route("/history", HTTP_ANY, [this]() { handleHistoryPage(); });
    route("/history/cfg", HTTP_POST, [this]() { handleHistoryCfg(); });
    route("/history.csv", HTTP_ANY, [this]() { handleCsv(); });
    route("/time", HTTP_ANY, [this]() { handleTimePage(""); });
    route("/time/save", HTTP_POST, [this]() { handleTimeSave(); });
    route("/time/ntp", HTTP_POST, [this]() { handleTimeNtp(); });
    route("/api/time", HTTP_ANY, [this]() {
      if (server_.method() == HTTP_POST) handleApiTimePost();
      else handleApiTimeGet();
    });
    route("/memory", HTTP_ANY, [this]() { handleMemoryPage(""); });
    route("/memory/days", HTTP_POST, [this]() { handleMemoryDays(); });
    route("/memory/clean", HTTP_POST, [this]() { handleMemoryClean(); });
#if SVT_FAKE_INTERNET
    // Проверки связи телефонов и компьютеров: ответ "интернет есть"
    route("/generate_204", HTTP_ANY, [this]() { server_.send(204, "text/plain", ""); });   // Android, Chrome
    route("/gen_204", HTTP_ANY, [this]() { server_.send(204, "text/plain", ""); });
    route("/hotspot-detect.html", HTTP_ANY, [this]() {                                     // iPhone, Mac
      server_.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    route("/library/test/success.html", HTTP_ANY, [this]() {
      server_.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
    });
    route("/connecttest.txt", HTTP_ANY, [this]() { server_.send(200, "text/plain", "Microsoft Connect Test"); });   // Windows
    route("/ncsi.txt", HTTP_ANY, [this]() { server_.send(200, "text/plain", "Microsoft NCSI"); });
    route("/success.txt", HTTP_ANY, [this]() { server_.send(200, "text/plain", "success\n"); });                     // Firefox
#endif
    server_.onNotFound([this]() {
      logReq();
      server_.send(404, "text/plain; charset=utf-8", "Not found");
    });
    server_.begin();
    SVT_SERIAL.println("WEB: started on port 80");
  }

  // Вызывать из loop()
  void loop() {
    server_.handleClient();
    if (resetPending_ && (int32_t)(millis() - resetAt_) >= 0) SVT_RESTART();
    if (restartAp_ && millis() - restartAt_ > 1500) {   // дать ответу уйти клиенту
      restartAp_ = false;
      wifi_->startAp();
    }
    if (applySta_ && millis() - applyStaAt_ > 1500) {
      applySta_ = false;
      wifi_->connectSta();
    }
    if (!applySta_) wifi_->loop();   // пока ответ не ушёл клиенту, попытки не начинаем
  }

 private:
  static String esc(const String& s) {
    String o;
    for (size_t i = 0; i < s.length(); i++) {
      char c = s[i];
      if (c == '&') o += "&amp;";
      else if (c == '<') o += "&lt;";
      else if (c == '>') o += "&gt;";
      else if (c == '"') o += "&quot;";
      else o += c;
    }
    return o;
  }

  static String jsonEsc(const String& s) {
    String o;
    for (size_t i = 0; i < s.length(); i++) {
      char c = s[i];
      if (c == '"' || c == '\\') { o += '\\'; o += c; }
      else if ((uint8_t)c >= 0x20) o += c;
    }
    return o;
  }

  // Регистрация обработчика с записью запроса в Serial0
  void route(const char* uri, HTTPMethod method, std::function<void()> fn) {
    server_.on(uri, method, [this, fn]() {
      logReq();
      fn();
    });
  }

  // Запись запроса в Serial0: видно, доходят ли запросы до платы
  void logReq() {
    if (SVT_WEB_LOG == 0) return;
    String u = server_.uri();
    if (SVT_WEB_LOG < 2 && u == "/api/meters") return;   // не засорять опросом страницы
    SVT_SERIAL.printf("WEB: %s %s from %s\n", server_.method() == HTTP_POST ? "POST" : "GET",
                      u.c_str(), server_.client().remoteIP().toString().c_str());
  }

  void handleRoot() {
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.send(200, "text/html; charset=utf-8", "");
    server_.sendContent(SVT_PAGE_MAIN);
    server_.sendContent(SVT_PAGE_STYLE);
    server_.sendContent(SVT_PAGE_MAIN_BODY);
    server_.sendContent("<script>" SVT_SYNC_JS "</script></body></html>");
    server_.sendContent("");
  }

  void handleMeters() {
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.send(200, "application/json; charset=utf-8", "");
    char buf[420];
    String sta = wifi_->staConnected() ? wifi_->staIp() : String("");
    snprintf(buf, sizeof(buf), "{\"ver\":\"%s\",\"heap\":%u,\"minheap\":%u,\"up\":%lu,\"sta\":\"%s\",\"meters\":[",
             SVT15_LCD_VERSION, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
             (unsigned long)(millis() / 1000UL),
             sta.c_str());
    server_.sendContent(buf);
    uint8_t n = meters_->count();
    bool first = true;
    for (uint8_t i = 0; i < n; i++) {
      MeterData m;
      if (!meters_->get(i, m)) continue;
      char t1[24], t2[24], total[24], tp[16];
      if (m.hasT1) snprintf(t1, sizeof(t1), "%.*f", m.dec, m.t1); else strcpy(t1, "null");
      if (m.twoTariff && m.hasT2) snprintf(t2, sizeof(t2), "%.*f", m.dec, m.t2); else strcpy(t2, "null");
       if (m.twoTariff && m.hasT1 && m.hasT2) snprintf(total, sizeof(total), "%.*f", m.dec, m.t1 + m.t2); else strcpy(total, "null");
      if (m.hasTemp) snprintf(tp, sizeof(tp), "%.1f", m.temp); else strcpy(tp, "null");
      Svt15Level lvl;
      const char* st = svt15Status(m, &lvl);
      snprintf(buf, sizeof(buf),
               "%s{\"type\":%u,\"sn\":%lu,\"model\":%u,\"two\":%s,\"unit\":\"%s\","
               "\"t1\":%s,\"t2\":%s,\"total\":%s,\"temp\":%s,\"bat\":%u,\"fw\":%.1f,\"rssi\":%d,"
               "\"age\":%lu,\"q\":%d,\"st\":\"%s\",\"lv\":%d}",
               first ? "" : ",", m.type, (unsigned long)m.id, m.model,
               m.twoTariff ? "true" : "false", svt15Unit(m.type), t1, t2, total, tp,
               m.battery, m.fw, m.rssi,
               (unsigned long)((millis() - m.lastSeenMs) / 1000UL), svt15Quality(m), st, (int)lvl);
      server_.sendContent(buf);
      first = false;
    }
    server_.sendContent("]}");
    server_.sendContent("");
  }

  void handleWifi(const char* msg) {
    String h;
    h.reserve(3500);
    h += SVT_PAGE_MAIN;
    h += SVT_PAGE_STYLE;
    h += "</head><body><h1>Monitor Elehant</h1>"
         SVT_NAV_HTML
         "<div class=\"box\">";
    if (msg && msg[0]) { h += "<p><b>"; h += msg; h += "</b></p>"; }

    // ---- точка доступа ----
    h += "<h2>Точка доступа</h2><p class=\"mut\">IP: ";
    h += wifi_->apIp();
    h += "</p><form method=\"POST\" action=\"/wifi/save\">"
         "<label>SSID (1-32 символа)</label><input name=\"ssid\" maxlength=\"32\" value=\"";
    h += esc(wifi_->apSsid());
    h += "\"><label>Пароль (8-63 символа)</label><input name=\"pass\" maxlength=\"63\" value=\"";
    h += esc(wifi_->apPass());
    h += "\"><button type=\"submit\">Сохранить</button></form>"
         "<p class=\"mut\">После сохранения точка доступа перезапустится, переподключитесь с новыми данными.</p>";

    // ---- домашний Wi-Fi ----
    h += "<h2>Домашний Wi-Fi</h2><p class=\"mut\">Состояние: ";
    if (!wifi_->staEnabled() || wifi_->staSsid().length() == 0) {
      if (wifi_->staSsid().length() > 0) h += "отключено (снята галочка «Подключаться к этой сети»)";
      else h += "отключено";
    } else if (wifi_->staConnected()) {
      h += "подключено к ";
      h += esc(wifi_->staSsid());
      h += ", IP ";
      h += wifi_->staIp();
      h += ", сигнал ";
      h += String(wifi_->staRssi());
      h += " dBm";
    } else {
      h += "не подключено к ";
      h += esc(wifi_->staSsid());
      h += ": ";
      h += wifi_->staReasonText();
    }
    h += "</p><form method=\"POST\" action=\"/wifi/sta\">"
         "<label>Сеть (SSID)</label><input name=\"ssid\" maxlength=\"32\" list=\"nets\" autocomplete=\"off\" value=\"";
    h += esc(wifi_->staSsid());
    h += "\"><datalist id=\"nets\"></datalist>"
         "<button type=\"button\" id=\"scan\">Найти сети</button> <span class=\"mut\" id=\"scanst\"></span>"
         "<label>Пароль (8-63 символа; пусто: не менять для той же сети, для новой сети без пароля)</label>"
         "<input name=\"pass\" type=\"password\" maxlength=\"63\" autocomplete=\"new-password\">"
         "<label><input type=\"checkbox\" name=\"en\" value=\"1\" style=\"width:auto;margin:0 6px 0 0\"";
    if (wifi_->staEnabled() || wifi_->staSsid().length() == 0) h += " checked";   // при первой настройке галочка уже стоит
    h += ">Подключаться к этой сети</label>"
         "<button type=\"submit\">Сохранить</button></form>"
         "<p class=\"mut\">ESP32 работает только в диапазоне 2.4 ГГц, сети 5 ГГц не подходят. "
         "Когда подключена домашняя сеть, канал точки доступа подстраивается под неё, "
         "подключение к AP может кратковременно прерваться.</p>"
         "</div><script>"
         "const b=document.getElementById('scan'),st=document.getElementById('scanst');"
         "b.onclick=async()=>{b.disabled=true;st.textContent='Поиск...';"
         "for(let i=0;i<12;i++){try{const d=await(await fetch('/api/scan')).json();"
         "if(d.state=='done'){const dl=document.getElementById('nets');dl.innerHTML='';"
         "d.nets.forEach(n=>{const o=document.createElement('option');o.value=n.s;o.label=n.s+' ('+n.r+' dBm)';dl.appendChild(o);});"
         "st.textContent='Найдено: '+d.nets.length+', выберите в поле SSID';b.disabled=false;return;}}catch(e){}"
         "await new Promise(r=>setTimeout(r,1500));}"
         "st.textContent='Не удалось выполнить поиск';b.disabled=false;};"
         "</script><script>" SVT_SYNC_JS "</script></body></html>";
    server_.send(200, "text/html; charset=utf-8", h);
  }

  void handleWifiSave() {
    String ssid = server_.arg("ssid");
    String pass = server_.arg("pass");
    if (ssid.length() < 1 || ssid.length() > 32 || pass.length() < 8 || pass.length() > 63) {
      handleWifi("Ошибка: SSID 1-32 символа, пароль 8-63 символа. Параметры не изменены.");
      return;
    }
    wifi_->saveAp(ssid, pass);
    restartAp_ = true;
    restartAt_ = millis();
    handleWifi("Сохранено. Точка доступа перезапускается.");
  }

  void handleWifiSta() {
    String ssid = server_.arg("ssid");
    String pass = server_.arg("pass");
    bool en = server_.arg("en").length() > 0;
    if (ssid.length() > 32 || (en && ssid.length() == 0) ||
        (pass.length() > 0 && (pass.length() < 8 || pass.length() > 63))) {
      handleWifi("Ошибка: SSID до 32 символов, пароль 8-63 символа. Параметры не изменены.");
      return;
    }
    if (pass.length() == 0 && ssid == wifi_->staSsid()) pass = wifi_->staPass();   // та же сеть: пароль не менять
    wifi_->saveSta(ssid, pass, en);
    applySta_ = true;
    applyStaAt_ = millis();
    handleWifi(en ? "Сохранено. Идёт подключение к домашней сети." : "Сохранено. Домашний Wi-Fi отключён.");
  }

  // Асинхронный поиск сетей: повторять запрос, пока state не станет done
  void handleScan() {
    int n = WiFi.scanComplete();
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.send(200, "application/json; charset=utf-8", "");
    if (n == WIFI_SCAN_FAILED) {
      WiFi.scanNetworks(true);
      server_.sendContent("{\"state\":\"run\"}");
    } else if (n == WIFI_SCAN_RUNNING) {
      server_.sendContent("{\"state\":\"run\"}");
    } else {
      server_.sendContent("{\"state\":\"done\",\"nets\":[");
      bool first = true;
      for (int i = 0; i < n; i++) {
        String s = WiFi.SSID(i);
        if (s.length() == 0) continue;
        String item = first ? "" : ",";
        item += "{\"s\":\"";
        item += jsonEsc(s);
        item += "\",\"r\":";
        item += String(WiFi.RSSI(i));
        item += "}";
        server_.sendContent(item);
        first = false;
      }
      server_.sendContent("]}");
      WiFi.scanDelete();
    }
    server_.sendContent("");
  }

// ---------- новые страницы: потоковая отдача, без больших String ----------
  void pageHead() {
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.send(200, "text/html; charset=utf-8", "");
    server_.sendContent(SVT_PAGE_MAIN);
    server_.sendContent(SVT_PAGE_STYLE);
    server_.sendContent("</head><body><h1>Monitor Elehant</h1>" SVT_NAV_HTML);
  }

  void pageFoot() {
    server_.sendContent("<script>" SVT_SYNC_JS "</script></body></html>");
    server_.sendContent("");
  }

  static void tzLabel(int m, char* out, size_t n) {
    int a = m < 0 ? -m : m;
    snprintf(out, n, "%c%d:%02d", m < 0 ? '-' : '+', a / 60, a % 60);
  }

  static bool validDate(const String& s) {   // YYYY-MM-DD
    if (s.length() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (int i = 0; i < 10; i++) {
      if (i != 4 && i != 7 && !(s[i] >= '0' && s[i] <= '9')) return false;
    }
    return true;
  }

  // ---------- Время ----------
  void handleTimePage(const char* msg) {
    pageHead();
    char b[360];
    server_.sendContent("<div class=\"box\">");
    if (msg && msg[0]) {
      snprintf(b, sizeof(b), "<p><b>%s</b></p>", msg);
      server_.sendContent(b);
    }
    server_.sendContent("<h2>Время</h2><p class=\"mut\">");
    if (clk_->valid()) {
      char dt[24], tz[12];
      TimeManager::fmtDateTime(clk_->local(), dt, sizeof(dt));
      tzLabel(clk_->tzMinutes(), tz, sizeof(tz));
      snprintf(b, sizeof(b), "Установлено: %s (UTC%s)", dt, tz);
    } else {
      snprintf(b, sizeof(b), "Время не установлено: откройте страницу с телефона или компьютера, "
                             "задайте вручную или включите NTP. Пока времени нет, история не пишется.");
    }
    server_.sendContent(b);
    server_.sendContent("</p><form method=\"POST\" action=\"/time/save\">"
                        "<label>Часовой пояс</label><select name=\"tz\">");
    for (int m = -720; m <= 840; m += 30) {
      char tz[12];
      tzLabel(m, tz, sizeof(tz));
      snprintf(b, sizeof(b), "<option value=\"%d\"%s>UTC%s</option>", m,
               m == clk_->tzMinutes() ? " selected" : "", tz);
      server_.sendContent(b);
    }
    server_.sendContent("</select><label><input type=\"checkbox\" name=\"ntp\" value=\"1\" "
                        "style=\"width:auto;margin:0 6px 0 0\"");
    if (clk_->ntpEnabled()) server_.sendContent(" checked");
    server_.sendContent(">Синхронизировать по NTP (нужен домашний Wi-Fi)</label>"
                        "<label>NTP-сервер</label><input name=\"ntpsrv\" maxlength=\"60\" value=\"");
    putText(esc(clk_->ntpServer()));
    server_.sendContent("\"><label>Задать вручную (местное время, пусто: не менять)</label>"
                        "<input type=\"datetime-local\" name=\"manual\" value=\"");
    if (clk_->valid()) {
      TimeManager::fmtInput(clk_->local(), b, sizeof(b));
      server_.sendContent(b);
    }
    server_.sendContent("\"><button type=\"submit\">Сохранить</button></form>"
                        "<p><button type=\"button\" id=\"bs\">Синхронизировать с браузером</button></p>"
                        "<form method=\"POST\" action=\"/time/ntp\"><button type=\"submit\">Запросить время по NTP сейчас</button></form>"
                        "<p class=\"mut\">Часовой пояс задаётся смещением, перехода на летнее время нет. "
                        "Часов реального времени на плате нет: после перезагрузки время восстанавливается "
                        "при заходе на страницу или по NTP.</p></div>"
                        "<script>document.getElementById('bs').onclick=async()=>{"
                        "const n=Math.floor(Date.now()/1000);"
                        "await fetch('/api/time',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
                        "body:'epoch='+n+'&tz='+(-new Date().getTimezoneOffset())+'&force=1'});location.reload();};</script>");
    pageFoot();
  }

  void handleTimeSave() {
    int tz = server_.arg("tz").toInt();
    bool ntp = server_.arg("ntp").length() > 0;
    String srv = server_.arg("ntpsrv");
    srv.trim();
    String manual = server_.arg("manual");
    if (tz < -720 || tz > 840 || srv.length() > 60 || (ntp && srv.length() == 0)) {
      handleTimePage("Ошибка: проверьте часовой пояс и адрес NTP-сервера. Параметры не изменены.");
      return;
    }
    int64_t manualUtc = 0;
    bool haveManual = false;
    if (manual.length() >= 16) {   // YYYY-MM-DDTHH:MM
      int y = manual.substring(0, 4).toInt(), mo = manual.substring(5, 7).toInt();
      int d = manual.substring(8, 10).toInt(), hh = manual.substring(11, 13).toInt();
      int mi = manual.substring(14, 16).toInt();
      if (y < 2024 || y > 2100 || mo < 1 || mo > 12 || d < 1 || d > 31 || hh > 23 || mi > 59) {
        handleTimePage("Ошибка: неверная дата или время. Параметры не изменены.");
        return;
      }
      manualUtc = svtCivilToEpoch(y, mo, d, hh, mi) - (int64_t)tz * 60;
      haveManual = true;
    }
    clk_->setTz(tz, true);
    clk_->setNtp(ntp, srv);
    if (haveManual) clk_->setUtc((time_t)manualUtc);
    handleTimePage("Сохранено.");
  }

  void handleTimeNtp() {
    bool ok = clk_->syncNtpNow(wifi_->staConnected());
    handleTimePage(ok ? "Запрос времени по NTP отправлен, обновите страницу через несколько секунд."
                      : "Нет подключения к домашней сети, NTP недоступен.");
  }

  void handleApiTimeGet() {
    char b[160];
    snprintf(b, sizeof(b), "{\"valid\":%s,\"epoch\":%lu,\"tz\":%d,\"ntp\":%s}",
             clk_->valid() ? "true" : "false", (unsigned long)clk_->utc(), clk_->tzMinutes(),
             clk_->ntpEnabled() ? "true" : "false");
    server_.send(200, "application/json; charset=utf-8", b);
  }

  // Время из браузера: обновляется, если не установлено или расхождение больше 60 минут (force: кнопка)
  void handleApiTimePost() {
    long long epoch = atoll(server_.arg("epoch").c_str());
    int tz = server_.arg("tz").toInt();
    bool force = server_.arg("force").length() > 0;
    bool changed = false;
    if (epoch > 1700000000LL) changed = clk_->fromBrowser((time_t)epoch, tz, force);
    server_.send(200, "application/json; charset=utf-8", changed ? "{\"changed\":true}" : "{\"changed\":false}");
  }

  // ---------- Настройки счётчиков ----------
  bool findMeter(uint8_t type, uint32_t sn, uint8_t* idx) {
    uint8_t n = meters_->count();
    for (uint8_t i = 0; i < n; i++) {
      MeterData m;
      if (meters_->get(i, m) && m.type == type && m.id == sn) {
        if (idx) *idx = i;
        return true;
      }
    }
    return false;
  }

  // Пустая строка в sendContent завершает HTTP-ответ, поэтому пустые значения не отправляем
  void putText(const String& t) {
    if (t.length()) server_.sendContent(t);
  }

  static const char* chk(bool b) { return b ? " checked" : ""; }
  static const char* sel(bool b) { return b ? " selected" : ""; }

  void handleMetersPage() {
    pageHead();
    char b[260];
    server_.sendContent("<div class=\"box\" style=\"max-width:none\"><h2>Настройки счётчиков</h2>"
                        "<div class=\"wrap\"><table><thead><tr><th>Счётчик</th><th>Тарифы</th><th>Telegram</th><th></th></tr></thead><tbody>");
    uint8_t n = meters_->count();
    int rows = 0;
    for (uint8_t i = 0; i < n; i++) {
      MeterData m;
      if (!meters_->get(i, m)) continue;
      MeterCfg& c = store_->at(m.type, m.id);
      snprintf(b, sizeof(b), "<tr><td>%c%lu</td><td>%s / %s</td><td>%s</td><td><a href=\"/meter?t=%u&sn=%lu\">Настроить</a></td></tr>",
               svt15TypeChar(m.type), (unsigned long)m.id, store_->name(m.type, m.id, 1, true),
               store_->name(m.type, m.id, 2, true), c.tgOn ? "вкл" : "выкл", (unsigned)m.type, (unsigned long)m.id);
      server_.sendContent(b);
      rows++;
    }
    if (rows == 0) server_.sendContent("<tr><td colspan=\"4\">Счётчиков пока нет</td></tr>");
    server_.sendContent("</tbody></table></div>"
                        "<p><a href=\"/reset\">Сброс настроек платы</a></p></div>");
    pageFoot();
  }

  void handleMeterPage(const char* msg) {
    uint8_t type = (uint8_t)server_.arg("t").toInt();
    uint32_t sn = (uint32_t)server_.arg("sn").toInt();
    pageHead();
    char b[1200];
    server_.sendContent("<div class=\"box\">");
    if (msg && msg[0]) { snprintf(b, sizeof(b), "<p><b>%s</b></p>", msg); server_.sendContent(b); }
    if (!(type == 1 || type == 2 || type == 4) || sn == 0 || !findMeter(type, sn, nullptr)) {
      server_.sendContent("<p>Счётчик не найден. Откройте его со страницы «Настройки».</p></div>");
      pageFoot();
      return;
    }
    MeterCfg& c = store_->at(type, sn);
    snprintf(b, sizeof(b), "<h2>%c%lu</h2><form method=\"POST\" action=\"/meter/save\">"
             "<input type=\"hidden\" name=\"t\" value=\"%u\"><input type=\"hidden\" name=\"sn\" value=\"%lu\">",
             svt15TypeChar(type), (unsigned long)sn, (unsigned)type, (unsigned long)sn);
    server_.sendContent(b);
    server_.sendContent("<h2>Названия тарифов</h2><label>Тариф 1 (пусто: по умолчанию)</label><input name=\"n1\" maxlength=\"23\" value=\"");
    putText(esc(String(c.name1)));
    server_.sendContent("\"><label>Тариф 2</label><input name=\"n2\" maxlength=\"23\" value=\"");
    putText(esc(String(c.name2)));
    server_.sendContent("\">");
    server_.sendContent("<h2>Telegram</h2>");
    snprintf(b, sizeof(b), "<label><input type=\"checkbox\" name=\"en\" value=\"1\" style=\"width:auto;margin:0 6px 0 0\"%s>Отправлять данные этого счётчика в Telegram</label>", chk(c.tgOn));
    server_.sendContent(b);
    snprintf(b, sizeof(b), "<label>Когда отправлять показания</label><select name=\"mode\" style=\"width:auto\">"
             "<option value=\"0\"%s>не отправлять</option><option value=\"1\"%s>каждый день</option>"
             "<option value=\"2\"%s>раз в неделю</option><option value=\"3\"%s>раз в месяц</option></select>",
             sel(c.tgMode == 0), sel(c.tgMode == 1), sel(c.tgMode == 2), sel(c.tgMode == 3));
    server_.sendContent(b);
    snprintf(b, sizeof(b), "<label>День: для недели 0 (вс) - 6 (сб), для месяца 1-28, для ежедневной не важен</label>"
             "<input name=\"day\" type=\"number\" min=\"0\" max=\"28\" value=\"%u\">"
             "<label>Час отправки (0-23)</label><input name=\"hour\" type=\"number\" min=\"0\" max=\"23\" value=\"%u\">",
             (unsigned)c.tgDay, (unsigned)c.tgHour);
    server_.sendContent(b);
    server_.sendContent("<label>Какие данные отправлять</label>");
    snprintf(b, sizeof(b),
             "<label><input type=\"checkbox\" name=\"f1\" value=\"1\" style=\"width:auto;margin:0 6px 0 0\"%s>тариф 1</label>"
             "<label><input type=\"checkbox\" name=\"f2\" value=\"1\" style=\"width:auto;margin:0 6px 0 0\"%s>тариф 2</label>"
             "<label><input type=\"checkbox\" name=\"ftemp\" value=\"1\" style=\"width:auto;margin:0 6px 0 0\"%s>температура</label>"
             "<label><input type=\"checkbox\" name=\"fbat\" value=\"1\" style=\"width:auto;margin:0 6px 0 0\"%s>батарея</label>"
             "<label><input type=\"checkbox\" name=\"frssi\" value=\"1\" style=\"width:auto;margin:0 6px 0 0\"%s>уровень сигнала</label>"
             "<label><input type=\"checkbox\" name=\"ffw\" value=\"1\" style=\"width:auto;margin:0 6px 0 0\"%s>версия прошивки счётчика</label>",
             chk(c.tgFields & 1), chk(c.tgFields & 2), chk(c.tgFields & 4), chk(c.tgFields & 8), chk(c.tgFields & 16), chk(c.tgFields & 32));
    server_.sendContent(b);
    server_.sendContent("<label>Какие тревоги отправлять</label>");
    for (uint8_t t = 0; t < AL_COUNT; t++) {
      snprintf(b, sizeof(b), "<label><input type=\"checkbox\" name=\"a%u\" value=\"1\" style=\"width:auto;margin:0 6px 0 0\"%s>%s</label>",
               (unsigned)t, chk(c.tgAlarms & (1u << t)), SVT_ALARM_NAME[t]);
      server_.sendContent(b);
    }
    snprintf(b, sizeof(b), "<label>Повторять незакрытую тревогу каждые N часов (0: один раз)</label>"
             "<input name=\"rep\" type=\"number\" min=\"0\" max=\"168\" value=\"%u\">", (unsigned)c.tgRepeatH);
    server_.sendContent(b);
    server_.sendContent("<button type=\"submit\">Сохранить</button></form>");
    snprintf(b, sizeof(b), "<form method=\"POST\" action=\"/meter/now\" style=\"margin-top:10px\">"
             "<input type=\"hidden\" name=\"t\" value=\"%u\"><input type=\"hidden\" name=\"sn\" value=\"%lu\">"
             "<button type=\"submit\">Отправить показания этого счётчика сейчас</button></form>", (unsigned)type, (unsigned long)sn);
    server_.sendContent(b);
    snprintf(b, sizeof(b), "<h2>Home Assistant</h2><p class=\"mut\">Файл YAML только для этого счётчика, с IP платы и его серийным номером.</p>"
             "<p><a href=\"/ha.yaml?t=%u&sn=%lu\">Скачать YAML для Home Assistant</a></p>", (unsigned)type, (unsigned long)sn);
    server_.sendContent(b);
    server_.sendContent("</div>");
    pageFoot();
  }

  void handleMeterSave() {
    uint8_t type = (uint8_t)server_.arg("t").toInt();
    uint32_t sn = (uint32_t)server_.arg("sn").toInt();
    if (!(type == 1 || type == 2 || type == 4) || sn == 0 || !findMeter(type, sn, nullptr)) {
      handleMeterPage("Ошибка: счётчик не найден.");
      return;
    }
    String n1 = server_.arg("n1"), n2 = server_.arg("n2");
    int mode = server_.arg("mode").toInt();
    int day = server_.arg("day").toInt(), hour = server_.arg("hour").toInt(), rep = server_.arg("rep").toInt();
    if (!MeterCfgStore::validName(n1.c_str()) || !MeterCfgStore::validName(n2.c_str())) {
      handleMeterPage("Ошибка: название тарифа до 23 символов, без кавычек, слэша и угловых скобок.");
      return;
    }
    if (mode < 0 || mode > 3 || hour < 0 || hour > 23 || rep < 0 || rep > 168) {
      handleMeterPage("Ошибка: проверьте расписание, час и повтор.");
      return;
    }
    if (mode == SVT_TGM_WEEKLY && (day < 0 || day > 6)) { handleMeterPage("Ошибка: день недели от 0 (вс) до 6 (сб)."); return; }
    if (mode == SVT_TGM_MONTHLY && (day < 1 || day > 28)) { handleMeterPage("Ошибка: число месяца от 1 до 28."); return; }
    if (mode != SVT_TGM_WEEKLY && mode != SVT_TGM_MONTHLY) day = 0;

    MeterCfg& c = store_->at(type, sn);
    snprintf(c.name1, sizeof(c.name1), "%s", n1.c_str());
    snprintf(c.name2, sizeof(c.name2), "%s", n2.c_str());
    c.tgOn = server_.arg("en").length() > 0;
    c.tgMode = (uint8_t)mode;
    c.tgDay = (uint8_t)day;
    c.tgHour = (uint8_t)hour;
    c.tgRepeatH = (uint8_t)rep;
    uint8_t f = 0;
    if (server_.arg("f1").length()) f |= SVT_TGF_T1;
    if (server_.arg("f2").length()) f |= SVT_TGF_T2;
    if (server_.arg("ftemp").length()) f |= SVT_TGF_TEMP;
    if (server_.arg("fbat").length()) f |= SVT_TGF_BAT;
    if (server_.arg("frssi").length()) f |= SVT_TGF_RSSI;
    if (server_.arg("ffw").length()) f |= SVT_TGF_FW;
    c.tgFields = f;
    uint8_t a = 0;
    for (uint8_t t = 0; t < AL_COUNT; t++) {
      char name[4];
      snprintf(name, sizeof(name), "a%u", (unsigned)t);
      if (server_.arg(name).length()) a |= (uint8_t)(1u << t);
    }
    c.tgAlarms = a;
    store_->save();
    handleMeterPage("Сохранено.");
  }

  void handleMeterNow() {
    uint8_t type = (uint8_t)server_.arg("t").toInt();
    uint32_t sn = (uint32_t)server_.arg("sn").toInt();
    uint8_t idx = 0;
    if (!findMeter(type, sn, &idx)) { handleMeterPage("Ошибка: счётчик не найден."); return; }
    handleMeterPage(tg_->sendMeterNow(idx) ? "Показания поставлены в очередь отправки."
                                           : "Не отправлено: Telegram выключен или не настроен, либо нет данных счётчика.");
  }

  // ---------- Telegram ----------
  static void qrOut(void* ctx, const char* d, size_t n) { ((WebServer*)ctx)->sendContent(d, n); }

  void handleTelegramPage(const char* msg) {
    pageHead();
    char b[900];
    server_.sendContent("<div class=\"box\">");
    if (msg && msg[0]) { snprintf(b, sizeof(b), "<p><b>%s</b></p>", msg); server_.sendContent(b); }
    char tok[24];
    if (tg_->cfg.token.length()) snprintf(tok, sizeof(tok), "...%s", tg_->tokenTail().c_str());
    else snprintf(tok, sizeof(tok), "не задан");
    snprintf(b, sizeof(b), "<h2>Telegram</h2><p class=\"mut\">Токен: %s, бот: %s, очередь: %u, статус: %s, код: %d</p>",
             tok, tg_->cfg.botName.length() ? tg_->cfg.botName.c_str() : "не определён",
             (unsigned)tg_->queued(), tg_->status(), tg_->lastCode());
    server_.sendContent(b);
    server_.sendContent("<form method=\"POST\" action=\"/telegram/save\">"
                        "<label>Токен бота (от @BotFather; пусто: не менять)</label>"
                        "<input name=\"token\" type=\"password\" maxlength=\"90\" autocomplete=\"off\">"
                        "<label>ID чата или @имя канала (для группы ID с минусом)</label>"
                        "<input name=\"chat\" maxlength=\"40\" value=\"");
    putText(esc(tg_->cfg.chat));
    snprintf(b, sizeof(b), "\"><label><input type=\"checkbox\" name=\"en\" value=\"1\" style=\"width:auto;margin:0 6px 0 0\"%s>Отправлять в Telegram</label>"
             "<label><input type=\"checkbox\" name=\"alarms\" value=\"1\" style=\"width:auto;margin:0 6px 0 0\"%s>Отправлять тревоги</label>"
             "<button type=\"submit\">Сохранить</button></form>", chk(tg_->cfg.enabled), chk(tg_->cfg.alarms));
    server_.sendContent(b);
    server_.sendContent("<form method=\"POST\" action=\"/telegram/detect\" style=\"margin-top:8px\"><button type=\"submit\">Определить ID чата из сообщения боту</button></form>"
                        "<form method=\"POST\" action=\"/telegram/check\" style=\"margin-top:8px\"><button type=\"submit\">Проверить токен и определить бота</button></form>"
                        "<form method=\"POST\" action=\"/telegram/test\" style=\"margin-top:8px\"><button type=\"submit\">Тестовое сообщение</button></form>"
                        "<form method=\"POST\" action=\"/telegram/now\" style=\"margin-top:8px\"><button type=\"submit\">Отправить показания всех счётчиков сейчас</button></form>");
    server_.sendContent("<h2>QR-код бота</h2>");
    String link;
    if (tg_->botLink(link)) {
      // QR строит браузер по ссылке (библиотека /qrcode.js). Ссылка содержит только буквы, цифры и _
      char qb[700];
      snprintf(qb, sizeof(qb),
               "<div id=\"qr\" style=\"width:200px;background:#fff;padding:6px\"></div>"
               "<p class=\"mut\"><a href=\"%s\">%s</a></p>"
               "<script src=\"/qrcode.js\"></script>"
               "<script>var q=qrcode(0,'M');q.addData('%s');q.make();"
               "document.getElementById('qr').innerHTML=q.createSvgTag({cellSize:6,margin:4,scalable:true});</script>",
               link.c_str(), link.c_str(), link.c_str());
      server_.sendContent(qb);
    } else {
      server_.sendContent("<p class=\"mut\">Нажмите «Проверить токен и определить бота», тогда появится QR-код.</p>");
    }
    server_.sendContent("<p class=\"mut\">Токен хранится в памяти платы и на странице не показывается полностью. "
                        "Проверка сертификата Telegram отключена, поэтому не используйте прошивку в недоверенных сетях.</p></div>");
    pageFoot();
  }

  void handleTelegramSave() {
    String tok = server_.arg("token"), chat = server_.arg("chat");
    tok.trim();
    chat.trim();
    bool en = server_.arg("en").length() > 0, alarms = server_.arg("alarms").length() > 0;
    if (tok.length() && !TelegramManager::validToken(tok)) {
      handleTelegramPage("Ошибка: токен неверного вида (формат 123456:ABC...). Параметры не изменены.");
      return;
    }
    if (chat.length() && !TelegramManager::validChat(chat)) {
      handleTelegramPage("Ошибка: ID чата (число, для группы с минусом) или @имя. Параметры не изменены.");
      return;
    }
    // Токен сохраняется даже без ID чата: отправку тогда включить нельзя, пока нет ID и токена
    bool noChat = en && !(chat.length() || tg_->cfg.chat.length());
    bool noTok = en && !(tok.length() || TelegramManager::validToken(tg_->cfg.token));
    if (noChat || noTok) en = false;
    if (tok.length()) {
      if (tok != tg_->cfg.token) tg_->cfg.botName = "";
      tg_->cfg.token = tok;
    }
    if (chat.length()) tg_->cfg.chat = chat;
    tg_->cfg.enabled = en;
    tg_->cfg.alarms = alarms;
    tg_->save();
    handleTelegramPage(noChat ? "Токен сохранён. ID чата пока нет: определите его кнопкой и включите отправку."
                       : noTok ? "Сохранено. Отправка выключена: укажите токен бота."
                               : "Сохранено.");
  }

  // ---------- REST для Home Assistant: счётчики по серийному номеру, плоские поля ----------
  static const char* typeWord(uint8_t t) { return t == 1 ? "gas" : (t == 4 ? "heat" : "water"); }

  void handleApiHa() {
    server_.sendHeader("Cache-Control", "no-store");
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.send(200, "application/json; charset=utf-8", "");
    char b[160];
    snprintf(b, sizeof(b), "{\"ver\":\"%s\",\"time\":%lu,\"meters\":{", SVT15_LCD_VERSION,
             (unsigned long)(clk_->valid() ? clk_->utc() : 0));
    server_.sendContent(b);
    uint8_t n = meters_->count();
    bool first = true;
    for (uint8_t i = 0; i < n; i++) {
      MeterData m;
      if (!meters_->get(i, m)) continue;
      MeterCfg& c = store_->at(m.type, m.id);
      Svt15Level lvl;
      const char* st = svt15Status(m, &lvl);
      char t1[24], t2[24], total[24], tp[16], fw[12];
      if (m.hasT1) snprintf(t1, sizeof(t1), "%.*f", m.dec, m.t1); else strcpy(t1, "null");
      if (m.twoTariff && m.hasT2) snprintf(t2, sizeof(t2), "%.*f", m.dec, m.t2); else strcpy(t2, "null");
       if (m.twoTariff && m.hasT1 && m.hasT2) snprintf(total, sizeof(total), "%.*f", m.dec, m.t1 + m.t2); else strcpy(total, "null");
      if (m.hasTemp) snprintf(tp, sizeof(tp), "%.1f", m.temp); else strcpy(tp, "null");
      snprintf(fw, sizeof(fw), "%.1f", m.fw);
      int q = svt15Quality(m);
      String n1 = jsonEsc(String(store_->name(m.type, m.id, 1, m.twoTariff)));
      String n2 = jsonEsc(String(store_->name(m.type, m.id, 2, m.twoTariff)));
      char body[520];
      snprintf(body, sizeof(body),
               "%s\"%lu\":{\"type\":\"%s\",\"name1\":\"%s\",\"name2\":\"%s\",\"two\":%s,\"unit\":\"%s\","
               "\"t1\":%s,\"t2\":%s,\"total\":%s,\"temp\":%s,\"battery\":%u,\"rssi\":%d,\"quality\":%s,"
               "\"status\":\"%s\",\"age_s\":%lu,\"fw\":%s}",
               first ? "" : ",", (unsigned long)m.id, typeWord(m.type), n1.c_str(), n2.c_str(),
               m.twoTariff ? "true" : "false", svt15Unit(m.type), t1, t2, total, tp,
               (unsigned)m.battery, (int)m.rssi, q < 0 ? "null" : (String(q)).c_str(), st,
               (unsigned long)((millis() - m.lastSeenMs) / 1000UL), fw);
      server_.sendContent(body);
      first = false;
    }
    server_.sendContent("}}");
    server_.sendContent("");
  }

  // ---------- Файл для Home Assistant: генерирует плата (реальный IP и серийные номера) ----------
  void haSensor(const char* ip, uint32_t sn, const char* field, const char* label, const char* unit,
                const char* devClass, const char* stateClass, int interval) {
    char b[420];
    snprintf(b, sizeof(b),
             "  - platform: rest\n"
             "    name: \"%s\"\n"
             "    unique_id: svt15_%lu_%s\n"
             "    resource: http://%s/api/ha\n"
             "    scan_interval: %d\n"
             "    value_template: \"{{ value_json.meters['%lu'].%s }}\"\n",
             label, (unsigned long)sn, field, ip, interval, (unsigned long)sn, field);
    server_.sendContent(b);
    if (unit && unit[0]) { snprintf(b, sizeof(b), "    unit_of_measurement: \"%s\"\n", unit); server_.sendContent(b); }
    if (devClass && devClass[0]) { snprintf(b, sizeof(b), "    device_class: %s\n", devClass); server_.sendContent(b); }
    if (stateClass && stateClass[0]) { snprintf(b, sizeof(b), "    state_class: %s\n", stateClass); server_.sendContent(b); }
  }

  void handleHaYaml() {
    bool up = wifi_->staConnected();
    String ip = up ? wifi_->staIp() : String("192.168.4.1");
    // ?t=тип&sn=номер — файл только для одного счётчика; без параметров — для всех
    bool only = server_.arg("sn").length() > 0;
    uint8_t wantT = (uint8_t)server_.arg("t").toInt();
    uint32_t wantSn = (uint32_t)server_.arg("sn").toInt();
    auto want = [&](const MeterData& mm) { return !only || (mm.type == wantT && mm.id == wantSn); };
    uint8_t n = 0;
    for (uint8_t i = 0; i < meters_->count(); i++) {
      MeterData mm;
      if (meters_->get(i, mm) && want(mm)) n++;
    }
    server_.sendHeader("Cache-Control", "no-store");
    if (only) {
      char fn[48];
      snprintf(fn, sizeof(fn), "attachment; filename=svt15_%lu.yaml", (unsigned long)wantSn);
      server_.sendHeader("Content-Disposition", fn);
    }
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.send(200, "text/plain; charset=utf-8", "");
    char b[260];
    snprintf(b, sizeof(b), "# Файл для Home Assistant, создан платой Monitor Elehant %s\n# IP платы: %s\n", SVT15_LCD_VERSION, ip.c_str());
    server_.sendContent(b);
    if (!up) server_.sendContent("# ВНИМАНИЕ: плата не в домашней сети, адрес 192.168.4.1 из Home Assistant недоступен.\n# Подключите домашний Wi-Fi и скачайте файл снова.\n");
    if (n == 0) {
      server_.sendContent("sensor: []\nbinary_sensor: []\n");
      server_.sendContent("");
      return;
    }
    server_.sendContent("sensor:\n");
    for (uint8_t i = 0; i < meters_->count(); i++) {
      MeterData m;
      if (!meters_->get(i, m) || !want(m)) continue;
      uint32_t sn = m.id;
      const char* tl = m.type == 1 ? "Газ" : (m.type == 4 ? "Тепло" : "Вода");
      const char* u = m.type == 4 ? "GJ" : "m³";
      const char* dc = m.type == 2 ? "water" : (m.type == 1 ? "gas" : "");
      char lbl[96];
      snprintf(lbl, sizeof(lbl), "%s %lu %s", tl, (unsigned long)sn, store_->name(m.type, sn, 1, m.twoTariff));
      haSensor(ip.c_str(), sn, "t1", lbl, u, dc, "total_increasing", 60);
      if (m.twoTariff) {
        snprintf(lbl, sizeof(lbl), "%s %lu %s", tl, (unsigned long)sn, store_->name(m.type, sn, 2, true));
        haSensor(ip.c_str(), sn, "t2", lbl, u, dc, "total_increasing", 60);
         snprintf(lbl, sizeof(lbl), "%s %lu ОБ", tl, (unsigned long)sn);
         haSensor(ip.c_str(), sn, "total", lbl, u, dc, "total_increasing", 60);
      }
      if (m.hasTemp) {
        snprintf(lbl, sizeof(lbl), "%s %lu температура", tl, (unsigned long)sn);
        haSensor(ip.c_str(), sn, "temp", lbl, "°C", "temperature", "measurement", 300);
      }
      snprintf(lbl, sizeof(lbl), "%s %lu батарея", tl, (unsigned long)sn);
      haSensor(ip.c_str(), sn, "battery", lbl, "%", "battery", "measurement", 300);
      snprintf(lbl, sizeof(lbl), "%s %lu сигнал", tl, (unsigned long)sn);
      haSensor(ip.c_str(), sn, "rssi", lbl, "dBm", "signal_strength", "measurement", 300);
      snprintf(lbl, sizeof(lbl), "%s %lu статус", tl, (unsigned long)sn);
      haSensor(ip.c_str(), sn, "status", lbl, "", "", "", 60);
      snprintf(lbl, sizeof(lbl), "%s %lu данные, с", tl, (unsigned long)sn);
      haSensor(ip.c_str(), sn, "age_s", lbl, "s", "", "measurement", 60);
    }
    server_.sendContent("binary_sensor:\n");
    for (uint8_t i = 0; i < meters_->count(); i++) {
      MeterData m;
      if (!meters_->get(i, m) || !want(m)) continue;
      const char* tl = m.type == 1 ? "Газ" : (m.type == 4 ? "Тепло" : "Вода");
      char b2[360];
      snprintf(b2, sizeof(b2),
               "  - platform: rest\n"
               "    name: \"%s %lu проблема\"\n"
               "    unique_id: svt15_%lu_problem\n"
               "    resource: http://%s/api/ha\n"
               "    scan_interval: 60\n"
               "    value_template: \"{{ value_json.meters['%lu'].status != 'OK' }}\"\n"
               "    device_class: problem\n",
               tl, (unsigned long)m.id, (unsigned long)m.id, ip.c_str(), (unsigned long)m.id);
      server_.sendContent(b2);
    }
    server_.sendContent("");
  }

  // ---------- Сброс настроек ----------
  void handleResetPage(const char* msg) {
    pageHead();
    char b[300];
    server_.sendContent("<div class=\"box\">");
    if (msg && msg[0]) { snprintf(b, sizeof(b), "<p><b>%s</b></p>", msg); server_.sendContent(b); }
    server_.sendContent("<h2>Сброс настроек</h2>"
                        "<p class=\"mut\">Что сбрасывается:</p>"
                        "<p>Вариант «Только настройки»:</p>"
                        "<label>• точка доступа и домашний Wi-Fi (вернутся заводские SSID и пароль)</label>"
                        "<label>• время и часовой пояс, NTP</label>"
                        "<label>• тревоги, пороги и напоминания</label>"
                        "<label>• Telegram: токен, ID чата, отправка</label>"
                        "<label>• названия тарифов и настройки отправки по счётчикам</label>"
                        "<label>• срок хранения истории</label>"
                        "<p>Вариант «Полный сброс» дополнительно удаляет:</p>"
                        "<label>• список счётчиков</label>"
                        "<label>• всю историю</label>"
                        "<p class=\"mut\">Плата перезагрузится через 2 секунды. Чтобы продолжить, введите RESET.</p>"
                        "<form method=\"POST\" action=\"/reset/do\">"
                        "<label><input type=\"radio\" name=\"mode\" value=\"settings\" checked style=\"width:auto;margin:0 6px 0 0\">Только настройки</label>"
                        "<label><input type=\"radio\" name=\"mode\" value=\"full\" style=\"width:auto;margin:0 6px 0 0\">Полный сброс (с счётчиками и историей)</label>"
                        "<label>Подтверждение: введите RESET</label>"
                        "<input name=\"confirm\" autocomplete=\"off\">"
                        "<button type=\"submit\">Сбросить</button></form></div>");
    pageFoot();
  }

  void handleResetDo() {
    if (server_.arg("confirm") != "RESET") {
      handleResetPage("Не сброшено: введите RESET в поле подтверждения.");
      return;
    }
    bool full = server_.arg("mode") == "full";
    static const char* const settingsNs[] = {"svt15wifi", "svt15time", "svt15alm", "svt15hist", "svt15tg", "svt15mcfg"};
    for (const char* ns : settingsNs) {
      Preferences p;
      if (p.begin(ns, false)) { p.clear(); p.end(); }
    }
    if (full) {
      Preferences p;
      if (p.begin("svt15", false)) { p.clear(); p.end(); }
      if (LittleFS.exists(SVT_HIST_FILE)) LittleFS.remove(SVT_HIST_FILE);
    }
    SVT_SERIAL.println(full ? "WEB RESET: full" : "WEB RESET: settings");
    resetPending_ = true;
    resetAt_ = millis() + 2000UL;
    handleResetPage(full ? "Полный сброс выполнен. Плата перезагрузится через 2 секунды."
                         : "Настройки сброшены. Плата перезагрузится через 2 секунды.");
  }

  // Библиотека QR для браузера (файл из прошивки)
  void handleQrJs() {
    server_.sendHeader("Cache-Control", "max-age=86400");
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.send(200, "application/javascript; charset=utf-8", "");
    server_.sendContent(SVT_QRJS, sizeof(SVT_QRJS) - 1);
    server_.sendContent("");
  }

  void handleTelegramDetect() {
    String r;
    tg_->detectChat(r);
    handleTelegramPage(r.c_str());
  }

  void handleTelegramCheck() {
    String r;
    tg_->checkToken(r);
    handleTelegramPage(r.c_str());
  }

  void handleTelegramTest() {
    handleTelegramPage(tg_->sendTest() ? "Тестовое сообщение поставлено в очередь."
                                       : "Не отправлено: Telegram выключен или не настроен.");
  }

  void handleTelegramNow() {
    tg_->sendAllNow();
    handleTelegramPage("Показания всех счётчиков с включённой отправкой поставлены в очередь.");
  }

  // QR в виде монохромного BMP, собранного на плате без библиотек (модуль 6 точек, тихая зона 4 модуля)
  // ---------- Тревоги и напоминания ----------
  void handleAlarmsPage(const char* msg) {
    pageHead();
    char b[480], rs[24];
    server_.sendContent("<div class=\"box\" style=\"max-width:none\">");
    if (msg && msg[0]) {
      snprintf(b, sizeof(b), "<p><b>%s</b></p>", msg);
      server_.sendContent(b);
    }
    server_.sendContent("<h2>Активные тревоги</h2><div class=\"wrap\"><table><thead><tr><th>Счётчик</th>"
                        "<th>Тревога</th><th>Причина</th><th>Состояние</th><th></th></tr></thead><tbody>");
    uint8_t mc = meters_->count();
    uint32_t now = millis();
    int rows = 0;
    for (uint8_t i = 0; i < mc; i++) {
      MeterData m;
      if (!meters_->get(i, m)) continue;
      for (uint8_t t = 0; t < AL_COUNT; t++) {
        const AlarmState& s = al_->state(i, t);
        if (!s.active) continue;
        al_->reasonText(i, t, rs, sizeof(rs));
        const char* st = s.acked ? "подтверждена" : (al_->suppressed(t, now) ? "подавлена" : "новая");
        snprintf(b, sizeof(b), "<tr><td>%c%lu</td><td>%s</td><td>%s</td><td>%s</td><td>",
                 svt15TypeChar(m.type), (unsigned long)m.id, SVT_ALARM_NAME[t], rs, st);
        server_.sendContent(b);
        if (!s.acked) {
          snprintf(b, sizeof(b), "<form method=\"POST\" action=\"/alarms/ack\"><input type=\"hidden\" name=\"m\" value=\"%u\">"
                                 "<input type=\"hidden\" name=\"t\" value=\"%u\"><button type=\"submit\">Подтвердить</button></form>", i, t);
          server_.sendContent(b);
        }
        server_.sendContent("</td></tr>");
        rows++;
      }
    }
    if (rows == 0) server_.sendContent("<tr><td colspan=\"5\">Активных тревог нет</td></tr>");
    server_.sendContent("</tbody></table></div>");
    if (rows > 0) {
      server_.sendContent("<form method=\"POST\" action=\"/alarms/ackall\"><button type=\"submit\">Подтвердить все</button></form>");
    }
    server_.sendContent("<p class=\"mut\">На экране тревога показывается мигающей рамкой в правом верхнем углу. "
                        "Подтверждение кнопкой: одна кнопка подтверждает показанную, UP+DOWN вместе все. "
                        "После подтверждения рамка не мигает, в этом списке тревога остаётся активной, "
                        "пока условие не исчезнет; если оно повторится, будет новая тревога.</p>");

    // ---- подавление ----
    server_.sendContent("<h2>Подавление</h2><p class=\"mut\">Подавленная тревога не показывается на экране, "
                        "но обнаружение продолжается и в списке она остаётся активной. После перезагрузки подавление снимается.</p>");
    for (uint8_t t = 0; t < AL_COUNT; t++) {
      uint32_t left = al_->suppressLeftMin(t);
      snprintf(b, sizeof(b), "<form method=\"POST\" action=\"/alarms/suppress\" style=\"margin-bottom:12px\">"
                             "<input type=\"hidden\" name=\"t\" value=\"%u\"><b>%s</b>", t, SVT_ALARM_NAME[t]);
      server_.sendContent(b);
      if (left > 0) {
        snprintf(b, sizeof(b), " <span class=\"mut\">(подавлена, осталось %lu мин)</span>", (unsigned long)left);
        server_.sendContent(b);
      }
      server_.sendContent("<br><select name=\"hp\" style=\"width:auto\"><option value=\"1\">1 ч</option><option value=\"3\">3 ч</option>"
                          "<option value=\"6\">6 ч</option><option value=\"12\">12 ч</option><option value=\"24\">24 ч</option>"
                          "<option value=\"0\">своё число часов</option></select> "
                          "<input name=\"hc\" type=\"number\" min=\"1\" max=\"999\" placeholder=\"часов\" style=\"width:90px;display:inline-block\"> "
                          "<button type=\"submit\">Подавить</button> <button type=\"submit\" name=\"clear\" value=\"1\">Снять</button></form>");
    }

    // ---- пороги ----
    const AlarmSettings& c = al_->cfg;
    server_.sendContent("<h2>Пороги и напоминания</h2><form method=\"POST\" action=\"/alarms/remind\" style=\"margin-bottom:12px\"><button type=\"submit\">Подавить напоминание на этот месяц</button></form><form method=\"POST\" action=\"/alarms/cfg\">");
    for (uint8_t t = 0; t < AL_COUNT; t++) {
      snprintf(b, sizeof(b), "<label><input type=\"checkbox\" name=\"en%u\" value=\"1\" style=\"width:auto;margin:0 6px 0 0\"%s>%s</label>",
               t, (c.mask & (1u << t)) ? " checked" : "", SVT_ALARM_NAME[t]);
      server_.sendContent(b);
    }
    snprintf(b, sizeof(b), "<label>Низкий заряд: порог, %% (0-100)</label><input name=\"lowbat\" type=\"number\" min=\"0\" max=\"100\" value=\"%u\">", c.lowBat);
    server_.sendContent(b);
    snprintf(b, sizeof(b), "<label>Высокая температура: порог, °C (-20...150)</label><input name=\"temp\" type=\"number\" min=\"-20\" max=\"150\" value=\"%d\">", c.highTemp);
    server_.sendContent(b);
    snprintf(b, sizeof(b), "<label>Нет данных: часов (1-999)</label><input name=\"nodata\" type=\"number\" min=\"1\" max=\"999\" value=\"%u\">", c.noDataH);
    server_.sendContent(b);
    snprintf(b, sizeof(b), "<label>Нет расхода: часов (1-999; отсчёт от запуска платы)</label><input name=\"noflow\" type=\"number\" min=\"1\" max=\"999\" value=\"%u\">", c.noFlowH);
    server_.sendContent(b);
    snprintf(b, sizeof(b), "<label>Суточный лимит, тариф 1, литров (0: выключен)</label><input name=\"lim1\" type=\"number\" min=\"0\" max=\"100000\" value=\"%lu\">", (unsigned long)c.limit1);
    server_.sendContent(b);
    snprintf(b, sizeof(b), "<label>Суточный лимит, тариф 2, литров (0: выключен)</label><input name=\"lim2\" type=\"number\" min=\"0\" max=\"100000\" value=\"%lu\">", (unsigned long)c.limit2);
    server_.sendContent(b);
    snprintf(b, sizeof(b), "<label>Слабый сигнал: порог, dBm (-120...0)</label><input name=\"weak\" type=\"number\" min=\"-120\" max=\"0\" value=\"%d\">", c.weakDbm);
    server_.sendContent(b);
    snprintf(b, sizeof(b), "<label><input type=\"checkbox\" name=\"remen\" value=\"1\" style=\"width:auto;margin:0 6px 0 0\"%s>Ежемесячное напоминание передать показания</label>", c.remEnabled ? " checked" : "");
    server_.sendContent(b);
    snprintf(b, sizeof(b), "<label>День месяца (1-28)</label><input name=\"remday\" type=\"number\" min=\"1\" max=\"28\" value=\"%u\">", c.remDay);
    server_.sendContent(b);
    snprintf(b, sizeof(b), "<label>С часа (0-23)</label><input name=\"remfrom\" type=\"number\" min=\"0\" max=\"23\" value=\"%u\">", c.remFrom);
    server_.sendContent(b);
    snprintf(b, sizeof(b), "<label>До часа (1-24)</label><input name=\"remto\" type=\"number\" min=\"1\" max=\"24\" value=\"%u\">", c.remTo);
    server_.sendContent(b);
    server_.sendContent("<button type=\"submit\">Сохранить</button></form>"
                        "<p class=\"mut\">Суточный лимит и напоминание работают при установленном времени. "
                        "Лимит считается от первых показаний за сутки (после перезагрузки отсчёт начинается заново).</p></div>");
    pageFoot();
  }

  void handleRemindDismiss() {
    if (al_->dismissReminderThisMonth()) handleAlarmsPage("Напоминание подавлено на этот месяц.");
    else handleAlarmsPage("Не подавлено: сначала установите время (откройте страницу «Время» или включите NTP).");
  }

  void handleAlarmAck() {
    al_->ackAlarm((uint8_t)server_.arg("m").toInt(), (uint8_t)server_.arg("t").toInt());
    handleAlarmsPage("Тревога подтверждена.");
  }

  void handleAlarmSuppress() {
    int t = server_.arg("t").toInt();
    if (t < 0 || t >= AL_COUNT) {
      handleAlarmsPage("Ошибка: неизвестный тип тревоги.");
      return;
    }
    if (server_.arg("clear").length() > 0) {
      al_->suppress((uint8_t)t, 0);
      handleAlarmsPage("Подавление снято.");
      return;
    }
    int hp = server_.arg("hp").toInt();
    int hours = hp > 0 ? hp : (int)server_.arg("hc").toInt();
    if (hours < 1 || hours > 999) {
      handleAlarmsPage("Ошибка: число часов от 1 до 999.");
      return;
    }
    al_->suppress((uint8_t)t, (uint32_t)hours);
    handleAlarmsPage("Тревога подавлена.");
  }

  void handleAlarmCfg() {
    long lowbat = server_.arg("lowbat").toInt(), temp = server_.arg("temp").toInt();
    long nodata = server_.arg("nodata").toInt(), noflow = server_.arg("noflow").toInt();
    long lim1 = server_.arg("lim1").toInt(), lim2 = server_.arg("lim2").toInt();
    long weak = server_.arg("weak").toInt();
    long remday = server_.arg("remday").toInt(), remfrom = server_.arg("remfrom").toInt(), remto = server_.arg("remto").toInt();
    if (lowbat < 0 || lowbat > 100 || temp < -20 || temp > 150 || nodata < 1 || nodata > 999 ||
        noflow < 1 || noflow > 999 || lim1 < 0 || lim1 > 100000 || lim2 < 0 || lim2 > 100000 ||
        weak < -120 || weak > 0 || remday < 1 || remday > 28 || remfrom < 0 || remfrom > 23 ||
        remto < 1 || remto > 24) {
      handleAlarmsPage("Ошибка: значение вне допустимого диапазона. Параметры не изменены.");
      return;
    }
    AlarmSettings& c = al_->cfg;
    uint16_t mask = 0;
    for (uint8_t t = 0; t < AL_COUNT; t++) {
      char name[6];
      snprintf(name, sizeof(name), "en%u", t);
      if (server_.arg(name).length() > 0) mask |= (1u << t);
    }
    c.mask = mask;
    c.lowBat = (uint8_t)lowbat;
    c.highTemp = (int16_t)temp;
    c.noDataH = (uint16_t)nodata;
    c.noFlowH = (uint16_t)noflow;
    c.limit1 = (uint32_t)lim1;
    c.limit2 = (uint32_t)lim2;
    c.weakDbm = (int16_t)weak;
    c.remEnabled = server_.arg("remen").length() > 0;
    c.remDay = (uint8_t)remday;
    c.remFrom = (uint8_t)remfrom;
    c.remTo = (uint8_t)remto;
    al_->save();
    handleAlarmsPage("Сохранено.");
  }

  // ---------- История ----------
  // Настройка периодических записей истории
  void historyCfgForm() {
    char b[520];
    snprintf(b, sizeof(b),
             "<div class=\"box\"><h2>Запись истории</h2><form method=\"POST\" action=\"/history/cfg\">"
             "<label><input type=\"checkbox\" name=\"per\" value=\"1\"%s>Сохранять показания через каждые N часов</label>"
             "<label>Интервал, часов (1–12)</label><input name=\"ivl\" type=\"number\" min=\"1\" max=\"12\" value=\"%u\">"
             "<button type=\"submit\">Сохранить настройку</button></form>"
             "<p class=\"mut\">Кроме этого, каждый день в 23:55 всегда сохраняется запись дня. Время записей местное.</p></div>",
             hist_->periodicOn() ? " checked" : "", (unsigned)hist_->interval());
    server_.sendContent(b);
  }

  void handleHistoryCfg() {
    bool on = server_.arg("per").length() > 0;
    int hours = server_.arg("ivl").toInt();
    if (hours >= 1 && hours <= 12) hist_->setPeriodic(on, (uint8_t)hours);
    handleHistoryPage();
  }

  void handleHistoryPage() {
    pageHead();
    historyCfgForm();
    server_.sendContent(R"hist(
<h2>История</h2>
<div class="mut" id="hinfo">Загрузка...</div>
<p><button data-d="1">24 часа</button> <button data-d="7">7 дней</button>
<button data-d="30">30 дней</button> <button data-d="60">60 дней</button></p>
<p>С <input type="date" id="from" style="width:auto"> по <input type="date" id="to" style="width:auto">
SN <input id="sn" placeholder="все" style="width:90px"> <button id="show">Показать</button>
<a id="dl" href="#">Скачать CSV</a></p>
<div class="wrap"><table id="ht"><thead><tr><th>Дата</th><th>SN</th><th>Тип</th><th>Тариф 1</th><th>Тариф 2</th>
<th>Темп.</th><th>Батарея</th><th>RSSI</th><th>Возраст, ч</th><th>Время</th></tr></thead><tbody></tbody></table></div>
<script>
const TN={1:'Газ',2:'Вода',4:'Тепло'};
function loc(d){return d.getFullYear()+'-'+String(d.getMonth()+1).padStart(2,'0')+'-'+String(d.getDate()).padStart(2,'0');}
function setRange(days){const t=new Date();const f=new Date(t.getTime()-days*86400000);
 document.getElementById('from').value=loc(f);document.getElementById('to').value=loc(t);}
async function show(){
 const f=document.getElementById('from').value,t=document.getElementById('to').value,sn=document.getElementById('sn').value.trim();
 const q='from='+f+'&to='+t+(sn?'&sn='+encodeURIComponent(sn):'');
 document.getElementById('dl').href='/history.csv?'+q+'&dl=1';
 const info=document.getElementById('hinfo');info.textContent='Загрузка...';
 try{const r=await fetch('/history.csv?'+q);
  if(!r.ok){info.textContent='Файловая система недоступна';return;}
  const rows=(await r.text()).trim().split('\n').slice(1).filter(x=>x).reverse();
  const tb=document.querySelector('#ht tbody');tb.innerHTML='';
  rows.slice(0,500).forEach(l=>{const c=l.split(',');const tr=tb.insertRow();
   [c[0],c[1],TN[c[2]]||c[2],c[3]||'—',c[4]||'—',c[5]?c[5]+' °C':'—',c[6]+' %',c[7],c[8]==='999'?'нет данных':(c[8]||'—'),c[9]||'—'].forEach(x=>tr.insertCell().textContent=x);});
  info.textContent='Записей: '+rows.length+(rows.length>500?' (показаны последние 500, весь список в CSV)':'');
 }catch(e){info.textContent='Нет связи с устройством';}
}
document.querySelectorAll('button[data-d]').forEach(b=>b.onclick=()=>{setRange(+b.dataset.d);show();});
document.getElementById('show').onclick=show;
setRange(30);show();
</script>
)hist");
    pageFoot();
  }

  static void csvSend(void* ctx, const char* d, size_t n) {
    ((WebServer*)ctx)->sendContent(d, n);
  }

  void handleCsv() {
    if (!hist_->ok()) {
      server_.send(503, "text/plain; charset=utf-8", "LittleFS unavailable");
      return;
    }
    String from = server_.arg("from"), to = server_.arg("to"), sn = server_.arg("sn");
    if (server_.arg("dl").length() > 0) server_.sendHeader("Content-Disposition", "attachment; filename=history.csv");
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.send(200, "text/csv; charset=utf-8", "");
    server_.sendContent("date,sn,type,tariff1,tariff2,temp_c,battery_pct,rssi_dbm,age_h,time\n");
    hist_->csvFilter(validDate(from) ? from.c_str() : "0000-00-00",
                     validDate(to) ? to.c_str() : "9999-99-99",
                     sn.length() > 0 ? (long)sn.toInt() : -1, csvSend, &server_);
    server_.sendContent("");
  }

  // ---------- Память ----------
  void handleMemoryPage(const char* msg) {
    pageHead();
    char b[400];
    server_.sendContent("<div class=\"box\">");
    if (msg && msg[0]) {
      snprintf(b, sizeof(b), "<p><b>%s</b></p>", msg);
      server_.sendContent(b);
    }
    server_.sendContent("<h2>Память истории (LittleFS)</h2>");
    if (!hist_->ok()) {
      server_.sendContent("<p>Файловая система недоступна: история не ведётся.</p></div>");
      pageFoot();
      return;
    }
    size_t tot = hist_->total(), use = hist_->used();
    unsigned pct = tot ? (unsigned)((uint64_t)use * 100 / tot) : 0;
    snprintf(b, sizeof(b), "<p>Всего %u КБ, занято %u КБ (%u%%), свободно %u КБ.</p>",
             (unsigned)(tot / 1024), (unsigned)(use / 1024), pct, (unsigned)((tot - use) / 1024));
    server_.sendContent(b);
    if (pct >= 90) {
      server_.sendContent("<p><b>Занято 90% и больше: удалите старые данные или уменьшите срок хранения.</b></p>");
    }
    snprintf(b, sizeof(b),
             "<form method=\"POST\" action=\"/memory/days\"><label>Срок хранения истории, дней (1-365)</label>"
             "<input name=\"days\" type=\"number\" min=\"1\" max=\"365\" value=\"%u\">"
             "<button type=\"submit\">Сохранить</button></form>", (unsigned)hist_->days());
    server_.sendContent(b);
    server_.sendContent("<form method=\"POST\" action=\"/memory/clean\"><label>Удалить записи старше даты</label>"
                        "<input name=\"before\" type=\"date\"><label><input type=\"checkbox\" name=\"yes\" value=\"1\" "
                        "style=\"width:auto;margin:0 6px 0 0\">Подтверждаю удаление</label>"
                        "<button type=\"submit\">Удалить</button></form>"
                        "<p class=\"mut\">Запись делается раз в сутки в 23:55 по местному времени, по одной строке на счётчик. "
                        "Для удаления всей истории укажите дату в будущем.</p></div>");
    pageFoot();
  }

  void handleMemoryDays() {
    int d = server_.arg("days").toInt();
    if (d < 1 || d > 365) {
      handleMemoryPage("Ошибка: срок хранения от 1 до 365 дней.");
      return;
    }
    hist_->setDays((uint16_t)d);
    handleMemoryPage("Сохранено.");
  }

  void handleMemoryClean() {
    String before = server_.arg("before");
    if (!validDate(before)) {
      handleMemoryPage("Ошибка: укажите дату.");
      return;
    }
    if (server_.arg("yes").length() == 0) {
      handleMemoryPage("Отметьте подтверждение удаления.");
      return;
    }
    handleMemoryPage(hist_->trimBefore(before.c_str()) ? "Старые записи удалены." : "Не удалось удалить записи.");
  }

  WebServer server_;
  MeterManager* meters_ = nullptr;
  WifiManager* wifi_ = nullptr;
  TimeManager* clk_ = nullptr;
  HistoryManager* hist_ = nullptr;
  AlertManager* al_ = nullptr;
  bool resetPending_ = false;      // после сброса: перезапуск платы
  uint32_t resetAt_ = 0;
  MeterCfgStore* store_ = nullptr;
  TelegramManager* tg_ = nullptr;
  bool restartAp_ = false;
  uint32_t restartAt_ = 0;
  bool applySta_ = false;
  uint32_t applyStaAt_ = 0;
};
