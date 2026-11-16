// DNS точки доступа: любое имя сайта отвечает адресом платы.
// Вместе с ответами /generate_204 и др. (monitor_web.h) телефон считает сеть рабочей:
// не отключается от точки доступа и открывает страницы через неё, а не через мобильную сеть.
#pragma once
#include <WiFi.h>
#include <DNSServer.h>
#include "monitor_common.h"

class CaptiveDns {
 public:
  void begin() {
    dns_.setErrorReplyCode(DNSReplyCode::NoError);
    dns_.start(53, "*", WiFi.softAPIP());
    on_ = true;
    SVT_SERIAL.println("DNS: all names -> AP address");
  }
  void loop() {
    if (on_) dns_.processNextRequest();
  }

 private:
  DNSServer dns_;
  bool on_ = false;
};
