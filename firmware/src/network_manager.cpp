#include "network_manager.h"
#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <time.h>

static WebServer server(80);
static MeterManager* manager=nullptr;
static bool started=false;

static const char PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SVT-15 Monitor</title><style>
body{font:16px system-ui;margin:0;background:#101827;color:#edf2f7}header{padding:18px;background:#172337;position:sticky;top:0}
main{max-width:900px;margin:auto;padding:14px}.meter{background:#1b293d;border-radius:12px;padding:14px;margin:12px 0}
h1{font-size:22px;margin:0}h2{font-size:18px;margin:0 0 8px}.muted{color:#aab8ca;font-size:13px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:8px;margin-top:12px}
label{display:flex;align-items:center;gap:8px;background:#26364c;padding:9px;border-radius:8px}input{accent-color:#5eead4;width:18px;height:18px}
button{padding:10px 14px;border:0;border-radius:8px;background:#0f766e;color:white;font-weight:600}
</style></head><body><header><h1>Elehant SVT-15 Monitor</h1><div class="muted">Локальная сеть устройства</div></header><main>
<p id="status">Загрузка данных…</p><section class="meter"><h2>Состояние и история</h2><p id="sys" class="muted">Загрузка…</p><a href="/history.csv" style="color:#5eead4">Скачать историю CSV</a></section><div id="meters"></div></main>
<script>
const flags=['active','collect','history','lcd','web','telegram','totals','calculator','alarms'];
const labels=['Активен','Сбор данных','История','Экран','WEB','Telegram','Итоги','Калькулятор','Тревоги'];
async function refresh(){try{let r=await fetch('/api/meters');if(!r.ok)throw Error('HTTP '+r.status);let d=await r.json();document.getElementById('status').textContent='Счётчиков: '+d.meters.length+' / '+d.max;
document.getElementById('meters').innerHTML=d.meters.map((m,i)=>'<section class="meter"><h2>'+m.serial+' · '+m.typeName+'</h2><div class="muted">Модель '+m.model+' · '+m.rssi+' dBm · '+(m.battery===null?'батарея —':('батарея '+m.battery+'%'))+'</div><p>'+m.readings.map(x=>x.key+': '+x.value+' '+x.unit).join('<br>')+'</p><div class="grid">'+flags.map((f,j)=>'<label><input type="checkbox" '+((m.flags&(1<<j))?'checked':'')+' onchange="setFlag('+i+',\''+f+'\',this.checked)"><span>'+labels[j]+'</span></label>').join('')+'</div></section>').join('')||'<section class="meter">Пока нет счётчиков. Ожидается BLE-реклама.</section>';
}catch(e){document.getElementById('status').textContent='Ошибка: '+e.message}}
async function setFlag(i,f,v){try{let r=await fetch('/api/flag?index='+i+'&flag='+f+'&value='+(v?1:0),{method:'POST'});if(!r.ok)throw Error('HTTP '+r.status);refresh()}catch(e){alert('Не удалось сохранить настройку: '+e.message);refresh()}}
async function status(){try{let r=await fetch('/api/status');let d=await r.json();document.getElementById('sys').textContent='Время: '+d.time+' · uptime '+d.uptime+' с · свободная RAM '+d.heap+' байт · LittleFS '+d.fsUsed+'/'+d.fsTotal+' байт · клиентов Wi-Fi '+d.clients;}catch(e){}}
refresh();status();setInterval(refresh,5000);setInterval(status,15000);
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
  char timeText[32]="не синхронизировано";
  if (now>1760000000 && localtime_r(&now,&tmNow)) strftime(timeText,sizeof(timeText),"%Y-%m-%d %H:%M:%S",&tmNow);
  String out="{\"time\":\""+String(timeText)+"\",\"uptime\":"+String(millis()/1000)+
    ",\"heap\":"+String(ESP.getFreeHeap())+",\"fsUsed\":"+String(LittleFS.usedBytes())+
    ",\"fsTotal\":"+String(LittleFS.totalBytes())+",\"clients\":"+String(WiFi.softAPgetStationNum())+"}";
  server.send(200,"application/json; charset=utf-8",out);
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
  server.on("/api/status",HTTP_GET,handleStatus);
  server.on("/api/meters",HTTP_GET,handleMeters);
  server.on("/api/flag",HTTP_POST,handleFlag);
  server.onNotFound([](){server.send(404,"application/json","{\"error\":\"not_found\"}");});
  server.begin();
  started=true;
}
void networkLoop(){if(started)server.handleClient();}
void networkRestartAp(){
  if(!manager)return;
  WiFi.softAPdisconnect(true);
  delay(200);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0));
  WiFi.softAP(AP_DEFAULT_SSID,AP_DEFAULT_PASSWORD);
  Serial.printf("Wi-Fi AP restarted: %s\n",WiFi.softAPIP().toString().c_str());
}
