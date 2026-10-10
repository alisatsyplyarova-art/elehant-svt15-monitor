#include "network_manager.h"
#include "alarm_manager.h"
#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include <math.h>
#include <LittleFS.h>
#include <Update.h>
#include <esp_system.h>
#include <time.h>

static WebServer server(80);
static MeterManager* manager=nullptr;
static bool started=false;
static String otaPassword;
static bool otaUploadOk=false;
static bool otaUploadAuthorized=false;

static void loadOtaPassword() {
  Preferences p;
  if (!p.begin("svt15ota", false)) {
    Serial.println(F("OTA disabled: cannot open credential storage."));
    return;
  }
  otaPassword=p.getString("password", "");
  if (otaPassword.length()!=16) {
    static const char hex[]="0123456789abcdef";
    char generated[17];
    for (int block=0; block<2; ++block) {
      const uint32_t value=esp_random();
      for (int nibble=0; nibble<8; ++nibble)
        generated[block*8+nibble]=hex[(value >> (28-4*nibble)) & 0x0F];
    }
    generated[16]='\0';
    otaPassword=generated;
    p.putString("password", otaPassword);
    if (p.getString("password", "").length()!=16) {
      otaPassword="";
      Serial.println(F("OTA disabled: failed to persist update password."));
      p.end();
      return;
    }
    Serial.println(F("Generated a new OTA password; it is printed below."));
  }
  p.end();
  Serial.printf("OTA update: http://192.168.4.1/update  username=admin  password=%s\n", otaPassword.c_str());
}
static bool otaClientOnAccessPoint() {
  const IPAddress remote=server.client().remoteIP();
  return remote[0]==192 && remote[1]==168 && remote[2]==4;
}
static bool authorizeOta() {
  if (otaPassword.length()==16 && otaClientOnAccessPoint() &&
      server.authenticate("admin", otaPassword.c_str())) return true;
  server.requestAuthentication();
  return false;
}
static void handleOtaPage() {
  if (!authorizeOta()) return;
  static const char page[] PROGMEM =
    "<!doctype html><html lang='ru'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Обновление SVT-15</title><body style='font:16px system-ui;max-width:600px;margin:2em auto;padding:0 1em'>"
    "<h2>Обновление прошивки ESP32</h2><p>Выберите собранный для этой платы файл .bin. Не отключайте питание во время обновления.</p>"
    "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin' required>"
    "<p><button type='submit'>Загрузить и установить</button></p></form><p><a href='/'>Назад</a></p></body></html>";
  server.send_P(200,"text/html; charset=utf-8",page);
}
static void handleOtaUpload() {
  HTTPUpload& upload=server.upload();
  if (otaPassword.length()!=16 || !otaClientOnAccessPoint() || !server.authenticate("admin", otaPassword.c_str())) {
    otaUploadAuthorized=false;
    otaUploadOk=false;
    return;
  }
  otaUploadAuthorized=true;
  if (upload.status==UPLOAD_FILE_START) {
    otaUploadOk=Update.begin(UPDATE_SIZE_UNKNOWN);
    if (!otaUploadOk) Update.printError(Serial);
    else Serial.printf("OTA upload started: %s\n", upload.filename.c_str());
  } else if (upload.status==UPLOAD_FILE_WRITE) {
    if (!otaUploadOk || Update.write(upload.buf, upload.currentSize)!=upload.currentSize) {
      otaUploadOk=false;
      Update.printError(Serial);
    }
  } else if (upload.status==UPLOAD_FILE_END) {
    if (otaUploadOk) {
      otaUploadOk=Update.end(true);
      if (!otaUploadOk) Update.printError(Serial);
      else Serial.printf("OTA upload complete: %u bytes\n", upload.totalSize);
    }
  } else if (upload.status==UPLOAD_FILE_ABORTED) {
    otaUploadOk=false;
    Update.abort();
  }
}
static void handleOtaResult() {
  if (!authorizeOta()) return;
  if (!otaUploadAuthorized || !otaUploadOk || Update.hasError()) {
    server.send(500,"text/plain; charset=utf-8","Обновление не выполнено. Проверьте Serial Monitor и размер/тип .bin.");
    return;
  }
  server.send(200,"text/plain; charset=utf-8","Прошивка записана. Устройство перезапускается...");
  delay(400);
  ESP.restart();
}

