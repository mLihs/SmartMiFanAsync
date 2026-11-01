#pragma once

#include <SmartMiFanAsync.h>

struct FanEntry {
  const char *ip;
  const char *token;
  const char *model;
};

extern FanEntry fans[];
extern const uint8_t FAN_COUNT;
