/*
 * FanParticipationToggle — 2.0
 * setEnabled() changes who setPowerAll(Scope::PARTICIPATING) hits.
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
  {"192.168.1.106", "22222222222222222222222222222222"}
};
const uint8_t FAN_COUNT = sizeof(fans) / sizeof(fans[0]);

WiFiUDP fanUdp;
bool connected = false;
bool slot1Enabled = true;

void printParticipation() {
  Serial.printf("participating=0x%02X\n", SmartMiFan::participating());
  for (uint8_t i = 0; i < FAN_COUNT; ++i) {
    SmartMiFanInfo info;
    if (!SmartMiFan::info(i, info)) continue;
    Serial.printf("  [%u] enabled=%d ready=%d part=%d\n",
                  i, info.userEnabled, info.ready, static_cast<int>(info.participation));
  }
}

void onEvent(const EventInfo& ev) {
  if (ev.type == EventType::CONNECT_DONE) {
    connected = true;
    printParticipation();
  }
  if (ev.type == EventType::GROUP_DONE) {
    Serial.printf("GROUP_DONE kind=%u seq=%u ok=0x%02X fail=0x%02X\n",
                  static_cast<unsigned>(ev.kind), ev.seq, ev.okMask, ev.failMask);
  }
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
  if (!connected || millis() - last < 8000) return;
  last = millis();
  slot1Enabled = !slot1Enabled;
  if (FAN_COUNT > 1) SmartMiFan::setEnabled(1, slot1Enabled);
  Serial.printf("slot 1 enabled=%d\n", slot1Enabled);
  printParticipation();
  SmartMiFan::setPowerAll(true, Scope::PARTICIPATING);
  SmartMiFan::setSpeedAll(50, Scope::PARTICIPATING);
}