static void handleGetAlarms() {
  AlarmSettings s=alarmSettingsGet();
  String out="{\"battery\":"+String(s.batteryLow)+",\"stale\":"+String(s.staleMinutes)+
    ",\"lowEnabled\":"+(s.lowEnabled?String("true"):String("false"))+
    ",\"low\":"+String(s.lowValue,4)+",\"highEnabled\":"+(s.highEnabled?String("true"):String("false"))+
    ",\"high\":"+String(s.highValue,4)+"}";
  server.send(200,"application/json; charset=utf-8",out);
}
static void handleSetAlarms() {
  AlarmSettings s;
  if (!server.hasArg("battery") || !server.hasArg("stale") || !server.hasArg("lowEnabled") ||
      !server.hasArg("low") || !server.hasArg("highEnabled") || !server.hasArg("high")) {
    server.send(400,"application/json","{\"error\":\"missing_argument\"}"); return;
  }
  int battery=server.arg("battery").toInt(), stale=server.arg("stale").toInt();
  String lo=server.arg("low"), hi=server.arg("high");
  s.batteryLow=(uint8_t)constrain(battery,0,100);
  s.staleMinutes=(uint16_t)constrain(stale,1,1440);
  s.lowEnabled=server.arg("lowEnabled")=="1";
  s.highEnabled=server.arg("highEnabled")=="1";
  s.lowValue=lo.toFloat(); s.highValue=hi.toFloat();
  if (battery<0 || battery>100 || stale<1 || stale>1440 ||
      (s.lowEnabled && !lo.length()) || (s.highEnabled && !hi.length()) ||
      (s.lowEnabled && s.highEnabled && s.lowValue>=s.highValue) ||
      !alarmSettingsSave(s)) {
    server.send(400,"application/json","{\"error\":\"invalid_settings_or_storage_failure\"}"); return;
  }
  server.send(200,"application/json","{\"ok\":true}");
}
bool networkAlarmActive(uint32_t nowMillis) {
  if (!manager) return false;
  return alarmAnyActive(*manager,nowMillis,alarmSettingsGet());
}

