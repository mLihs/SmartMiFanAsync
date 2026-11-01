/*
 * IndependentFansControl — 2.0
 * Per-slot setSpeed/setPower on different intervals.
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
const uint32_t FAN_PAUSE_MS[] = {5000, 8000, 12000, 15000};

WiFiUDP fanUdp;
uint32_t lastMs[SMARTMIFAN_MAX_FANS] = {};
bool on[SMARTMIFAN_MAX_FANS] = {};

void onEvent(const EventInfo& ev) {
  if (ev.type == EventType::CMD_DONE) {
    Serial.printf("CMD_DONE slot=%d kind=%u ok=%d\n",
                  ev.fanIndex, static_cast<unsigned>(ev.kind), ev.ok);
  }
  if (ev.type == EventType::DISCOVERY_DONE) {
    Serial.printf("discovery ok=0x%02X\n", ev.okMask);
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
  if (SmartMiFan::discovering()) return;
  const uint32_t now = millis();
  for (uint8_t i = 0; i < TOKEN_COUNT && i < SMARTMIFAN_MAX_FANS; ++i) {
    if (now - lastMs[i] < FAN_PAUSE_MS[i]) continue;
    lastMs[i] = now;
    on[i] = !on[i];
    SmartMiFan::setPower(i, on[i]);
    if (on[i]) SmartMiFan::setSpeed(i, static_cast<uint8_t>(20 + (esp_random() % 61)));
  }
}
