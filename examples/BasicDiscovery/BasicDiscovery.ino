#include <WiFi.h>
#include <WiFiUdp.h>
#include <SmartMiFanAsync.h>

#ifndef WIFI_SSID
#define WIFI_SSID "your-ssid"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "your-pass"
#endif
#ifndef FAN_TOKEN
#define FAN_TOKEN "00112233445566778899aabbccddeeff"
#endif

WiFiUDP udp;

void onEvent(const EventInfo& ev) {
  if (ev.type == EventType::DISCOVERY_DONE) {
    Serial.printf("discovery done ok=0x%02X fail=0x%02X\n", ev.okMask, ev.failMask);
  }
  if (ev.type == EventType::FAN_READY) {
    SmartMiFanInfo info;
    if (SmartMiFan::info(static_cast<uint8_t>(ev.fanIndex), info)) {
      Serial.printf("ready slot %d ip=%s model=%s\n", ev.fanIndex, info.ip.toString().c_str(), info.model);
    }
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(200);
  SmartMiFan::begin(udp);
  SmartMiFan::setEventCallback(onEvent);
  SmartMiFan::add(0, "0.0.0.0", FAN_TOKEN);
  SmartMiFan::discover(static_cast<FanMask>(1u), 3000);
}

void loop() {
  SmartMiFan::update();
}
