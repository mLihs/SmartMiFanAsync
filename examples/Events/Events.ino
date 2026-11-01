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

void onEvent(const EventInfo& ev) {
  Serial.printf("event type=%u fan=%d kind=%u seq=%u ok=%d\n",
                static_cast<unsigned>(ev.type), ev.fanIndex,
                static_cast<unsigned>(ev.kind), ev.seq, ev.ok);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(200);
  SmartMiFan::begin(udp);
  SmartMiFan::setEventCallback(onEvent);
  SmartMiFan::connect();
}

void loop() {
  SmartMiFan::update();
}
