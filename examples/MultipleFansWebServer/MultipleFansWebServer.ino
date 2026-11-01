/*
 * MultipleFansWebServer — 2.0
 * Companion modules are unused; the sketch is self-contained.
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <SmartMiFanAsync.h>
#include "config.h"

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

WiFiUDP fanUdp;
WebServer server(80);

void handleStatus() {
  String body = "{\"used\":" + String(SmartMiFan::used()) +
                ",\"participating\":" + String(SmartMiFan::participating()) + "}";
  server.send(200, "application/json", body);
}

void handleSpeed() {
  const CmdAccept acc = SmartMiFan::setSpeedAll(static_cast<uint8_t>(server.arg("pct").toInt()));
  server.send(acc.result == CmdResult::REJECTED ? 409 : 200, "text/plain", String(acc.seq));
}

void handlePower() {
  const CmdAccept acc = SmartMiFan::setPowerAll(server.arg("on") == "1");
  server.send(acc.result == CmdResult::REJECTED ? 409 : 200, "text/plain", String(acc.seq));
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(250);

  SmartMiFan::begin(fanUdp);
  for (uint8_t i = 0; i < FAN_COUNT && i < SMARTMIFAN_MAX_FANS; ++i) {
    SmartMiFan::add(i, fans[i].ip, fans[i].token, fans[i].model);
  }
  SmartMiFan::connect(5000);

  server.on("/status", handleStatus);
  server.on("/speed", handleSpeed);
  server.on("/power", handlePower);
  server.begin();
}

void loop() {
  SmartMiFan::update();
  server.handleClient();
}
