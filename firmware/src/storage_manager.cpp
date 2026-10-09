#include "storage_manager.h"
#include <Preferences.h>
#include <LittleFS.h>
#include <time.h>

static const char* HISTORY_PATH="/history.csv";
static const uint8_t STORAGE_SCHEMA=1;

bool StorageManager::begin() {
  ready_=LittleFS.begin(true);
  return ready_;
}
bool StorageManager::loadMeters(MeterConfig* meters, uint8_t capacity, uint8_t& count) {
  count=0;
  if (!meters || !capacity) return false;
  Preferences p;
  if (!p.begin("svt15", true)) return false;
  const uint8_t schema=p.getUChar("schema",0);
  const uint8_t saved=p.getUChar("meterCount",0);
  const size_t bytes=p.getBytesLength("meters");
  if (schema==STORAGE_SCHEMA && saved<=capacity && bytes==sizeof(MeterConfig)*saved) {
    count=(uint8_t)p.getBytes("meters",meters,bytes);
    // Preferences::getBytes returns byte count, not element count.
    count=(uint8_t)saved;
  }
  p.end();
  return true;
}
bool StorageManager::saveMeters(const MeterConfig* meters, uint8_t count) {
  if (!meters || count>MAX_METERS) return false;
  Preferences p;
  if (!p.begin("svt15",false)) return false;
  bool ok=p.putUChar("schema",STORAGE_SCHEMA)>0;
  ok=(p.putUChar("meterCount",count)>0) && ok;
  const size_t bytes=sizeof(MeterConfig)*count;
  if (bytes) ok=(p.putBytes("meters",meters,bytes)==bytes) && ok;
  p.end();
  return ok;
}
bool StorageManager::appendHistory(const MeterPacket& packet, time_t timestamp) {
  if (!ready_ || timestamp<1760000000) return false;
  File f=LittleFS.open(HISTORY_PATH,FILE_APPEND);
  if (!f) return false;
  bool ok=true;
  ok &= f.print((long)timestamp)>0; ok &= f.print(';')>0;
  ok &= f.print(packet.serial)>0; ok &= f.print(';')>0;
  ok &= f.print(packet.type)>0; ok &= f.print(';')>0;
  ok &= f.print(packet.model)>0; ok &= f.print(';')>0;
  ok &= f.print(packet.mac)>0; ok &= f.print(';')>0;
  ok &= f.print(packet.version)>0;
  for (uint8_t i=0;i<packet.fieldCount;i++) {
    ok &= f.print(';')>0; ok &= f.print(packet.fields[i].key)>0; ok &= f.print('=')>0;
    ok &= f.print(packet.fields[i].value,4)>0; ok &= f.print(' ')>0;
    ok &= f.print(packet.fields[i].unit)>0;
  }
  ok &= f.println()>0;
  f.close();
  return ok;
}
void StorageManager::pruneHistory(time_t now, uint16_t retentionDays) {
  if (!ready_ || now<1760000000 || !LittleFS.exists(HISTORY_PATH)) return;
  const time_t cutoff=now-(time_t)retentionDays*86400;
  File in=LittleFS.open(HISTORY_PATH,FILE_READ);
  if (!in) return;
  File out=LittleFS.open("/history.tmp",FILE_WRITE);
  if (!out) { in.close(); return; }
  while (in.available()) {
    String line=in.readStringUntil('\n');
    const int sep=line.indexOf(';');
    const long epoch=sep>0?line.substring(0,sep).toInt():0;
    if (epoch>=cutoff) out.println(line);
  }
  in.close(); out.close();
  LittleFS.remove(HISTORY_PATH);
  LittleFS.rename("/history.tmp",HISTORY_PATH);
}
uint64_t StorageManager::filesystemTotal() const { return ready_ ? LittleFS.totalBytes() : 0; }
uint64_t StorageManager::filesystemUsed() const { return ready_ ? LittleFS.usedBytes() : 0; }
