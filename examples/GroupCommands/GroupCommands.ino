#include <WiFi.h>
#include <WiFiUdp.h>
#include <SmartMiFanAsync.h>

#ifndef WIFI_SSID
#define WIFI_SSID "your-ssid"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "your-pass"
#endif

WiFiUDP udp;
uint8_t waitSeq = 0;

void onEvent(const EventInfo& ev) {
  if (ev.type == EventType::GROUP_DONE && ev.seq == waitSeq) {
    Serial.printf("group %u allOk=%d anyOk=%d\n", ev.seq, ev.allOk, ev.anyOk);
    waitSeq = 0;
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(200);
  SmartMiFan::begin(udp);
  SmartMiFan::setEventCallback(onEvent);
  SmartMiFan::connect(3000);
}

void loop() {
  SmartMiFan::update();
  static uint32_t last;
  if (waitSeq == 0 && millis() - last > 5000 && !SmartMiFan::connecting()) {
    last = millis();
    const CmdAccept acc = SmartMiFan::setSpeedAll(40);
    waitSeq = acc.seq;
  }
}
