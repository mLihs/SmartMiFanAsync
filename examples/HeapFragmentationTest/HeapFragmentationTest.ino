/**
 * HeapFragmentationTest — 2.0
 * Same heap tick format as 1.x, commands via SmartMiFan::update() / connect().
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
  {"192.168.1.104", "33333333333333333333333333333333", "xiaomi.fan.p76"},
  {"192.168.1.106", "22222222222222222222222222222222", nullptr}
};
const uint8_t FAN_COUNT = sizeof(fans) / sizeof(fans[0]);

WiFiUDP fanUdp;
const uint32_t HEAP_TICK_INTERVAL_MS = 30000;
uint32_t baselineFree = 0;
uint32_t afterWifiFree = 0;
uint32_t afterBeginFree = 0;
uint32_t afterConnectFree = 0;
bool connectDone = false;
bool fansOn = false;
uint8_t waitSeq = 0;

void formatUptime(uint32_t uptimeMs, char* buffer, size_t bufferSize) {
  uint32_t totalSeconds = uptimeMs / 1000;
  snprintf(buffer, bufferSize, "%02lu:%02lu:%02lu",
           totalSeconds / 3600,
           (totalSeconds % 3600) / 60,
           totalSeconds % 60);
}

float calcFragmentation(uint32_t largest, uint32_t free) {
  if (free == 0) return 0.0f;
  return (1.0f - ((float)largest / (float)free)) * 100.0f;
}

void printHeapTick(const char* phase) {
  char uptime[16];
  formatUptime(millis(), uptime, sizeof(uptime));
  const uint32_t free = ESP.getFreeHeap();
  const uint32_t largest = ESP.getMaxAllocHeap();
  const int32_t drift = baselineFree > 0 ? (int32_t)(baselineFree - free) : 0;
  Serial.printf("[Heap] TICK %s: free=%u, largest=%u, frag=%.1f%%, drift=%d, minFree=%u [%s]\n",
                uptime, free, largest, calcFragmentation(largest, free), drift,
                ESP.getMinFreeHeap(), phase);
}

void onEvent(const EventInfo& ev) {
  if (ev.type == EventType::CONNECT_DONE) {
    connectDone = true;
    afterConnectFree = ESP.getFreeHeap();
    Serial.printf("[Heap] AFTER_CONNECT: free=%u cost=%d\n",
                  afterConnectFree, (int32_t)(afterBeginFree - afterConnectFree));
  }
  if (ev.type == EventType::GROUP_DONE && ev.seq == waitSeq) waitSeq = 0;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  baselineFree = ESP.getFreeHeap();
  Serial.printf("[Heap] START: free=%u largest=%u\n", baselineFree, ESP.getMaxAllocHeap());

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) delay(250);
  afterWifiFree = ESP.getFreeHeap();
  Serial.printf("[Heap] AFTER_WIFI: free=%u cost=%d\n",
                afterWifiFree, (int32_t)(baselineFree - afterWifiFree));

  SmartMiFan::begin(fanUdp);
  SmartMiFan::setEventCallback(onEvent);
  afterBeginFree = ESP.getFreeHeap();
  Serial.printf("[Heap] AFTER_BEGIN: free=%u cost=%d\n",
                afterBeginFree, (int32_t)(afterWifiFree - afterBeginFree));

  for (uint8_t i = 0; i < FAN_COUNT && i < SMARTMIFAN_MAX_FANS; ++i) {
    SmartMiFan::add(i, fans[i].ip, fans[i].token, fans[i].model);
  }
  SmartMiFan::connect(5000);
  printHeapTick("Connect");
}

void loop() {
  SmartMiFan::update();

  static uint32_t lastControl = 0;
  if (connectDone && waitSeq == 0 && millis() - lastControl > 10000) {
    lastControl = millis();
    fansOn = !fansOn;
    const CmdAccept acc = SmartMiFan::setPowerAll(fansOn);
    waitSeq = acc.seq;
    if (fansOn) SmartMiFan::setSpeedAll(static_cast<uint8_t>(20 + (esp_random() % 61)));
  }

  static uint32_t lastTick = 0;
  if (millis() - lastTick >= HEAP_TICK_INTERVAL_MS) {
    lastTick = millis();
    printHeapTick(connectDone ? "Operating" : "Connect");
  }
}
