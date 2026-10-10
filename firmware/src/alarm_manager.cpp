#include "alarm_manager.h"
#include <Preferences.h>

static AlarmSettings currentSettings;

void alarmSettingsLoad() {
  Preferences p;
  if (!p.begin("svt15alarm", true)) return;
  currentSettings.batteryLow=(uint8_t)constrain(p.getUInt("battery",20),0,100);
  currentSettings.staleMinutes=(uint16_t)constrain(p.getUInt("stale",15),1,1440);
  currentSettings.lowEnabled=p.getBool("lowOn",false);
  currentSettings.highEnabled=p.getBool("highOn",false);
  currentSettings.lowValue=p.getFloat("low",0.0f);
  currentSettings.highValue=p.getFloat("high",0.0f);
  p.end();
}

AlarmSettings alarmSettingsGet() { return currentSettings; }

bool alarmSettingsSave(const AlarmSettings& s) {
  if (s.batteryLow>100 || s.staleMinutes<1 || s.staleMinutes>1440 ||
      (s.lowEnabled && s.highEnabled && s.lowValue>=s.highValue)) return false;
  Preferences p;
  if (!p.begin("svt15alarm",false)) return false;
  bool ok=true;
  ok=(p.putUInt("battery",s.batteryLow)>0) && ok;
  ok=(p.putUInt("stale",s.staleMinutes)>0) && ok;
  ok=(p.putBool("lowOn",s.lowEnabled)>0) && ok;
  ok=(p.putBool("highOn",s.highEnabled)>0) && ok;
  ok=(p.putFloat("low",s.lowValue)>0) && ok;
  ok=(p.putFloat("high",s.highValue)>0) && ok;
  p.end();
  if (ok) currentSettings=s;
  return ok;
}

bool alarmAnyActive(const MeterManager& meters, uint32_t nowMillis, const AlarmSettings& s) {
  const uint32_t staleMs=(uint32_t)s.staleMinutes*60000UL;
  for (uint8_t i=0;i<meters.count();++i) {
    const MeterConfig* cfg=meters.configAt(i);
    if (!cfg || !(cfg->flags&METER_ACTIVE) || !(cfg->flags&METER_ALARMS)) continue;
    const MeterPacket* p=meters.latestAt(i);
    const uint32_t last=meters.lastSeenMillisAt(i);
    if ((!p && nowMillis>=staleMs) || (p && nowMillis-last>=staleMs)) return true;
    if (!p) continue;
    if (p->hasBattery && p->batteryPercent<=s.batteryLow) return true;
    if (p->hasReading && ((s.lowEnabled && p->reading<s.lowValue) ||
                         (s.highEnabled && p->reading>s.highValue))) return true;
  }
  return false;
}
