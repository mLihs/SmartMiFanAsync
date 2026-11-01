#include <WiFi.h>
#include <WiFiUdp.h>
#include <SmartMiFanAsync.h>

#ifndef WIFI_SSID
#define WIFI_SSID "your-ssid"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "your-pass"
#endif
#ifndef FAN0_IP
#define FAN0_IP "192.168.1.50"
#endif
#ifndef FAN0_TOKEN
#define FAN0_TOKEN "00112233445566778899aabbccddeeff"
#endif

WiFiUDP udp;

void onEvent(const EventInfo& ev) {
  if (ev.type == EventType::CONNECT_DONE) Serial.println("connect done");
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(200);
  SmartMiFan::begin(udp);
  SmartMiFan::setEventCallback(onEvent);
  SmartMiFan::add(0, FAN0_IP, FAN0_TOKEN);
  SmartMiFan::connect(3000);
}

void loop() {
  SmartMiFan::update();
}
