/*
 * MultipleFansSmartConnect — 2.0
 * connect() is Fast Connect + discovery for TIMEOUT / WRONG_DEVICE slots.
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <SmartMiFanAsync.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

struct FanEntry {
  const char *ip;
  const char *token;
};

FanEntry fans[] = {
  {"192.168.1.104", "33333333333333333333333333333333"},
  {"0.0.0.0", "22222222222222222222222222222222"}
};
const uint8_t FAN_COUNT = sizeof(fans) / sizeof(fans[0]);

WiFiUDP fanUdp;
bool ready = false;
uint8_t waitSeq = 0;
bool fansOn = false;

void onEvent(const EventInfo& ev) {
  if (ev.type == EventType::CONNECT_DONE) {
    ready = true;
    Serial.printf("CONNECT_DONE used=0x%02X\n", SmartMiFan::used());
  }
  if (ev.type == EventType::DISCOVERY_DONE) {
    Serial.printf("DISCOVERY_DONE ok=0x%02X fail=0x%02X\n", ev.okMask, ev.failMask);
  }
  if (ev.type == EventType::GROUP_DONE && ev.seq == waitSeq) waitSeq = 0;
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(250);

  SmartMiFan::begin(fanUdp);
  SmartMiFan::setEventCallback(onEvent);
  for (uint8_t i = 0; i < FAN_COUNT && i < SMARTMIFAN_MAX_FANS; ++i) {
    SmartMiFan::add(i, fans[i].ip, fans[i].token);
  }
  SmartMiFan::connect(5000);
}

void loop() {
  SmartMiFan::update();
  static uint32_t last;
  if (!ready || waitSeq != 0 || millis() - last < 3000) return;
  last = millis();
  fansOn = !fansOn;
  const CmdAccept acc = SmartMiFan::setPowerAll(fansOn);
  waitSeq = acc.seq;
  if (fansOn) SmartMiFan::setSpeedAll(90);
}
