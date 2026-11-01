/*
 * FullOrchestrationFlow — 2.0
 * connect → handshakeAll → power/speed groups → disable one slot.
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
enum class Step : uint8_t { CONNECT, HANDSHAKE, POWER, SPEED, TOGGLE, DONE };
Step step = Step::CONNECT;
uint8_t waitSeq = 0;

void onEvent(const EventInfo& ev) {
  if (ev.type == EventType::CONNECT_DONE) {
    Serial.println("CONNECT_DONE");
    step = Step::HANDSHAKE;
  }
  if (ev.type == EventType::GROUP_DONE && ev.seq == waitSeq) {
    Serial.printf("GROUP_DONE seq=%u allOk=%d\n", ev.seq, ev.allOk);
    waitSeq = 0;
    if (step == Step::HANDSHAKE) step = Step::POWER;
    else if (step == Step::POWER) step = Step::SPEED;
    else if (step == Step::SPEED) step = Step::TOGGLE;
  }
  if (ev.type == EventType::FAN_ERROR) {
    Serial.printf("FAN_ERROR slot=%d err=%u\n", ev.fanIndex, static_cast<unsigned>(ev.error));
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
  if (waitSeq != 0) return;
  switch (step) {
    case Step::HANDSHAKE: {
      const CmdAccept acc = SmartMiFan::handshakeAll();
      waitSeq = acc.seq;
      break;
    }
    case Step::POWER: {
      const CmdAccept acc = SmartMiFan::setPowerAll(true);
      waitSeq = acc.seq;
      break;
    }
    case Step::SPEED: {
      const CmdAccept acc = SmartMiFan::setSpeedAll(50);
      waitSeq = acc.seq;
      break;
    }
    case Step::TOGGLE:
      if (FAN_COUNT > 1) SmartMiFan::setEnabled(1, false);
      Serial.printf("participating=0x%02X\n", SmartMiFan::participating());
      step = Step::DONE;
      break;
    default:
      break;
  }
}
