#include "protocol_parser.h"
#include <cstring>
#include <cstdio>

static uint16_t u16le(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t u32le(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static uint32_t serialFromAddress(const char* mac, bool& ok, uint8_t& prefix, uint8_t& model, uint8_t& type) {
  ok=false; prefix=model=type=0;
  if (!mac || strlen(mac)<17) return 0;
  unsigned a=0,b=0,c=0,d=0,e=0,f=0;
  if (sscanf(mac,"%x:%x:%x:%x:%x:%x",&a,&b,&c,&d,&e,&f)!=6) return 0;
  prefix=a; model=b; type=c;
  ok=(prefix==0xB0 || prefix==0xC0);
  return (d<<16)|(e<<8)|f;
}
static bool knownModel(uint8_t type, uint8_t model) {
  switch(type) {
    case 1: return (model>=1 && model<=5) || (model>=16 && model<=20) || (model>=32 && model<=36) ||
                   (model>=48 && model<=52) || (model>=64 && model<=68) || (model>=80 && model<=84);
    case 2: return model>=1 && model<=18;
    case 3: return model==1 || model==4 || model==5 || model==7;
    case 4: return model==1;
    case 7: return model==2;
    default: return false;
  }
}
static bool modelHasTemperature(uint8_t type, uint8_t model) {
  if (type==2) {
    if (model==1 || model==2 || (model>=3 && model<=6) || (model>=9 && model<=12) || (model>=15 && model<=18)) return true;
  }
  return type==4 && model==1;
}
static bool temperatureForFirmware(uint8_t type, uint8_t model, uint8_t fw) {
  bool valid=modelHasTemperature(type,model);
  if (type==2 && model==1 && (fw==14 || fw==16 || fw==17 || fw==18)) valid=false;
  if (type==2 && model==2 && (fw==14 || fw==16 || fw==17 || fw==18 || fw==19)) valid=false;
  return valid;
}
static uint8_t batteryPercent(uint8_t raw, uint8_t boundary) {
  if (raw<1 || raw>boundary) return 1;
  return raw>100 ? 100 : raw;
}
static void addField(MeterPacket& out, const char* key, float value, const char* unit) {
  if (out.fieldCount>=36) return;
  MeasurementField& f=out.fields[out.fieldCount++];
  snprintf(f.key,sizeof(f.key),"%s",key);
  snprintf(f.unit,sizeof(f.unit),"%s",unit);
  f.value=value;
}
static void addEnergyPair(MeterPacket& out, const char* keyA, const char* keyB, uint32_t a, uint32_t b, const char* unit) {
  addField(out,keyA,a/1000.0f,unit); addField(out,keyB,b/1000.0f,unit);
  out.reading=a/1000.0f; out.reading2=b/1000.0f;
  out.hasReading=out.hasReading2=true; out.unit=unit;
}
bool parseElehantPacket(const uint8_t* p, size_t n, const char* mac, int rssi, MeterPacket& out) {
  if (!p || n!=17 || !(p[0]&0x80)) return false;
  bool addrOk=false; uint8_t prefix=0, macModel=0, macType=0;
  const uint32_t macSn=serialFromAddress(mac,addrOk,prefix,macModel,macType);
  if (!addrOk) return false;
  const uint8_t type=p[4], model=p[5], version=p[3];
  const uint32_t serial=(uint32_t)p[6] | ((uint32_t)p[7]<<8) | ((uint32_t)p[8]<<16);
  if (!serial || serial!=macSn || type!=macType || model!=macModel || !knownModel(type,model)) return false;
  out=MeterPacket{};
  out.type=type; out.model=model; out.version=version; out.serial=serial;
  out.mac=String(mac); out.rssi=rssi;

  // WiFi box is recognized for diagnostics, but deliberately has no meter reading.
  if (type==7) {
    if (version!=1) return false;
    out.hasBattery=true;
    out.batteryPercent=batteryPercent(p[9],171);
    addField(out,"gateway_firmware",p[10]/10.0f,"version");
    addField(out,"wifi_status",p[11],"code");
    addField(out,"meter_count",p[12],"count");
    addField(out,"wifi_signal",p[14],"%");
    addField(out,"gateway_state",p[15],"state");
    return true;
  }

  if (type==3) {
    const uint32_t a=u32le(p+9), b=u32le(p+13);
    char keyA[28],keyB[28];
    if (version==0) addEnergyPair(out,"energy_import","energy_export",a,b,"kWh");
    else if (version>=1 && version<=4) {
      snprintf(keyA,sizeof(keyA),"energy_import_t%u",version);
      snprintf(keyB,sizeof(keyB),"energy_export_t%u",version);
      addEnergyPair(out,keyA,keyB,a,b,"kWh");
    } else if (version==16) addEnergyPair(out,"reactive_import","reactive_export",a,b,"kvarh");
    else if (version>=17 && version<=20) {
      snprintf(keyA,sizeof(keyA),"reactive_import_t%u",version-16);
      snprintf(keyB,sizeof(keyB),"reactive_export_t%u",version-16);
      addEnergyPair(out,keyA,keyB,a,b,"kvarh");
    } else if (version==32) {
      addField(out,"voltage",u16le(p+9)/10.0f,"V");
      addField(out,"current",u16le(p+11)/100.0f,"A");
      addField(out,"neutral_current",u16le(p+13)/100.0f,"A");
      addField(out,"differential_current",u16le(p+15)/100.0f,"A");
    } else if (version==33) {
      addField(out,"voltage_a",u16le(p+9)/10.0f,"V"); addField(out,"voltage_b",u16le(p+11)/10.0f,"V"); addField(out,"voltage_c",u16le(p+13)/10.0f,"V");
    } else if (version==34) {
      addField(out,"voltage_ab",u16le(p+9)/10.0f,"V"); addField(out,"voltage_bc",u16le(p+11)/10.0f,"V"); addField(out,"voltage_ca",u16le(p+13)/10.0f,"V");
    } else if (version==35) {
      addField(out,"current_a",u16le(p+9)/100.0f,"A"); addField(out,"current_b",u16le(p+11)/100.0f,"A"); addField(out,"current_c",u16le(p+13)/100.0f,"A"); addField(out,"neutral_current",u16le(p+15)/100.0f,"A");
    } else if (version==48) {
      addField(out,"apparent_power",a/100.0f,"VA"); addField(out,"frequency",u16le(p+13)/100.0f,"Hz"); addField(out,"power_factor",p[15]/100.0f,"cos");
    } else if (version==49) {
      addField(out,"active_power",a/100.0f,"W"); addField(out,"reactive_power",b/100.0f,"var");
    } else return false;
    return out.fieldCount>0;
  }

  if (version==1) {
    uint32_t raw=u32le(p+9);
    if (p[0]&0x04) raw += (p[2]&0x0F)/10.0f;
    out.reading=raw/10000.0f; out.hasReading=true;
    out.unit=(type==4?"GJ":"m3");
    if (type==2 && (model==3 || model==4 || model==5 || model==6 || (model>=9 && model<=12) || (model>=15 && model<=18))) {
      const uint8_t tariff=(model==4 || model==6 || model==10 || model==12 || model==16 || model==18)?1:2;
      char key[28];
      snprintf(key,sizeof(key),"volume_t%u",tariff);
      addField(out,key,out.reading,"m3");
    } else {
      addField(out,type==4?"energy":"volume",out.reading,out.unit.c_str());
    }
    out.batteryPercent=batteryPercent(p[13],171); out.hasBattery=(type!=3);
    if (type!=3) addField(out,"battery",out.batteryPercent,"%");
    const int16_t rawTemp=(int16_t)u16le(p+14);
    out.temperatureC=rawTemp/100.0f;
    out.temperatureValid=temperatureForFirmware(type,model,p[16]);
    if (out.temperatureValid) addField(out,"temperature",out.temperatureC,"C");
    return true;
  }
  if (version==5 && type==2) {
    const uint32_t own=((uint32_t)(p[9]>>4)<<24)|((uint32_t)p[10]<<16)|((uint32_t)p[11]<<8)|p[12];
    const uint32_t paired=((uint32_t)(p[9]&0x0F)<<24)|((uint32_t)p[13]<<16)|((uint32_t)p[14]<<8)|p[15];
    const uint8_t ownTariff=(model==4 || model==6 || model==10 || model==12 || model==16 || model==18) ? 1 : 2;
    const uint8_t pairedTariff=ownTariff==1?2:1;
    out.reading=own/1000.0f; out.reading2=paired/1000.0f;
    out.hasReading=out.hasReading2=true; out.unit="m3";
    char ownKey[28],pairedKey[28];
    snprintf(ownKey,sizeof(ownKey),"volume_t%u",ownTariff);
    snprintf(pairedKey,sizeof(pairedKey),"volume_t%u",pairedTariff);
    addField(out,ownKey,out.reading,"m3"); addField(out,pairedKey,out.reading2,"m3");
    out.batteryPercent=batteryPercent(p[1],127); out.hasBattery=true;
    addField(out,"battery",out.batteryPercent,"%");
    out.temperatureC=p[2]; out.temperatureValid=temperatureForFirmware(type,model,p[16]);
    if (out.temperatureValid) addField(out,"temperature",out.temperatureC,"C");
    return true;
  }
  if (version==9) {
    out.hasHeatCarrier=true;
    out.heatCarrierVolume=u32le(p+9)/10000.0f;
    out.inletTemperatureC=u16le(p+13)/100.0f;
    out.outletTemperatureC=u16le(p+15)/100.0f;
    out.heatTemperaturesValid=true;
    addField(out,"coolant_volume",out.heatCarrierVolume,"m3");
    addField(out,"temperature_inlet",out.inletTemperatureC,"C");
    addField(out,"temperature_outlet",out.outletTemperatureC,"C");
    return true;
  }
  return false;
}
