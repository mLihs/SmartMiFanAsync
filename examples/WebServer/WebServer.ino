#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <SmartMiFanAsync.h>

#ifndef WIFI_SSID
#define WIFI_SSID "your-ssid"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "your-pass"
#endif

WiFiUDP udp;
WebServer server(80);

void handleStatus() {
  String body = "{\"used\":" + String(SmartMiFan::used()) +
                ",\"participating\":" + String(SmartMiFan::participating()) + "}";
  server.send(200, "application/json", body);
}

void handleSpeed() {
  const uint8_t pct = static_cast<uint8_t>(server.arg("pct").toInt());
  const CmdAccept acc = SmartMiFan::setSpeedAll(pct);
  server.send(acc.result == CmdResult::REJECTED ? 409 : 200, "text/plain",
              String(acc.seq));
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(200);
  SmartMiFan::begin(udp);
  SmartMiFan::connect();
  server.on("/status", handleStatus);
  server.on("/speed", handleSpeed);
  server.begin();
}

void loop() {
  SmartMiFan::update();
  server.handleClient();
}
