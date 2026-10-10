#pragma once
#include <Arduino.h>

enum class ButtonEvent : uint8_t { None, UpShort, DownShort, BothShort, UpLong, DownLong, BothLong, ApRecovery };

class ButtonManager {
public:
  void begin(int upPin, int downPin, bool upPullup, bool downPullup);
  ButtonEvent update(uint32_t now);
private:
  int upPin_=-1, downPin_=-1;
  bool upPullup_=true, downPullup_=true;
  bool upRaw_=false, downRaw_=false, upStable_=false, downStable_=false;
  bool prevUp_=false, prevDown_=false;
  uint32_t upChangedAt_=0, downChangedAt_=0, pressAt_=0, startupAt_=0;
  bool longSent_=false, recoverySent_=false;
};
