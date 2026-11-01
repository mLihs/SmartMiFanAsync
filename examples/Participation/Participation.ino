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

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(200);
  SmartMiFan::begin(udp);
  SmartMiFan::connect();
}

void loop() {
  SmartMiFan::update();
  static uint32_t last;
  if (millis() - last < 2000) return;
  last = millis();
  Serial.printf("participating=0x%02X used=0x%02X\n",
                SmartMiFan::participating(), SmartMiFan::used());
  for (uint8_t i = 0; i < SMARTMIFAN_MAX_FANS; ++i) {
    SmartMiFanInfo info;
    if (!SmartMiFan::info(i, info)) continue;
    Serial.printf("  [%u] enabled=%d ready=%d part=%d\n",
                  i, info.userEnabled, info.ready, static_cast<int>(info.participation));
  }
}
