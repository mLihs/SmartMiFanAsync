/*
 * WebServerControl — 2.0
 * WebServer + JSON status. Group commands are non-blocking.
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SmartMiFanAsync.h>

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

struct FanEntry {
  const char *ip;
  const char *token;
};

FanEntry fans[] = {
  {"192.168.1.100", "11111111111111111111111111111111"},
  {"192.168.1.101", "22222222222222222222222222222222"}
};
const uint8_t FAN_COUNT = sizeof(fans) / sizeof(fans[0]);

WiFiUDP fanUdp;
WebServer server(80);

void handleStatus() {
  String body = "{\"used\":" + String(SmartMiFan::used()) +
                ",\"participating\":" + String(SmartMiFan::participating()) +
                ",\"connecting\":" + String(SmartMiFan::connecting() ? "true" : "false") +
                ",\"fans\":[";
  bool first = true;
  for (uint8_t i = 0; i < SMARTMIFAN_MAX_FANS; ++i) {
    SmartMiFanInfo info;
    if (!SmartMiFan::info(i, info)) continue;
    if (!first) body += ",";
    first = false;
    body += "{\"i\":" + String(i) +
            ",\"token\":\"" + String(info.token) +
            "\",\"ready\":" + String(info.ready ? "true" : "false") +
            ",\"enabled\":" + String(info.userEnabled ? "true" : "false") +
            ",\"ip\":\"" + info.ip.toString() + "\"}";
  }
  body += "]}";
  server.send(200, "application/json", body);
}

void handlePower() {
  const bool on = server.arg("on") == "1";
  const CmdAccept acc = SmartMiFan::setPowerAll(on);
  server.send(acc.result == CmdResult::REJECTED ? 409 : 200, "text/plain", String(acc.seq));
}

void handleSpeed() {
  const uint8_t pct = static_cast<uint8_t>(server.arg("pct").toInt());
  const CmdAccept acc = SmartMiFan::setSpeedAll(pct);
  server.send(acc.result == CmdResult::REJECTED ? 409 : 200, "text/plain", String(acc.seq));
}

void handleEnable() {
  const uint8_t i = static_cast<uint8_t>(server.arg("i").toInt());
  SmartMiFan::setEnabled(i, server.arg("on") == "1");
  server.send(200, "text/plain", "ok");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(250);
  MDNS.begin("smartmifan");

  SmartMiFan::begin(fanUdp);
  for (uint8_t i = 0; i < FAN_COUNT && i < SMARTMIFAN_MAX_FANS; ++i) {
    SmartMiFan::add(i, fans[i].ip, fans[i].token);
  }
  SmartMiFan::connect(5000);

  server.on("/status", handleStatus);
  server.on("/power", handlePower);
  server.on("/speed", handleSpeed);
  server.on("/enable", handleEnable);
  server.begin();
}

void loop() {
  SmartMiFan::update();
  server.handleClient();
}
