/*
 * MultipleFansAsync — 2.0
 * Discover several tokens, then toggle group power/speed.
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <SmartMiFanAsync.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

const char *TOKENS[] = {
  "22222222222222222222222222222222",
  "33333333333333333333333333333333"
};
const size_t TOKEN_COUNT = sizeof(TOKENS) / sizeof(TOKENS[0]);

WiFiUDP fanUdp;
uint8_t waitSeq = 0;
bool fansOn = false;

void onEvent(const EventInfo& ev) {
  if (ev.type == EventType::DISCOVERY_DONE) {
    Serial.printf("discovery ok=0x%02X fail=0x%02X\n", ev.okMask, ev.failMask);
  }
  if (ev.type == EventType::GROUP_DONE && ev.seq == waitSeq) {
    Serial.printf("GROUP_DONE seq=%u allOk=%d\n", ev.seq, ev.allOk);
    waitSeq = 0;
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
  static uint32_t last;
  if (waitSeq != 0 || SmartMiFan::discovering() || millis() - last < 8000) return;
  last = millis();
  fansOn = !fansOn;
  const CmdAccept acc = SmartMiFan::setPowerAll(fansOn);
  waitSeq = acc.seq;
  if (fansOn) SmartMiFan::setSpeedAll(40);
}
