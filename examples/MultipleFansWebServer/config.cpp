#include "config.h"

FanEntry fans[] = {
  {"192.168.1.104", "33333333333333333333333333333333", nullptr},
  {"192.168.1.106", "22222222222222222222222222222222", nullptr}
};

const uint8_t FAN_COUNT = sizeof(fans) / sizeof(fans[0]);