static const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SVT-15 Monitor</title><style>
body{font:16px system-ui;margin:0;background:#101827;color:#edf2f7}header{padding:18px;background:#172337;position:sticky;top:0}
main{max-width:900px;margin:auto;padding:14px}.meter{background:#1b293d;border-radius:12px;padding:14px;margin:12px 0}
h1{font-size:22px;margin:0}h2{font-size:18px;margin:0 0 8px}.muted{color:#aab8ca;font-size:13px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:8px;margin-top:12px}
label{display:flex;align-items:center;gap:8px;background:#26364c;padding:9px;border-radius:8px}input[type=text],input[type=password],input[type=number]{width:auto;height:auto;accent-color:initial}input{accent-color:#5eead4;width:18px;height:18px}
button{padding:10px 14px;border:0;border-radius:8px;background:#0f766e;color:white;font-weight:600}
</style></head><body><header><h1>Elehant SVT-15 Monitor</h1><div class="muted">Локальная сеть устройства</div></header><main>
<p id="status">Загрузка данных…</p><section class="meter"><h2>Состояние и история</h2><p id="sys" class="muted">Загрузка…</p><a href="/history.csv" style="color:#5eead4">Скачать историю CSV</a> · <a href="/update" style="color:#5eead4">Обновить прошивку (OTA)</a><hr><h2 style="margin-top:14px">Wi-Fi роутер и время</h2><p class="muted">Точка доступа SVT-15 останется доступной. Пароль роутера хранится в настройках ESP32.</p><form id="netform" onsubmit="saveNetwork(event)"><p>Имя Wi-Fi (SSID)<br><input id="ssid" maxlength="32" autocomplete="off" style="width:95%;padding:10px"></p><p>Пароль Wi-Fi (если сеть та же, пустое поле сохраняет прежний пароль; для открытой сети оставьте пустым)<br><input id="pass" type="password" maxlength="63" autocomplete="new-password" style="width:95%;padding:10px"></p><p>Часовой пояс относительно UTC, часов (например, 5.5)<br><input id="tz" type="number" min="-12" max="14" step="0.25" value="0" style="width:100px;padding:10px"></p><button type="submit">Сохранить настройки</button></form><p id="netmsg" class="muted"></p></section><div id="meters"></div></main>
<script>
const flags=['active','collect','history','lcd','web','telegram','totals','calculator','alarms'];
const labels=['Активен','Сбор данных','История','Экран','WEB','Telegram','Итоги','Калькулятор','Тревоги'];
async function refresh(){try{let r=await fetch('/api/meters');if(!r.ok)throw Error('HTTP '+r.status);let d=await r.json();document.getElementById('status').textContent='Счётчиков: '+d.meters.length+' / '+d.max;
document.getElementById('meters').innerHTML=d.meters.map((m,i)=>'<section class="meter"><h2>'+m.serial+' · '+m.typeName+'</h2><div class="muted">Модель '+m.model+' · '+m.rssi+' dBm · '+(m.battery===null?'батарея —':('батарея '+m.battery+'%'))+'</div><p>'+m.readings.map(x=>x.key+': '+x.value+' '+x.unit).join('<br>')+'</p><div class="grid">'+flags.map((f,j)=>'<label><input type="checkbox" '+((m.flags&(1<<j))?'checked':'')+' onchange="setFlag('+i+',\''+f+'\',this.checked)"><span>'+labels[j]+'</span></label>').join('')+'</div></section>').join('')||'<section class="meter">Пока нет счётчиков. Ожидается BLE-реклама.</section>';
}catch(e){document.getElementById('status').textContent='Ошибка: '+e.message}}
async function setFlag(i,f,v){try{let r=await fetch('/api/flag?index='+i+'&flag='+f+'&value='+(v?1:0),{method:'POST'});if(!r.ok)throw Error('HTTP '+r.status);refresh()}catch(e){alert('Не удалось сохранить настройку: '+e.message);refresh()}}
async function saveNetwork(e){e.preventDefault();let p=new URLSearchParams();p.set('ssid',document.getElementById('ssid').value);p.set('pass',document.getElementById('pass').value);p.set('tz',document.getElementById('tz').value);try{let r=await fetch('/api/network',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:p.toString()});let d=await r.json();if(!r.ok)throw Error(d.error||r.status);document.getElementById('netmsg').textContent='Сохранено. Подключение к роутеру и синхронизация времени выполняются в фоне.';document.getElementById('pass').value='';loadNetwork();status();}catch(err){document.getElementById('netmsg').textContent='Ошибка: '+err.message}}
async function loadNetwork(){try{let r=await fetch('/api/network');let d=await r.json();document.getElementById('ssid').value=d.ssid||'';document.getElementById('tz').value=d.tz;}catch(e){}}
async function status(){try{let r=await fetch('/api/status');let d=await r.json();document.getElementById('sys').textContent='Время: '+d.time+' · uptime '+d.uptime+' с · свободная RAM '+d.heap+' байт · LittleFS '+d.fsUsed+'/'+d.fsTotal+' байт · клиентов Wi-Fi '+d.clients;}catch(e){}}
refresh();loadNetwork();status();setInterval(refresh,5000);setInterval(status,15000);
</script></body></html>)HTML";

static uint16_t bitForFlag(const String& name) {
  if(name=="active") return METER_ACTIVE;
  if(name=="collect") return METER_COLLECT;
  if(name=="history") return METER_HISTORY;
  if(name=="lcd") return METER_LCD;
  if(name=="web") return METER_WEB;
  if(name=="telegram") return METER_TELEGRAM;
  if(name=="totals") return METER_TOTALS;
  if(name=="calculator") return METER_CALCULATOR;
  if(name=="alarms") return METER_ALARMS;
  return 0;
}
static String jsonEscape(const String& s) {
  String out; out.reserve(s.length()+4);
  for(size_t i=0;i<s.length();++i){char c=s[i]; if(c=='"'||c=='\\')out+='\\'; if((uint8_t)c>=0x20)out+=c;}
  return out;
}

static void handleHistory() {
  if (!LittleFS.exists("/history.csv")) {
    server.send(404,"text/plain; charset=utf-8","История пока пуста");
    return;
  }
  File file=LittleFS.open("/history.csv",FILE_READ);
  if (!file) { server.send(500,"text/plain; charset=utf-8","Не удалось открыть историю"); return; }
  server.sendHeader("Content-Disposition","attachment; filename=svt15-history.csv");
  server.streamFile(file,"text/csv; charset=utf-8");
  file.close();
}
static void handleStatus() {
  time_t now=time(nullptr);
  struct tm tmNow{};
  char timeText[32]="not synchronized";
  if (now>1760000000 && localtime_r(&now,&tmNow)) strftime(timeText,sizeof(timeText),"%Y-%m-%d %H:%M:%S",&tmNow);
  String out="{\"time\":\""+String(timeText)+"\",\"uptime\":"+String(millis()/1000)+
    ",\"heap\":"+String(ESP.getFreeHeap())+",\"fsUsed\":"+String(LittleFS.usedBytes())+
    ",\"fsTotal\":"+String(LittleFS.totalBytes())+",\"clients\":"+String(WiFi.softAPgetStationNum())+",\"sta\":"+String(WiFi.status()==WL_CONNECTED?"true":"false")+",\"staIp\":\""+WiFi.localIP().toString()+"\"}";
  server.send(200,"application/json; charset=utf-8",out);
}


static String makeTimezone(float hours) {
  int total=(int)lroundf(hours*60.0f);
  char buf[24];
  if(total==0) return String("UTC0");
  char sign=total>0?'-':'+';
  int a=abs(total);
  snprintf(buf,sizeof(buf),"UTC%c%d:%02d",sign,a/60,a%60);
  return String(buf);
}
static void applyTimezone(float hours) {
  String tz=makeTimezone(hours);
  setenv("TZ",tz.c_str(),1);
  tzset();
}
static void startNtp(float hours) {
  applyTimezone(hours);
  configTime(0,0,"pool.ntp.org","time.google.com","time.cloudflare.com");
}
static void handleGetNetwork() {
  Preferences p;
  p.begin("svt15net",true);
  String ssid=p.getString("ssid","");
  float tz=p.getFloat("tz",0.0f);
  p.end();
  server.send(200,"application/json; charset=utf-8","{\"ssid\":\""+jsonEscape(ssid)+"\",\"tz\":"+String(tz,2)+"}");
}
static void handleSetNetwork() {
  if(!server.hasArg("ssid") || !server.hasArg("pass") || !server.hasArg("tz")) {
    server.send(400,"application/json","{\"error\":\"missing_argument\"}"); return;
  }
  String ssid=server.arg("ssid");
  String pass=server.arg("pass");
  String tzArg=server.arg("tz");
  if(ssid.length()>32 || pass.length()>63 || !tzArg.length()) {
    server.send(400,"application/json","{\"error\":\"invalid_settings\"}"); return;
  }
  float tz=tzArg.toFloat();
  if(tz < -12.0f || tz > 14.0f) {
    server.send(400,"application/json","{\"error\":\"timezone_out_of_range\"}"); return;
  }
  Preferences p;
  if(!p.begin("svt15net",false)) { server.send(500,"application/json","{\"error\":\"storage_failed\"}"); return; }
  const String oldSsid=p.getString("ssid","");
  const String oldPass=p.getString("pass","");
  if(ssid==oldSsid && pass.length()==0) pass=oldPass;
  p.putString("ssid",ssid);
  p.putString("pass",pass);
  p.putFloat("tz",tz);
  p.end();
  startNtp(tz);
  if(ssid.length()) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid.c_str(),pass.c_str());
  } else {
    WiFi.disconnect(false,false);
    WiFi.mode(WIFI_AP);
  }
  server.send(200,"application/json","{\"ok\":true}");
}
static void loadNetworkSettings() {
  Preferences p;
  p.begin("svt15net",true);
  String ssid=p.getString("ssid","");
  String pass=p.getString("pass","");
  float tz=p.getFloat("tz",0.0f);
  p.end();
  startNtp(tz);
  if(ssid.length()) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid.c_str(),pass.c_str());
  }
}

