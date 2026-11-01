/*
 * SystemStateOrchestration — 2.0
 * 1.x SystemState is gone. Sleep/wake is prepareForSleep() / wake().
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
  {"192.168.1.104", "33333333333333333333333333333333"}
};
const uint8_t FAN_COUNT = sizeof(fans) / sizeof(fans[0]);

WiFiUDP fanUdp;
enum class Phase : uint8_t { CONNECT, RUN, SLEEP, WAKE };
Phase phase = Phase::CONNECT;
uint32_t phaseAt = 0;

void onEvent(const EventInfo& ev) {
  if (ev.type == EventType::CONNECT_DONE) {
    phase = Phase::RUN;
    phaseAt = millis();
    Serial.println("connected, running 8s");
  }
  if (ev.type == EventType::CANCELLED) Serial.println("CANCELLED");
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
  phaseAt = millis();
}

void loop() {
  SmartMiFan::update();
  const uint32_t now = millis();
  switch (phase) {
    case Phase::RUN:
      if (now - phaseAt > 8000) {
        Serial.println("prepareForSleep(true)");
        while (SmartMiFan::commandPending() && millis() - now < 4000) {
          SmartMiFan::update();
          yield();
        }
        SmartMiFan::prepareForSleep(true);
        phase = Phase::SLEEP;
        phaseAt = millis();
      }
      break;
    case Phase::SLEEP:
      if (now - phaseAt > 5000) {
        Serial.println("wake()");
        SmartMiFan::wake();
        SmartMiFan::handshakeAll();
        phase = Phase::WAKE;
        phaseAt = millis();
      }
      break;
    case Phase::WAKE:
      if (now - phaseAt > 3000) {
        SmartMiFan::setPowerAll(true);
        SmartMiFan::setSpeedAll(40);
        phase = Phase::RUN;
        phaseAt = millis();
      }
      break;
    default:
      break;
  }
}
