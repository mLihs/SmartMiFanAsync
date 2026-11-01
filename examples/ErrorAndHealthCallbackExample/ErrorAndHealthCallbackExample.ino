/*
 * ErrorAndHealthCallbackExample — 2.0
 * EventCallback replaces the 1.x error/health callbacks. handshake() is the health check.
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
uint8_t hsSeq = 0;

void onEvent(const EventInfo& ev) {
  switch (ev.type) {
    case EventType::FAN_ERROR:
      Serial.printf("FAN_ERROR slot=%d err=%u\n", ev.fanIndex, static_cast<unsigned>(ev.error));
      break;
    case EventType::FAN_DEVICE_ERROR:
      Serial.printf("FAN_DEVICE_ERROR slot=%d\n", ev.fanIndex);
      break;
    case EventType::FAN_READY:
      Serial.printf("FAN_READY slot=%d\n", ev.fanIndex);
      break;
    case EventType::CMD_DONE:
      Serial.printf("CMD_DONE slot=%d kind=%u ok=%d\n",
                    ev.fanIndex, static_cast<unsigned>(ev.kind), ev.ok);
      if (ev.kind == CmdKind::HANDSHAKE) hsSeq = 0;
      break;
    case EventType::GROUP_DONE:
      if (ev.kind == CmdKind::HANDSHAKE) {
        Serial.printf("handshakeAll seq=%u allOk=%d\n", ev.seq, ev.allOk);
        hsSeq = 0;
      }
      break;
    case EventType::CONNECT_DONE:
      Serial.println("CONNECT_DONE");
      break;
    default:
      break;
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
  if (SmartMiFan::connecting() || hsSeq != 0 || millis() - last < 15000) return;
  last = millis();
  const CmdAccept acc = SmartMiFan::handshakeAll();
  hsSeq = acc.seq;
  Serial.printf("handshakeAll seq=%u\n", hsSeq);
}
