/*
 * AsyncQueryDevice — 2.0
 * add() a known IP+token, then connect() (hello + miIO.info).
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <SmartMiFanAsync.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char *FAN_IP = "192.168.1.100";
const char *TOKEN_HEX = "11111111111111111111111111111111";

WiFiUDP fanUdp;

void onEvent(const EventInfo& ev) {
  if (ev.type == EventType::FAN_INFO || ev.type == EventType::FAN_READY) {
    SmartMiFanInfo info;
    if (SmartMiFan::info(0, info)) {
      Serial.printf("slot0 ready=%d did=%u model=%s fw=%s\n",
                    info.ready, info.did, info.model, info.fw_ver);
    }
  }
  if (ev.type == EventType::CONNECT_DONE) Serial.println("CONNECT_DONE");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(250);

  SmartMiFan::begin(fanUdp);
  SmartMiFan::setEventCallback(onEvent);
  SmartMiFan::add(0, FAN_IP, TOKEN_HEX);
  SmartMiFan::connect(3000);
}

void loop() {
  SmartMiFan::update();
}