static void handleRoot(){server.send_P(200,"text/html; charset=utf-8",PAGE);}
static void handleMeters(){
  if(!manager){server.send(503,"application/json","{\"error\":\"not_ready\"}");return;}
  String out="{\"max\":"+String(MAX_METERS)+",\"meters\":[";
  for(uint8_t i=0;i<manager->count();++i){
    const MeterConfig* cfg=manager->configAt(i);
    if (!cfg || !(cfg->flags&METER_ACTIVE) || !(cfg->flags&METER_WEB)) continue;
    if (out[out.length()-1]!='[') out+=",";
    const MeterPacket* p=manager->latestAt(i);
    out+="{\"serial\":"+String(cfg?cfg->serial:0)+",\"type\":"+String(cfg?cfg->type:0)+",\"typeName\":\""+String(cfg&&cfg->type==1?"Газ":cfg&&cfg->type==2?"Вода":cfg&&cfg->type==3?"Электроэнергия":cfg&&cfg->type==4?"Тепло":"Счётчик")+
      "\",\"model\":"+String(cfg?cfg->model:0)+",\"flags\":"+String(cfg?cfg->flags:0)+",\"rssi\":"+String(p?p->rssi:0)+",\"battery\":";
    out+=(p&&p->hasBattery)?String(p->batteryPercent):"null";
    out+=",\"readings\":[";
    if(p){
      bool comma=false;
      for(uint8_t j=0;j<p->fieldCount&&j<12;++j){
        if(comma)out+=",";
        const MeasurementField& f=p->fields[j];
        out+="{\"key\":\""+jsonEscape(String(f.key))+"\",\"value\":"+String(f.value,4)+",\"unit\":\""+jsonEscape(String(f.unit))+"\"}";
        comma=true;
      }
      if(!p->fieldCount&&p->hasReading){
        out+="{\"key\":\"reading\",\"value\":"+String(p->reading,4)+",\"unit\":\""+jsonEscape(p->unit)+"\"}";
      }
    }
    out+="]}";
  }
  out+="]}";
  server.send(200,"application/json; charset=utf-8",out);
}
static void handleFlag(){
  if(!manager){server.send(503,"application/json","{\"error\":\"not_ready\"}");return;}
  if(!server.hasArg("index")||!server.hasArg("flag")||!server.hasArg("value")){server.send(400,"application/json","{\"error\":\"missing_argument\"}");return;}
  const int idx=server.arg("index").toInt();
  const uint16_t bit=bitForFlag(server.arg("flag"));
  if(idx<0||idx>=manager->count()||!bit){server.send(400,"application/json","{\"error\":\"invalid_argument\"}");return;}
  manager->setFlagAt((uint8_t)idx,bit,server.arg("value")=="1");
  server.send(200,"application/json","{\"ok\":true}");
}
void networkBegin(MeterManager& meters){
  manager=&meters;
  WiFi.mode(WIFI_AP);
  IPAddress ip(192,168,4,1), gateway(192,168,4,1), subnet(255,255,255,0);
  WiFi.softAPConfig(ip,gateway,subnet);
  const bool ok=WiFi.softAP(AP_DEFAULT_SSID,AP_DEFAULT_PASSWORD);
  Serial.printf("Wi-Fi AP %s: %s, IP=%s\n",AP_DEFAULT_SSID,ok?"started":"FAILED",WiFi.softAPIP().toString().c_str());
  server.on("/",HTTP_GET,handleRoot);
  server.on("/history.csv",HTTP_GET,handleHistory);
  server.on("/update",HTTP_GET,handleOtaPage);
  server.on("/update",HTTP_POST,handleOtaResult,handleOtaUpload);
  server.on("/api/status",HTTP_GET,handleStatus);
  server.on("/api/network",HTTP_GET,handleGetNetwork);
  server.on("/api/network",HTTP_POST,handleSetNetwork);
  server.on("/api/meters",HTTP_GET,handleMeters);
  server.on("/api/flag",HTTP_POST,handleFlag);
  server.onNotFound([](){server.send(404,"application/json","{\"error\":\"not_found\"}");});
  server.begin();
  loadOtaPassword();
  loadNetworkSettings();
  started=true;
}
void networkLoop(){if(started)server.handleClient();}
void networkRestartAp(){
  if(!manager)return;
  WiFi.softAPdisconnect(true);
  delay(200);
  WiFi.mode(WiFi.status()==WL_CONNECTED?WIFI_AP_STA:WIFI_AP);
  WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0));
  WiFi.softAP(AP_DEFAULT_SSID,AP_DEFAULT_PASSWORD);
  loadNetworkSettings();
  Serial.printf("Wi-Fi AP restarted: %s\n",WiFi.softAPIP().toString().c_str());
}
