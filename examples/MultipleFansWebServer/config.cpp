#include "config.h"

// ------ Fast Connect Configuration ------
SmartMiFanFastConnectEntry fastConnectFans[] = {
  {"192.168.1.100", "11111111111111111111111111111111"},
  {"192.168.1.101", "22222222222222222222222222222222"},
  {"192.168.1.102", "33333333333333333333333333333333"},
  {"192.168.1.103", "44444444444444444444444444444444"}
};
const size_t FAST_CONNECT_FAN_COUNT = sizeof(fastConnectFans) / sizeof(fastConnectFans[0]);
// ----------------------------------------


