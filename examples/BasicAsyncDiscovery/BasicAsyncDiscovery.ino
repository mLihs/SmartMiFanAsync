/*
 * BasicAsyncDiscovery — 2.0
 * Discover fans by token. Slot index is chosen here (0..n-1).
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <SmartMiFanAsync.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

const char *TOKENS[] = {
  "44444444444444444444444444444444"
};
const size_t TOKEN_COUNT = sizeof(TOKENS) / sizeof(TOKENS[0]);

WiFiUDP fanUdp;

void onEvent(const EventInfo& ev) {
  if (ev.type == EventType::FAN_READY) {
    SmartMiFanInfo info;
    if (SmartMiFan::info(static_cast<uint8_t>(ev.fanIndex), info)) {
      Serial.printf("READY slot %d ip=%s model=%s\n",
                    ev.fanIndex, info.ip.toString().c_str(), info.model);
    }
  }
  if (ev.type == EventType::DISCOVERY_DONE) {
    Serial.printf("DISCOVERY_DONE ok=0x%02X fail=0x%02X restarted=%d\n",
                  ev.okMask, ev.failMask, ev.restarted);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(250);

  SmartMiFan::begin(fanUdp);
  SmartMiFan::setEventCallback(onEvent);
  FanMask mask = 0;
  for (uint8_t i = 0; i < TOKEN_COUNT && i < SMARTMIFAN_MAX_FANS; ++i) {
    SmartMiFan::add(i, "0.0.0.0", TOKENS[i]);
    mask = static_cast<FanMask>(mask | (1u << i));
  }
  SmartMiFan::discover(mask, 5000);
}

void loop() {
  SmartMiFan::update();
}
