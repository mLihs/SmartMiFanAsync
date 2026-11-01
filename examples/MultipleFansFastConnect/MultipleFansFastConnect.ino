/*
 * MultipleFansFastConnect — 2.0
 * Known IPs go through connect(); 0.0.0.0 slots fall through to discovery.
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <SmartMiFanAsync.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

struct FanEntry {
  const char *ip;
  const char *token;
  const char *model;
};

FanEntry fans[] = {
  {"192.168.1.104", "33333333333333333333333333333333", "dmaker.fan.p33"},
  {"192.168.1.106", "22222222222222222222222222222222", nullptr}
};
const uint8_t FAN_COUNT = sizeof(fans) / sizeof(fans[0]);

WiFiUDP fanUdp;
bool connected = false;
uint8_t waitSeq = 0;

void onEvent(const EventInfo& ev) {
  if (ev.type == EventType::CONNECT_DONE) {
    connected = true;
    Serial.println("CONNECT_DONE");
    for (uint8_t i = 0; i < FAN_COUNT; ++i) {
      SmartMiFanInfo info;
      if (SmartMiFan::info(i, info)) {
        Serial.printf("  [%u] ready=%d ip=%s model=%s\n",
                      i, info.ready, info.ip.toString().c_str(), info.model);
      }
    }
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
    SmartMiFan::add(i, fans[i].ip, fans[i].token, fans[i].model);
  }
  SmartMiFan::connect(5000);
}

void loop() {
  SmartMiFan::update();
  static uint32_t last;
  if (!connected || waitSeq != 0 || millis() - last < 10000) return;
  last = millis();
  const CmdAccept acc = SmartMiFan::setSpeedAll(35);
  waitSeq = acc.seq;
}
