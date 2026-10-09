#include "protocol_parser.h"
#include <math.h>
#include <cstring>
#include <cstdio>

static uint16_t u16le(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t u32le(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
static uint32_t snFromMac(const char* mac, bool& ok, uint8_t& prefix, uint8_t& model, uint8_t& type) {
  ok=false; prefix=model=type=0;
  if (!mac || strlen(mac)<17) return 0;
  unsigned a=0,b=0,c=0,d=0,e=0,f=0;
  if (sscanf(mac,"%x:%x:%x:%x:%x:%x",&a,&b,&c,&d,&e,&f)!=6) return 0;
  prefix=a; model=b; type=c;
  ok=(prefix==0xB0 || prefix==0xC0);
  return (d<<16)|(e<<8)|f; // BLE address serial is big endian.
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
static bool tempModel(uint8_t type, uint8_t model, uint8_t fw) {
  if (type==1 || type==3 || type==7) return false;
  if (type==4) return model==1;
  if (type==2) {
    if (model==1 && (fw==14 || fw==16 || fw==17 || fw==18)) return false;
    if (model==2 && (fw==14 || fw==16 || fw==17 || fw==18 || fw==19)) return false;
    if (model==7 || model==8 || model==13 || model==14) return false;
    return model>=1 && model<=6 || model>=9 && model<=12 || model>=15;
  }
  return false;
}
static uint8_t batteryPercent(uint8_t raw, uint8_t boundary) {
  if (raw<1 || raw>boundary) return 1;
  if (raw>100) return 100;
  return raw;
}
bool parseElehantPacket(const uint8_t* p, size_t n, const char* mac, int rssi, MeterPacket& out) {
  if (!p || n!=17 || !(p[0]&0x80)) return false;
  bool addrOk=false; uint8_t prefix=0, macModel=0, macType=0;
  uint32_t macSn=snFromMac(mac,addrOk,prefix,macModel,macType);
  if (!addrOk) return false;
  const uint8_t type=p[4], model=p[5], version=p[3];
  const uint32_t serial=(uint32_t)p[6] | ((uint32_t)p[7]<<8) | ((uint32_t)p[8]<<16);
  if (!serial || serial!=macSn || type!=macType || model!=macModel || !knownModel(type,model)) return false;
  out=MeterPacket{};
  out.type=type; out.model=model; out.version=version; out.serial=serial;
  out.mac=String(mac); out.rssi=rssi; out.unit="";
  if (type==7) return true; // Gateway status only; never a meter reading.
  if (type==3) {
    const uint32_t a=u32le(p+9), b=u32le(p+13);
    if (version==0) { out.hasReading=out.hasReading2=true; out.reading=a/1000.0f; out.reading2=b/1000.0f; out.unit="kWh"; }
    else if (version>=1 && version<=4) { out.hasReading=out.hasReading2=true; out.reading=a/1000.0f; out.reading2=b/1000.0f; out.unit="kWh tariff"; }
    else if (version==16 || (version>=17 && version<=20)) { out.hasReading=out.hasReading2=true; out.reading=a/1000.0f; out.reading2=b/1000.0f; out.unit="kvarh"; }
    else if (version==32) { out.hasReading=true; out.reading=u16le(p+9)/10.0f; out.unit="V"; }
    else if (version==33 || version==34 || version==35) { out.hasReading=true; out.reading=u16le(p+9)/(version==35?100.0f:10.0f); out.unit=(version==35?"A":"V"); }
    else if (version==48) { out.hasReading=true; out.reading=a/100.0f; out.unit="VA"; }
    else if (version==49) { out.hasReading=out.hasReading2=true; out.reading=a/100.0f; out.reading2=b/100.0f; out.unit="W/var"; }
    return out.hasReading;
  }
  if (version==1) {
    uint32_t raw=u32le(p+9);
    if (p[0]&0x04) raw += (p[2]&0x0F)/10.0f;
    out.reading=raw/10000.0f; out.hasReading=true;
    out.unit=(type==4?"GJ":"m3");
    out.batteryPercent=batteryPercent(p[13],171); out.hasBattery=true;
    const int16_t rawTemp=(int16_t)u16le(p+14);
    out.temperatureC=rawTemp/100.0f;
    out.temperatureValid=tempModel(type,model,p[16]);
    return true;
  }
  if (version==5 && type==2) {
    uint32_t first=((uint32_t)(p[9]>>4)<<24)|((uint32_t)p[10]<<16)|((uint32_t)p[11]<<8)|p[12];
    uint32_t second=((uint32_t)(p[9]&0x0F)<<24)|((uint32_t)p[13]<<16)|((uint32_t)p[14]<<8)|p[15];
    // Protocol: first value belongs to packet model, second to its paired model.
    // Models 4/6/10/12/16/18 are tariff 1; paired odd/previous models are tariff 2.
    const bool packetIsTariff1=(model==4 || model==6 || model==10 || model==12 || model==16 || model==18);
    out.reading=(packetIsTariff1?first:second)/1000.0f;
    out.reading2=(packetIsTariff1?second:first)/1000.0f;
    out.hasReading=out.hasReading2=true; out.unit="m3";
    out.batteryPercent=batteryPercent(p[1],127); out.hasBattery=true;
    out.temperatureC=p[2]; out.temperatureValid=tempModel(type,model,p[16]);
    return true;
  }
  // Version 8 is historical data, not a current reading; version 9 has
  // separate heat-carrier fields and will be handled by the richer model later.
  return false;
}
