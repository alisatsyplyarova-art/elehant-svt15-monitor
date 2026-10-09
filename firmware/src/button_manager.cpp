#include "button_manager.h"
#include "config.h"

void ButtonManager::begin(int upPin, int downPin, bool upPullup, bool downPullup) {
  upPin_=upPin; downPin_=downPin; upPullup_=upPullup; downPullup_=downPullup;
  pinMode(upPin_, upPullup_ ? INPUT_PULLUP : INPUT);
  pinMode(downPin_, downPullup_ ? INPUT_PULLUP : INPUT);
  startupAt_=millis();
  upRaw_=upStable_=(digitalRead(upPin_) == (upPullup_ ? LOW : HIGH));
  downRaw_=downStable_=(digitalRead(downPin_) == (downPullup_ ? LOW : HIGH));
  upChangedAt_=downChangedAt_=startupAt_;
}

ButtonEvent ButtonManager::update(uint32_t now) {
  bool u = digitalRead(upPin_) == (upPullup_ ? LOW : HIGH);
  bool d = digitalRead(downPin_) == (downPullup_ ? LOW : HIGH);
  if (u != upRaw_) { upRaw_=u; upChangedAt_=now; }
  if (d != downRaw_) { downRaw_=d; downChangedAt_=now; }
  if (now-upChangedAt_ >= BUTTON_DEBOUNCE_MS) upStable_=upRaw_;
  if (now-downChangedAt_ >= BUTTON_DEBOUNCE_MS) downStable_=downRaw_;

  // Recovery is only a request. Caller must reset AP credentials only.
  if (!recoverySent_ && now-startupAt_ <= 12000UL && downStable_ && !upStable_ &&
      now-downChangedAt_ >= AP_RECOVERY_HOLD_MS) {
    recoverySent_=true;
    return ButtonEvent::ApRecovery;
  }

  const bool any=upStable_ || downStable_;
  const bool both=upStable_ && downStable_;
  if (any && !(prevUp_ || prevDown_)) { pressAt_=now; longSent_=false; }
  if (any && !longSent_ && now-pressAt_ >= BUTTON_LONG_PRESS_MS) {
    longSent_=true;
    if (both) { prevUp_=upStable_; prevDown_=downStable_; return ButtonEvent::BothLong; }
    if (upStable_) { prevUp_=upStable_; prevDown_=downStable_; return ButtonEvent::UpLong; }
    if (downStable_) { prevUp_=upStable_; prevDown_=downStable_; return ButtonEvent::DownLong; }
  }
  ButtonEvent ev=ButtonEvent::None;
  if ((prevUp_ || prevDown_) && !any && !longSent_) {
    if (prevUp_ && prevDown_) ev=ButtonEvent::BothShort;
    else if (prevUp_) ev=ButtonEvent::UpShort;
    else if (prevDown_) ev=ButtonEvent::DownShort;
  }
  prevUp_=upStable_; prevDown_=downStable_;
  return ev;
}
